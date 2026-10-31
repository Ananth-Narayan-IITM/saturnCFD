# Built-in solvers are added here. A user solver is a copy under
# src/solvers/<name> plus one line:
#
#   addSolver(myBurgers)
#
# Naming convention: saturn<SolverName> for the library target.
addSolver(saturnBurger)

# Case-driven registration for the CLI. Every built-in solver adds one call in
# BuiltinSolvers.cpp; main only links this library.
add_library(saturnBuiltinSolvers STATIC BuiltinSolvers.cpp)
add_library(saturn::BuiltinSolvers ALIAS saturnBuiltinSolvers)
target_link_libraries(saturnBuiltinSolvers PUBLIC
  saturn::Simulation saturnBurger)
