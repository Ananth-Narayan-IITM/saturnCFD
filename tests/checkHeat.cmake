# Run one saturnHeat YAML case and check the analytic-comparison output.
#
# Required:
#   -DCLI=<saturnCFD executable>
#   -DCASE=<case YAML>
#   -DOUT=<working directory>
#
# Optional:
#   -DMPIEXEC=<mpiexec> -DRANKS=<n>
#   -DPREFIX=<output prefix relative to OUT>   (default: results/heat3d/fourier32)
#   -DROWS=<expected CSV rows>                 (default: 32769)
#   -DL2_MIN / -DL2_MAX / -DLINF_MAX           (defaults for 3D 32^3)

if(NOT DEFINED CLI OR NOT DEFINED CASE OR NOT DEFINED OUT)
  message(FATAL_ERROR "checkHeat.cmake needs CLI, CASE and OUT")
endif()

if(NOT DEFINED PREFIX)
  set(PREFIX "results/heat3d/fourier32")
endif()
if(NOT DEFINED ROWS)
  set(ROWS 32769)
endif()
if(NOT DEFINED L2_MIN)
  set(L2_MIN 3.0e-3)
endif()
if(NOT DEFINED L2_MAX)
  set(L2_MAX 5.0e-3)
endif()
if(NOT DEFINED LINF_MAX)
  set(LINF_MAX 1.5e-2)
endif()

# Start clean: a stale CSV/JSON would hide a failed or misplaced output.
file(REMOVE_RECURSE "${OUT}")
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
string(JSON solver GET "${text}" solver)
string(JSON l2 ERROR_VARIABLE json_error GET "${text}" exact l2)
if(json_error)
  message(FATAL_ERROR "cannot read exact.l2 from ${json}: ${json_error}")
endif()
string(JSON linf GET "${text}" exact linf)

if(NOT solver STREQUAL "saturnHeat")
  message(FATAL_ERROR "unexpected solver '${solver}', expected saturnHeat")
endif()
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
  "heat case OK: L2=${l2} Linf=${linf} rows=${rows}")
