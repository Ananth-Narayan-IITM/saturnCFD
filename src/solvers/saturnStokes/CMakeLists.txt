add_library(saturnStokes STATIC
  SaturnStokes.cpp
  StokesCase.cpp
  StokesMms.cpp
)
add_library(saturn::Stokes ALIAS saturnStokes)

target_link_libraries(saturnStokes PUBLIC saturn::Simulation saturn::LinAlg)
