#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <mpi.h>
#include <petsc.h>
#include <Kokkos_Core.hpp>

#include "linalg/Petsc.hpp"

int main(int argc, char** argv) {
  int provided = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

  int global = 0;
  {
    saturn::PetscScope petsc(argc, argv);
    Kokkos::initialize(argc, argv);

    int local = 0;
    {
      doctest::Context ctx;
      ctx.applyCommandLine(argc, argv);
      local = ctx.run();
    }
    MPI_Allreduce(&local, &global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    Kokkos::finalize();
  }

  MPI_Finalize();
  return global;
}
