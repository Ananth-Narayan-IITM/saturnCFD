# Built-in solvers are added here. A user solver is a copy under
# src/solvers/<name> plus one line:
#
#   addSolver(myBurgers)
#
# Naming convention: saturn<SolverName> for the library target.
add_subdirectory(saturnBurger)
