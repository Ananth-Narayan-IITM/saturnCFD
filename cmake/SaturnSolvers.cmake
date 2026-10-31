# addSolver(name)
#
# Adds src/solvers/<name>, creates its static library and records it for
# saturnCFD. A user copies a solver directory and adds exactly one line:
#
#   addSolver(myBurgers)
#
function(addSolver name)
  set(dir "${SATURN_SOURCE_DIR}/src/solvers/${name}")
  if(NOT EXISTS "${dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "addSolver(${name}): ${dir}/CMakeLists.txt does not exist")
  endif()

  add_subdirectory("${dir}" "${CMAKE_BINARY_DIR}/solvers/${name}")

  if(TARGET saturn_solver_${name})
    set(solver_target saturn_solver_${name})
  elseif(TARGET ${name})
    set(solver_target ${name})
  else()
    message(FATAL_ERROR
      "addSolver(${name}): the solver CMakeLists must define "
      "${name} or saturn_solver_${name}")
  endif()

  target_link_libraries(${solver_target} PUBLIC saturn::Core)
  set_property(GLOBAL APPEND PROPERTY SATURN_SOLVER_TARGETS ${solver_target})
endfunction()
