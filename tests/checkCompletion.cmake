if(NOT DEFINED CLI)
  message(FATAL_ERROR "checkCompletion.cmake needs CLI")
endif()

execute_process(
  COMMAND "${CLI}" -completion bash
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "completion failed:\n${err}")
endif()

foreach(needle "compgen -f" "-o filenames" "-writeVTK" "-dumpConfig")
  string(FIND "${out}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "completion script is missing '${needle}'")
  endif()
endforeach()
