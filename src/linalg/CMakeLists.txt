add_library(saturnLinAlg STATIC
  Petsc.cpp
)
add_library(saturn::LinAlg ALIAS saturnLinAlg)

target_link_libraries(saturnLinAlg PUBLIC saturn::core ${SATURN_PETSC_TARGET})
target_compile_definitions(saturnLinAlg PUBLIC SATURN_USE_PETSC)
