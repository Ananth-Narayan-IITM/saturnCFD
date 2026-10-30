set(SATURN_CORE_SOURCES
  core/Units.cpp
  config/Config.cpp
  config/ConfigSchema.cpp
  config/CommonSchema.cpp
  mesh/GmshReader.cpp
  mesh/Mesh.cpp
  mesh/CheckMesh.cpp
  mesh/VtuWriter.cpp
  comm/Partition.cpp
  comm/HaloExchange.cpp
)

add_library(saturn_core STATIC ${SATURN_CORE_SOURCES})
add_library(saturn::core ALIAS saturn_core)

target_include_directories(saturn_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(saturn_core PUBLIC ${SATURN_YAML_TARGET})

if(SATURN_ENABLE_MPI)
  target_link_libraries(saturn_core PUBLIC MPI::MPI_CXX)
  target_compile_definitions(saturn_core PUBLIC SATURN_USE_MPI)
endif()

add_subdirectory(solvers)
