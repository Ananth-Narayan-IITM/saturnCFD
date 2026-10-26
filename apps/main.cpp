// saturnCFD — single solver binary, driven entirely by a case YAML file.
//
//   saturnCFD examples/cavity_re100/case.yaml
//   saturnCFD --list-solvers
//   saturnCFD --dump-config cavity        # commented template for that solver
//
// A case file selects the physics with the top-level key
//
//   solver: cavity
//
// Everything else in the file is read by that solver's option reader. Because
// the readers register every option they query, `--dump-config <solver>` can
// never drift from the code: it is generated from the same functions that
// consume the file.
#include <Kokkos_Core.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
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
#include "solver/CavitySolver.hpp"
#include "solver/Limiters.hpp"

using namespace saturn;

namespace {

// ===========================================================================
// Shared CLI
// ===========================================================================
struct Cli {
  std::string casePath;
  std::string dumpSolver;
  bool listSolvers = false;
  int threads = 0;
};

void printUsage() {
  std::cout <<
      "saturnCFD — finite-volume incompressible solver\n\n"
      "usage:\n"
      "  saturnCFD <case.yaml>          run a case\n"
      "  saturnCFD --list-solvers       list available solvers\n"
      "  saturnCFD --dump-config S      print a commented YAML template for S\n\n"
      "options:\n"
      "  --threads N                    override the Kokkos thread count\n"
      "  -h, --help                     this message\n";
}

void printSolvers() {
  std::cout <<
      "available solvers (select with `solver:` in the case file):\n\n"
      "  cavity      lid-driven cavity, staggered projection, single level\n"
      "  substrate   parallel substrate benchmark (halo exchange + bandwidth)\n";
}

// ===========================================================================
// Solver 1 — substrate benchmark
// ===========================================================================
struct SubstrateOptions {
  int nx = 1024, ny = 1024, nz = 64;
  double lx = 1.0;
  int bx = 256, by = 256, bz = 32;
  int nghost = 2;
  bool periodic = true;
  int outEvery = 0;
  std::string outPrefix = "results/substrate/field";
  int benchIters = 20;
  bool verifyHalo = true;
};

SubstrateOptions readSubstrateOptions(const Config& cfg) {
  SubstrateOptions o;
  o.nx = cfg.getInt("domain.nx", o.nx, "cells in x at level 0");
  o.ny = cfg.getInt("domain.ny", o.ny, "cells in y at level 0");
  o.nz = cfg.getInt("domain.nz", o.nz, "cells in z (3D builds only)");
  o.lx = cfg.getDouble("domain.lx", o.lx, "physical length in x");
  o.bx = cfg.getInt("grid.block_x", o.bx, "max patch size in x");
  o.by = cfg.getInt("grid.block_y", o.by, "max patch size in y");
  o.bz = cfg.getInt("grid.block_z", o.bz, "max patch size in z");
  o.nghost = cfg.getInt("grid.nghost", o.nghost, "ghost layers per field");
  o.periodic = cfg.getBool("boundary.periodic", o.periodic,
                           "periodic in every direction");
  o.outEvery = cfg.getInt("output.every", o.outEvery,
                          "write VTK every N steps (0 = never)");
  o.outPrefix = cfg.getString("output.prefix", o.outPrefix, "VTK output prefix");
  o.benchIters = cfg.getInt("bench.iters", o.benchIters,
                            "stencil iterations for the bandwidth benchmark");
  o.verifyHalo = cfg.getBool("bench.verify_halo", o.verifyHalo,
                             "compare ghost cells against the analytic field");
  return o;
}

KOKKOS_INLINE_FUNCTION
Real analyticField(const Geometry& g, int i, int j, int k) {
  const Real pi = 3.14159265358979323846;
  Real v = Kokkos::sin(2 * pi * g.xc(0, i) / g.length(0)) *
           Kokkos::sin(2 * pi * g.xc(1, j) / g.length(1));
  if (NDIM == 3) v *= Kokkos::sin(2 * pi * g.xc(2, k) / g.length(2));
  return v;
}

Real substrateGhostError(const MultiFab& mf, const Geometry& geom) {
  Real mx = Real(0);
  Kokkos::Max<Real> err(mx);
  for (int lb = 0; lb < mf.numLocalBoxes(); ++lb) {
    const Box bx = mf.boxArray()[mf.localToGlobal(lb)];
    const Array4<Real> a = mf.array(lb);
    const Box gb = bx.grow(mf.nGrow());
    const int n0 = gb.length(0), n1 = gb.length(1);
    const int n2 = (NDIM == 3) ? gb.length(2) : 1;
    const Box dom = geom.domain;
    Kokkos::parallel_reduce(
        "substrate::ghostError", Kokkos::RangePolicy<>(0, n0 * n1 * n2),
        KOKKOS_LAMBDA(int idx, Real& m) {
          const int i = gb.lo[0] + idx % n0;
          const int r = idx / n0;
          const int j = gb.lo[1] + r % n1;
          const int k = (NDIM == 3) ? (gb.lo[2] + r / n1) : 0;
          Idx p{i, j, k};
          if (bx.contains(p)) return;
          if (!dom.contains(p)) return;
          const Real d =
              Kokkos::abs(a(i, j, k, 0) - analyticField(geom, i, j, k));
          if (d > m) m = d;
        },
        err);
  }
  return err.reference();
}

int runSubstrate(const SubstrateOptions& opt) {
  Geometry geom;
  Idx ext{};
  ext[0] = opt.nx;
  ext[1] = opt.ny;
  if (NDIM == 3) ext[2] = opt.nz;
  geom.domain = Box::fromExtent(ext);
  geom.dx = static_cast<Real>(opt.lx) / static_cast<Real>(opt.nx);
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

  const Geometry g = geom;
  f.forEachCell([=](int, Array4<Real> a, int i, int j, int k) {
    a(i, j, k, 0) = analyticField(g, i, j, k);
  });

  HaloExchange hx(ba, dm, geom, opt.nghost, 0, 1);
  hx.build();
  hx.execute(f);
  std::cout << "  halo ops     : " << hx.numLocalOps() << " local, "
            << hx.numRemoteOps() << " remote\n";

  if (opt.verifyHalo) {
    const Real err = substrateGhostError(f, geom);
    std::cout << "  ghost error  : " << err << "  "
              << (err < Real(1e-5) ? "[PASS]" : "[FAIL]") << "\n";
  }

  if (opt.outEvery > 0) {
    VTKWriter vtk(opt.outPrefix);
    vtk.write(f, geom, 0, 0.0);
    vtk.finalize();
    std::cout << "  output       : " << opt.outPrefix << ".pvd\n";
  }

  const View<Array4<Real>> lapA = lap.boxArrays();
  auto laplacian = [&]() {
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

  const double cells = static_cast<double>(geom.domainPts());
  const double bytes = 2.0 * cells * sizeof(Real);

  auto timeIt = [&](const char* label, auto&& fn) {
    fn();
    Kokkos::fence();
    double best = 1e300;
    for (int rep = 0; rep < 3; ++rep) {
      Timer t;
      t.start();
      for (int it = 0; it < opt.benchIters; ++it) fn();
      t.stop();
      best = std::min(best, t.seconds() / opt.benchIters);
    }
    std::cout << "  " << label << " : " << best * 1e3 << " ms/iter, "
              << cells / best / 1e6 << " Mcell/s, "
              << bytes / best / 1e9 << " GB/s\n";
  };
  timeIt("stencil", laplacian);
  std::cout << "  checksum     : " << lap.globalSum(0) << "\n";
  return 0;
}

// ===========================================================================
// Solver 2 — lid-driven cavity
// ===========================================================================
struct CavityCase {
  CavityParams prm;
  int nx = 128, ny = 128;
  double length = 1.0;
  double endTime = 100.0;
  int maxSteps = 500000;
  double steadyTol = 1e-6;
  int checkEvery = 50;
  std::string outPrefix = "results/cavity";
  bool centreLines = true;
  int vtkEvery = 0;
};

CavityCase readCavityCase(const Config& cfg) {
  CavityCase c;
  c.nx = cfg.getInt("domain.nx", c.nx, "cells in x");
  c.ny = cfg.getInt("domain.ny", c.ny, "cells in y");
  c.length = cfg.getDouble("domain.length", c.length, "physical side length");

  c.prm.reynolds =
      cfg.getDouble("flow.reynolds", c.prm.reynolds, "Re = U*L/nu");
  c.prm.lidVelocity =
      cfg.getDouble("flow.lid_velocity", c.prm.lidVelocity, "lid speed");
  const std::string scheme =
      cfg.getString("flow.scheme", "minmod",
                    "convection scheme: upwind|minmod|vanleer|superbee|mc|quick|central");
  c.prm.scheme = parseScheme(scheme);
  c.prm.cfl = cfg.getDouble("flow.cfl", c.prm.cfl,
                            "convective CFL safety factor");
  c.prm.diffusionFactor =
      cfg.getDouble("flow.diffusion_safety", c.prm.diffusionFactor,
                    "explicit viscous stability factor");

  c.endTime = cfg.getDouble("time.end_time", c.endTime, "physical end time");
  c.maxSteps = cfg.getInt("time.max_steps", c.maxSteps, "hard step limit");
  c.steadyTol = cfg.getDouble("time.steady_tol", c.steadyTol,
                              "steady state: stop when max|du| below this");
  c.checkEvery = cfg.getInt("time.check_every", c.checkEvery,
                            "steps between steady-state checks");

  c.prm.poissonMaxIter =
      cfg.getInt("pressure.max_iter", c.prm.poissonMaxIter,
                 "pressure CG iteration limit");
  c.prm.poissonTol =
      cfg.getDouble("pressure.tolerance", (double)c.prm.poissonTol,
                    "pressure CG relative residual");

  c.outPrefix = cfg.getString("output.prefix", c.outPrefix,
                              "output path prefix (directory is created)");
  c.centreLines = cfg.getBool("output.centre_lines", c.centreLines,
                              "write centreline CSV for Ghia comparison");
  c.vtkEvery = cfg.getInt("output.vtk_every", c.vtkEvery,
                          "write VTK every N steps (0 = off)");
  return c;
}

int runCavity(const CavityCase& c) {
  Geometry geom;
  Idx ext{};
  ext[0] = c.nx;
  ext[1] = c.ny;
  geom.domain = Box::fromExtent(ext);
  geom.dx = static_cast<Real>(c.length) / static_cast<Real>(c.nx);

  CavitySolver sim(geom, c.prm);
  sim.initialize();

  std::cout << "  grid      : " << c.nx << " x " << c.ny << "\n";
  std::cout << "  Re        : " << c.prm.reynolds << "\n";
  std::cout << "  scheme    : " << schemeName(c.prm.scheme) << "\n";

  std::unique_ptr<VTKWriter> vtk;
  if (c.vtkEvery > 0) vtk = std::make_unique<VTKWriter>(c.outPrefix + "/field");

  const BoxArray ba = sim.velocity().boxArray();
  const DistributionMapping dm = sim.velocity().distributionMap();

  auto onOutput = [&](int step, Real t) {
    if (!vtk) return;
    MultiFab cell(ba, dm, 2, 0, 0, "cell");
    sim.cellCentredVelocity(cell);
    vtk->write(cell, geom, step, static_cast<double>(t), {"u", "v"});
  };

  const SteadyReport rep =
      sim.runToSteady(static_cast<Real>(c.endTime), c.maxSteps,
                      static_cast<Real>(c.steadyTol), c.checkEvery,
                      c.vtkEvery, onOutput);

  if (vtk) {
    vtk->finalize();
    std::cout << "  vtk       : " << c.outPrefix << "/field.pvd\n";
  }

  std::cout << "  steps     : " << rep.steps << "\n";
  std::cout << "  time      : " << rep.time << "\n";
  std::cout << "  max|du|   : " << rep.maxDelta << "\n";
  std::cout << "  max|u|    : " << sim.maxSpeed() << "\n";
  std::cout << "  max|div u : " << sim.maxDivergence() << "\n";
  std::cout << "  converged : " << (rep.converged ? "yes" : "no (hit limit)")
            << "\n";

  if (c.centreLines) {
    sim.writeCentreLines(c.outPrefix + "/centreline");
    std::cout << "  centreline: " << c.outPrefix << "/centreline_u.csv, "
              << c.outPrefix << "/centreline_v.csv\n";
  }
  return 0;
}

}  // namespace

// ===========================================================================
int main(int argc, char** argv) {
  Cli cli;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--dump-config" && i + 1 < argc) cli.dumpSolver = argv[++i];
    else if (a == "--list-solvers") cli.listSolvers = true;
    else if (a == "--threads" && i + 1 < argc) cli.threads = std::atoi(argv[++i]);
    else if (a == "-h" || a == "--help") { printUsage(); return 0; }
    else if (!a.empty() && a[0] != '-') cli.casePath = a;
  }

