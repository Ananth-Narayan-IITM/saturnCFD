if(NOT DEFINED CLI OR NOT DEFINED MESH OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkVtk.cmake needs CLI, MESH and OUT")
endif()

get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")

execute_process(
  COMMAND "${CLI}" -checkMesh "${MESH}" -report "${OUT}.json" -writeVtk "${OUT}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "checkMesh failed:\n${stdout}\n${stderr}")
endif()

foreach(ext ".vtu" ".pvd" ".json" "_xmin.vtp" "_zmax.vtp")
  if(NOT EXISTS "${OUT}${ext}")
    message(FATAL_ERROR "missing VTK output: ${OUT}${ext}")
  endif()
endforeach()
