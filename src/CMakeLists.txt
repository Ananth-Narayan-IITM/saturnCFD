set(SATURN_CORE_SOURCES
  core/Units.cpp
  config/Config.cpp
  config/ConfigSchema.cpp
  config/CommonSchema.cpp
  mesh/GmshReader.cpp
  mesh/Mesh.cpp
  mesh/CheckMesh.cpp
  mesh/VtuWriter.cpp
)

add_library(saturn_core STATIC ${SATURN_CORE_SOURCES})
add_library(saturn::core ALIAS saturn_core)

target_include_directories(saturn_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(saturn_core PUBLIC ${SATURN_YAML_TARGET})

add_subdirectory(solvers)