  if (cli.listSolvers) { printSolvers(); return 0; }

  Kokkos::InitializationSettings init;
  if (cli.threads > 0) init.set_num_threads(cli.threads);
  Kokkos::initialize(init);

  int rc = 0;
  try {
    // ---- generate a template for a solver, without running anything --------
    if (!cli.dumpSolver.empty()) {
      const Config empty;
      if (cli.dumpSolver == "cavity") {
        (void)readCavityCase(empty);
      } else if (cli.dumpSolver == "substrate") {
        (void)readSubstrateOptions(empty);
      } else {
        std::cerr << "unknown solver '" << cli.dumpSolver << "'\n";
        printSolvers();
        rc = 1;
      }
      if (rc == 0) {
        std::cout << "solver: " << cli.dumpSolver << "\n\n";
        empty.dumpTemplate(std::cout);
      }
    }
    // ---- run a case -------------------------------------------------------
    else if (!cli.casePath.empty()) {
      const Config cfg = Config::fromFile(cli.casePath);
      const std::string solver = cfg.getString("solver", "cavity",
                                               "which solver to run");

      std::cout << "saturnCFD 0.2.0\n";
      std::cout << "  case      : " << cli.casePath << "\n";
      std::cout << "  solver    : " << solver << "\n";
      std::cout << "  backend   : " << ExecSpace().name() << "\n";
      std::cout << "  dimension : " << NDIM << "D\n";

      if (solver == "cavity") {
        const CavityCase cc = readCavityCase(cfg);
        cfg.reportUnknown(std::cerr);
        rc = runCavity(cc);
      } else if (solver == "substrate") {
        const SubstrateOptions so = readSubstrateOptions(cfg);
        cfg.reportUnknown(std::cerr);
        rc = runSubstrate(so);
      } else {
        std::cerr << "unknown solver '" << solver << "' in " << cli.casePath
                  << "\n";
        printSolvers();
        rc = 1;
      }
    } else {
      printUsage();
      rc = 1;
    }
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    rc = 1;
  }

  Kokkos::finalize();
  return rc;
}
