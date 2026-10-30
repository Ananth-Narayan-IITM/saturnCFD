add_executable(saturnCFD main.cpp)
set_target_properties(saturnCFD PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_link_libraries(saturnCFD PRIVATE saturn::core)

get_property(SATURN_SOLVER_TARGETS GLOBAL PROPERTY SATURN_SOLVER_TARGETS)
if(SATURN_SOLVER_TARGETS)
  target_link_libraries(saturnCFD PRIVATE ${SATURN_SOLVER_TARGETS})
endif()
