# Run a case with output.vtkEvery and check the VTK frames and PVD collection.
#
# Required:
#   -DCLI=<saturnCFD executable>
#   -DCASE=<case YAML>
#   -DOUT=<working directory>

if(NOT DEFINED CLI OR NOT DEFINED CASE OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkBurgersVtk.cmake needs CLI, CASE and OUT")
endif()

file(REMOVE_RECURSE "${OUT}")
file(MAKE_DIRECTORY "${OUT}")

execute_process(
  COMMAND "${CLI}" "${CASE}"
  WORKING_DIRECTORY "${OUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "case failed (${result})\n${stdout}\n${stderr}")
endif()

file(GLOB frames "${OUT}/results/burgers/vtk_*.vtu")
list(LENGTH frames count)
if(count LESS 2)
  message(FATAL_ERROR "expected at least two VTK frames, got ${count}")
endif()

list(GET frames 0 first)
file(READ "${first}" vtu)
if(NOT vtu MATCHES "<VTKFile type=\"UnstructuredGrid\"")
  message(FATAL_ERROR "not an UnstructuredGrid VTU: ${first}")
endif()
if(NOT vtu MATCHES "Name=\"u\"")
  message(FATAL_ERROR "missing cell field 'u' in ${first}")
endif()

set(pvd "${OUT}/results/burgers/vtk.pvd")
if(NOT EXISTS "${pvd}")
  message(FATAL_ERROR "missing PVD collection: ${pvd}")
endif()
file(READ "${pvd}" pvdText)
if(NOT pvdText MATCHES "<VTKFile type=\"Collection\"")
  message(FATAL_ERROR "not a PVD collection: ${pvd}")
endif()

message(STATUS "burgers VTK OK: ${count} frames + PVD")
