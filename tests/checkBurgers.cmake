# Run one YAML case and check the analytic-comparison output.
#
# Required:
#   -DCLI=<saturnCFD executable>
#   -DCASE=<case YAML>
#   -DOUT=<working directory>
#
# Optional:
#   -DMPIEXEC=<mpiexec> -DRANKS=<n>
#   -DPREFIX=<output prefix relative to OUT>   (default: burgers/traveling64)
#   -DROWS=<expected CSV rows>                 (default: 65)
#   -DL2_MIN / -DL2_MAX / -DLINF_MAX           (defaults for traveling64)

if(NOT DEFINED CLI OR NOT DEFINED CASE OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkBurgers.cmake needs CLI, CASE and OUT")
endif()

if(NOT DEFINED PREFIX)
  set(PREFIX "results/burgers/traveling64")
endif()
if(NOT DEFINED ROWS)
  set(ROWS 65)
endif()
if(NOT DEFINED L2_MIN)
  set(L2_MIN 2.0e-3)
endif()
if(NOT DEFINED L2_MAX)
  set(L2_MAX 2.6e-3)
endif()
if(NOT DEFINED LINF_MAX)
  set(LINF_MAX 1.0e-2)
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

set(json "${OUT}/${PREFIX}.json")
set(csv "${OUT}/${PREFIX}.csv")
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
string(JSON linf GET "${text}" exact linf)

if(l2 LESS ${L2_MIN} OR l2 GREATER ${L2_MAX})
  message(FATAL_ERROR
    "unexpected exact L2 = ${l2}; expected ${L2_MIN}..${L2_MAX}")
endif()
if(linf GREATER ${LINF_MAX})
  message(FATAL_ERROR "unexpected exact Linf = ${linf}; expected < ${LINF_MAX}")
endif()

file(STRINGS "${csv}" lines)
list(LENGTH lines rows)
if(NOT rows EQUAL ${ROWS})
  message(FATAL_ERROR "expected ${ROWS} CSV rows, got ${rows}")
endif()

message(STATUS
  "burgers case OK: L2=${l2} Linf=${linf} rows=${rows}")
