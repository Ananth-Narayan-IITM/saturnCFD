if(NOT DEFINED CLI OR NOT DEFINED MESH OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkVtk.cmake needs CLI, MESH and OUT")
endif()

get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")

if(WITH_PATCHES)
  execute_process(
    COMMAND "${CLI}" -checkMesh "${MESH}" -report "${OUT}" -writeVTK "${OUT}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
else()
  execute_process(
    COMMAND "${CLI}" -checkMesh "${MESH}" -report "${OUT}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
endif()

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "checkMesh failed:\n${stdout}\n${stderr}")
endif()

foreach(ext ".vtu" ".json")
  if(NOT EXISTS "${OUT}${ext}")
    message(FATAL_ERROR "missing VTK output: ${OUT}${ext}")
  endif()
endforeach()

if(WITH_PATCHES)
  foreach(ext ".pvd" "_xmin.vtp" "_zmax.vtp")
    if(NOT EXISTS "${OUT}${ext}")
      message(FATAL_ERROR "missing patch output: ${OUT}${ext}")
    endif()
  endforeach()
else()
  if(EXISTS "${OUT}.pvd")
    message(FATAL_ERROR "unexpected .pvd without -writeVTK: ${OUT}.pvd")
  endif()
endif()
