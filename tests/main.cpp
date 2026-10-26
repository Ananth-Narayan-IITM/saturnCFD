#include <Kokkos_Core.hpp>

#include "test.hpp"

int main(int argc, char** argv) {
  Kokkos::initialize(argc, argv);
  int rc = 0;
  {
    std::cout << "saturnCFD unit tests\n";
    rc = t::runAll();
  }
  Kokkos::finalize();
  return rc;
}
