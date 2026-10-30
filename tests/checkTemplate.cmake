if(NOT DEFINED CLI OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkTemplate.cmake needs CLI and OUT")
endif()

get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")

execute_process(
  COMMAND "${CLI}" -dumpConfig common -writeTemplate "${OUT}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "dumpConfig failed:\n${stdout}\n${stderr}")
endif()

if(NOT EXISTS "${OUT}.yaml")
  message(FATAL_ERROR "expected ${OUT}.yaml, but it does not exist")
endif()
