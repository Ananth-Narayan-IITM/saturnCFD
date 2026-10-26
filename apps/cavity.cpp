// saturnCFD — lid-driven cavity driver.
//
//   ./saturn_cavity --n 128 --re 1000 --tend 400 --tol 1e-6 --out cavity_re1000
//
// Writes two CSV files for comparison against Ghia et al. (1982), Basilisk or
// OpenFOAM:
//   <out>_u.csv   u along the vertical centreline  x = 0.5   (columns: y, u)
//   <out>_v.csv   v along the horizontal centreline y = 0.5  (columns: x, v)
#include <Kokkos_Core.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "core/Types.hpp"
#include "mesh/Geometry.hpp"
#include "solver/CavitySolver.hpp"

using namespace saturn;

int main(int argc, char** argv) {
  int n = 64;
  double re = 100.0;
  double tend = 200.0;
  int maxSteps = 500000;
  double tol = 1e-6;
  int checkEvery = 50;
  std::string out = "cavity";

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string {
      return (i + 1 < argc) ? argv[++i] : std::string();
    };
    if (a == "--n") n = std::atoi(next().c_str());
    else if (a == "--re") re = std::atof(next().c_str());
    else if (a == "--tend") tend = std::atof(next().c_str());
    else if (a == "--steps") maxSteps = std::atoi(next().c_str());
    else if (a == "--tol") tol = std::atof(next().c_str());
    else if (a == "--check") checkEvery = std::atoi(next().c_str());
    else if (a == "--out") out = next();
    else if (a == "--help" || a == "-h") {
      std::cout << "usage: saturn_cavity --n N --re RE --tend T --steps N "
                   "--tol TOL --out PREFIX\n";
      return 0;
    }
  }

  Kokkos::initialize(argc, argv);
  {
    std::cout << "saturnCFD lid-driven cavity\n";
    std::cout << "  backend   : " << ExecSpace().name() << "\n";
    std::cout << "  grid      : " << n << " x " << n << "\n";
    std::cout << "  Re        : " << re << "\n";

    Geometry geom;
    Idx ext{};
    ext[0] = n;
    ext[1] = n;
    geom.domain = Box::fromExtent(ext);
    geom.dx = Real(1) / Real(n);
    geom.origin[0] = 0;
    geom.origin[1] = 0;

    CavityParams prm;
    prm.reynolds = static_cast<Real>(re);
    prm.lidVelocity = 1;

    CavitySolver sim(geom, prm);
    sim.initialize();

    const SteadyReport rep = sim.runToSteady(static_cast<Real>(tend), maxSteps,
                                             static_cast<Real>(tol), checkEvery);

    std::cout << "  steps     : " << rep.steps << "\n";
    std::cout << "  time      : " << rep.time << "\n";
    std::cout << "  max|du|   : " << rep.maxDelta << "\n";
    std::cout << "  max|u|    : " << sim.maxSpeed() << "\n";
    std::cout << "  max|div u : " << sim.maxDivergence() << "\n";
    std::cout << "  converged : " << (rep.converged ? "yes" : "no (hit limit)")
              << "\n";

    sim.writeCentreLines(out);
    std::cout << "  output    : " << out << "_u.csv, " << out << "_v.csv\n";
  }
  Kokkos::finalize();
  return 0;
}
