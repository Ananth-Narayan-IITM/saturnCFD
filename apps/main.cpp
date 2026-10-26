// saturnCFD — driver.
//
// v0.1 driver: validate the parallel substrate end to end (decomposition,
// halo exchange, kernels, I/O) and measure sustained bandwidth. The flow solver
// is layered on top of exactly these primitives.
#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "amr/BoxArray.hpp"
#include "amr/DistributionMapping.hpp"
#include "amr/MultiFab.hpp"
#include "comm/HaloExchange.hpp"
#include "core/Config.hpp"
#include "core/Timer.hpp"
#include "core/Types.hpp"
#include "io/VTKWriter.hpp"
#include "mesh/Geometry.hpp"

using namespace saturn;

namespace {

struct Options {
  int nx = 64, ny = 64, nz = 64;
  double lx = 1.0, ly = 1.0, lz = 1.0;
  int bx = 32, by = 32, bz = 32;
  int nghost = 2;
  bool periodic = true;
  int out_every = 0;
  std::string out_prefix = "output/field";
  int bench_iters = 20;
  bool verify_halo = true;
};

Options readOptions(const Config& cfg) {
  Options o;
  o.nx = cfg.getInt("domain.nx", o.nx, "cells in x at level 0");
  o.ny = cfg.getInt("domain.ny", o.ny, "cells in y at level 0");
  o.nz = cfg.getInt("domain.nz", o.nz, "cells in z (3D only)");
  o.lx = cfg.getDouble("domain.lx", o.lx, "physical length in x");
  o.ly = cfg.getDouble("domain.ly", o.ly, "physical length in y");
  o.lz = cfg.getDouble("domain.lz", o.lz, "physical length in z");
  o.bx = cfg.getInt("grid.block_x", o.bx, "max patch size in x");
  o.by = cfg.getInt("grid.block_y", o.by, "max patch size in y");
  o.bz = cfg.getInt("grid.block_z", o.bz, "max patch size in z");
  o.nghost = cfg.getInt("grid.nghost", o.nghost, "ghost layers per field");
  o.periodic = cfg.getBool("boundary.periodic", o.periodic,
                           "periodic in every direction");
  o.out_every = cfg.getInt("output.every", o.out_every,
                           "write VTK every N steps (0 = only at init)");
  o.out_prefix = cfg.getString("output.prefix", o.out_prefix, "VTK output prefix");
  o.bench_iters = cfg.getInt("bench.iters", o.bench_iters,
                             "stencil iterations for the bandwidth benchmark");
  o.verify_halo = cfg.getBool("bench.verify_halo", o.verify_halo,
                              "compare ghost cells against the analytic field");
  return o;
}

/// Smooth periodic test field: sin(2*pi*x/Lx) * sin(2*pi*y/Ly) [* sin(2*pi*z/Lz)]
KOKKOS_INLINE_FUNCTION
Real analytic(const Geometry& g, int i, int j, int k) {
  const Real pi = 3.14159265358979323846;
  const Real x = g.xc(0, i);
  const Real y = g.xc(1, j);
  Real v = Kokkos::sin(2 * pi * x / g.length(0)) *
           Kokkos::sin(2 * pi * y / g.length(1));
  if (NDIM == 3) v *= Kokkos::sin(2 * pi * g.xc(2, k) / g.length(2));
  return v;
}

/// Max |ghost - analytic| over all ghost cells of all local boxes.
Real ghostError(const MultiFab& mf, const Geometry& geom) {
  Real mx = Real(0);
  Kokkos::Max<Real> err(mx);
  for (int lb = 0; lb < mf.numLocalBoxes(); ++lb) {
    const Box bx = mf.boxArray()[mf.localToGlobal(lb)];
    const Array4<Real> a = mf.array(lb);
    const int ng = mf.nGrow();
    const Box gb = bx.grow(ng);

    const int n0 = gb.length(0), n1 = gb.length(1);
    const int n2 = (NDIM == 3) ? gb.length(2) : 1;
    const Box domain = geom.domain;

    Kokkos::parallel_reduce(
        "ghostError", Kokkos::RangePolicy<>(0, n0 * n1 * n2),
        KOKKOS_LAMBDA(int idx, Real& m) {
          const int i = gb.lo[0] + idx % n0;
          const int r = idx / n0;
          const int j = gb.lo[1] + r % n1;
          const int k = (NDIM == 3) ? gb.lo[2] + r / n1 : 0;

          Idx p{};
          p[0] = i;
          p[1] = j;
          if (NDIM == 3) p[2] = k;
          if (bx.contains(p)) return;  // valid cell, not a ghost

          const Real diff = a(i, j, k, 0) - analytic(geom, i, j, k);
          const Real d = Kokkos::abs(diff);
          if (d > m) m = d;
        },
        err);
  }
  return err.reference();
}

}  // namespace

