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

if(SATURN_ENABLE_KOKKOS)
  list(APPEND SATURN_CORE_SOURCES
    fv/FieldHalo.cpp
    fv/Laplacian.cpp
    fv/LocalMesh.cpp
    fv/Operators.cpp
    fv/BoundaryFill.cpp
    fv/Convection.cpp
  )
endif()

add_library(saturnCore STATIC ${SATURN_CORE_SOURCES})
add_library(saturn::Core ALIAS saturnCore)

target_include_directories(saturnCore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(saturnCore PUBLIC ${SATURN_YAML_TARGET})

if(SATURN_ENABLE_MPI)
  target_link_libraries(saturnCore PUBLIC MPI::MPI_CXX)
  target_compile_definitions(saturnCore PUBLIC SATURN_USE_MPI)
endif()

if(SATURN_ENABLE_KOKKOS)
  target_link_libraries(saturnCore PUBLIC Kokkos::kokkos)
  target_compile_definitions(saturnCore PUBLIC SATURN_USE_KOKKOS)
  string(TOUPPER "${SATURN_BACKEND}" SATURN_BACKEND_UPPER)
  target_compile_definitions(saturnCore PUBLIC
    SATURN_BACKEND_${SATURN_BACKEND_UPPER})
endif()

if(SATURN_ENABLE_PETSC)
  add_subdirectory(linalg)
endif()

add_subdirectory(simulation)
add_subdirectory(solvers)
