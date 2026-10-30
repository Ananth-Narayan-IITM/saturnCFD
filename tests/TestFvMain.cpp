#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

int main(int argc, char** argv) {
#ifdef SATURN_USE_MPI
  int provided = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
#endif

  Kokkos::initialize(argc, argv);

  int local = 0;
  {
    doctest::Context ctx;
    ctx.applyCommandLine(argc, argv);
    local = ctx.run();
  }

  int global = local;
#ifdef SATURN_USE_MPI
  MPI_Allreduce(&local, &global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
#endif

  Kokkos::finalize();

#ifdef SATURN_USE_MPI
  MPI_Finalize();
#endif
  return global;
}