int main(int argc, char** argv) {
  std::string cfgPath;
  bool dumpConfig = false;
  int threads = 0;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--dump-config") {
      dumpConfig = true;
    } else if (a == "--threads" && i + 1 < argc) {
      threads = std::atoi(argv[++i]);
    } else if (a == "--help" || a == "-h") {
      std::cout << "usage: saturnCFD [config.yaml] [--dump-config] [--threads N]\n";
      return 0;
    } else if (!a.empty() && a[0] != '-') {
      cfgPath = a;
    }
  }

  Kokkos::InitializationSettings init;
  if (threads > 0) init.set_num_threads(threads);
  Kokkos::initialize(init);
  {
    Config cfg = cfgPath.empty() ? Config{} : Config::fromFile(cfgPath);
    const Options opt = readOptions(cfg);

    if (dumpConfig) {
      cfg.dumpTemplate(std::cout);
      Kokkos::finalize();
      return 0;
    }

    std::cout << "saturnCFD 0.1.0\n";
    std::cout << "  backend      : " << ExecSpace().name() << "\n";
    std::cout << "  concurrency  : " << ExecSpace().concurrency() << "\n";
    std::cout << "  dimension    : " << NDIM << "D\n";
    std::cout << "  precision    : " << (sizeof(Real) == 8 ? "double" : "float") << "\n";

    // ---------------------------------------------------------------- grid --
    Geometry geom;
    Idx ext{};
    ext[0] = opt.nx;
    ext[1] = opt.ny;
    if (NDIM == 3) ext[2] = opt.nz;
    geom.domain = Box::fromExtent(ext);
    geom.dx = static_cast<Real>(opt.lx) / static_cast<Real>(opt.nx);
    geom.origin[0] = 0;
    geom.origin[1] = 0;
    geom.origin[2] = 0;
    for (int d = 0; d < NDIM; ++d) geom.periodic[d] = opt.periodic;

    Idx patch{};
    patch[0] = opt.bx;
    patch[1] = opt.by;
    if (NDIM == 3) patch[2] = opt.bz;

    const BoxArray ba = BoxArray::tileDomain(geom.domain, patch);
    const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

    std::cout << "  cells        : " << geom.domainPts() << "\n";
    std::cout << "  patches      : " << ba.size() << "\n";

    MultiFab f(ba, dm, 1, opt.nghost, 0, "f");
    MultiFab lap(ba, dm, 1, 0, 0, "lap");

    // ---------------------------------------------------------- initialise --
    {
      const Geometry g = geom;
      f.forEachCell([=](int, Array4<Real> a, int i, int j, int k) {
        a(i, j, k, 0) = analytic(g, i, j, k);
      });
    }

    // -------------------------------------------------------- halo exchange --
    HaloExchange hx(ba, dm, geom, opt.nghost, 0, 1);
    hx.build();
    hx.execute(f);

    std::cout << "  halo ops     : " << hx.numLocalOps() << " local, "
              << hx.numRemoteOps() << " remote\n";

    if (opt.verify_halo) {
      const Real err = ghostError(f, geom);
      std::cout << "  ghost error  : " << err << "  "
                << (err < Real(1e-5) ? "[PASS]" : "[FAIL]") << "\n";
    }

    // --------------------------------------------------------------- output --
    if (opt.out_every > 0) {
      VTKWriter vtk(opt.out_prefix);
      vtk.write(f, geom, 0, 0.0);
      vtk.finalize();
      std::cout << "  output       : " << opt.out_prefix << ".pvd\n";
    }

    // ------------------------------------------------------------ bandwidth --
    // Identical stencil (lap = laplacian(f)) through both kernel paths, so the
    // comparison isolates kernel-dispatch strategy. Two arrays are touched per
    // cell, so the GB/s figure is well defined.
    const View<Array4<Real>> lapA = lap.boxArrays();

    auto laplacianTeam = [&]() {
      const View<Array4<Real>> L = lapA;
      f.forEachCell(KOKKOS_LAMBDA(int b, Array4<Real> a, int i, int j, int k) {
        Array4<Real> out = L(b);
        Real v = -2.0 * Real(NDIM) * a(i, j, k, 0);
        v += a(i + 1, j, k, 0) + a(i - 1, j, k, 0);
        v += a(i, j + 1, k, 0) + a(i, j - 1, k, 0);
        if (NDIM == 3) v += a(i, j, k + 1, 0) + a(i, j, k - 1, 0);
        out(i, j, k, 0) = v;
      });
    };

    auto laplacianFlat = [&]() {
      const View<Array4<Real>> L = lapA;
      f.forEachCellFlat(KOKKOS_LAMBDA(int b, Array4<Real> a, int i, int j, int k) {
        Array4<Real> out = L(b);
        Real v = -2.0 * Real(NDIM) * a(i, j, k, 0);
        v += a(i + 1, j, k, 0) + a(i - 1, j, k, 0);
        v += a(i, j + 1, k, 0) + a(i, j - 1, k, 0);
        if (NDIM == 3) v += a(i, j, k + 1, 0) + a(i, j, k - 1, 0);
        out(i, j, k, 0) = v;
      });
    };

    const double cells = static_cast<double>(geom.domainPts());
    const double bytes = 2.0 * cells * sizeof(Real);

    auto timeIt = [&](const char* label, auto&& fn) {
      fn();  // warm up
      Kokkos::fence();
      // Best of 3: this dev box throttles, and a single sample is misleading.
      double best = 1e300;
      for (int rep = 0; rep < 3; ++rep) {
        Timer t;
        t.start();
        for (int it = 0; it < opt.bench_iters; ++it) fn();
        t.stop();
        best = std::min(best, t.seconds() / opt.bench_iters);
      }
      const double per = best;
      std::cout << "  " << label << " : " << per * 1e3 << " ms/iter, "
                << cells / per / 1e6 << " Mcell/s, " << bytes / per / 1e9
                << " GB/s\n";
    };

    auto laplacianAuto = [&]() {
      const View<Array4<Real>> L = lapA;
      f.forEachCellAuto(KOKKOS_LAMBDA(int b, Array4<Real> a, int i, int j, int k) {
        Array4<Real> out = L(b);
        Real v = -2.0 * Real(NDIM) * a(i, j, k, 0);
        v += a(i + 1, j, k, 0) + a(i - 1, j, k, 0);
        v += a(i, j + 1, k, 0) + a(i, j - 1, k, 0);
        if (NDIM == 3) v += a(i, j, k + 1, 0) + a(i, j, k - 1, 0);
        out(i, j, k, 0) = v;
      });
    };

    timeIt("stencil/team", laplacianTeam);
    timeIt("stencil/flat", laplacianFlat);
    timeIt("stencil/auto", laplacianAuto);

    std::cout << "  checksum     : " << lap.globalSum(0) << "\n";
  }
  Kokkos::finalize();
  return 0;
}
