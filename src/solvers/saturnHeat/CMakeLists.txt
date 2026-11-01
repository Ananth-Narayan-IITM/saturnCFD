add_library(saturnHeat STATIC
  SaturnHeat.cpp
  HeatCase.cpp
)
add_library(saturn::Heat ALIAS saturnHeat)

target_link_libraries(saturnHeat PUBLIC saturn::Simulation)
