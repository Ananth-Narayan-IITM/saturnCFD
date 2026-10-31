# Run one YAML case and check the analytic-comparison output.
#
# Required:
#   -DCLI=<saturnCFD executable>
#   -DCASE=<case YAML>
#   -DOUT=<working directory>
#   -DL2_EXPECTED=<expected exact L2, used for a +/- 15% window>
#
# Optional:
#   -DMPIEXEC=<mpiexec> -DRANKS=<n>

if(NOT DEFINED CLI OR NOT DEFINED CASE OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkBurgers.cmake needs CLI, CASE and OUT")
endif()

file(MAKE_DIRECTORY "${OUT}")
if(DEFINED MPIEXEC AND DEFINED RANKS AND RANKS GREATER 1)
  execute_process(
    COMMAND "${MPIEXEC}" -np "${RANKS}" "${CLI}" "${CASE}"
    WORKING_DIRECTORY "${OUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
else()
  execute_process(
    COMMAND "${CLI}" "${CASE}"
    WORKING_DIRECTORY "${OUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
endif()

if(NOT result EQUAL 0)
  message(FATAL_ERROR "case failed (${result})\n${stdout}\n${stderr}")
endif()

set(json "${OUT}/results/burgers/traveling64.json")
set(csv "${OUT}/results/burgers/traveling64.csv")
if(NOT EXISTS "${json}")
  message(FATAL_ERROR "missing JSON summary: ${json}")
endif()
if(NOT EXISTS "${csv}")
  message(FATAL_ERROR "missing CSV profile: ${csv}")
endif()

file(READ "${json}" text)
string(JSON l2 ERROR_VARIABLE json_error GET "${text}" exact l2)
if(json_error)
  message(FATAL_ERROR "cannot read exact.l2 from ${json}: ${json_error}")
endif()

if(NOT DEFINED L2_EXPECTED)
  set(L2_EXPECTED 2.284148e-3)
endif()

if(l2 LESS 2.0e-3 OR l2 GREATER 2.6e-3)
  message(FATAL_ERROR
    "unexpected exact L2 = ${l2}; expected near ${L2_EXPECTED}")
endif()

file(STRINGS "${csv}" lines)
list(LENGTH lines rows)
if(NOT rows EQUAL 65)
  message(FATAL_ERROR "expected 65 CSV rows, got ${rows}")
endif()

string(JSON linf GET "${text}" exact linf)
if(linf GREATER 1.0e-2)
  message(FATAL_ERROR "unexpected exact Linf = ${linf}")
endif()

message(STATUS "burgers case OK: L2=${l2} Linf=${linf} rows=${rows}")
