#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <mpi.h>

int main(int argc, char** argv) {
  int provided = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

  doctest::Context ctx;
  ctx.applyCommandLine(argc, argv);
  const int local = ctx.run();

  int global = 0;
  MPI_Allreduce(&local, &global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

  MPI_Finalize();
  return global;
}
