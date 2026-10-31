add_library(saturnSimulation STATIC
  Simulation.cpp
)
add_library(saturn::Simulation ALIAS saturnSimulation)

target_link_libraries(saturnSimulation PUBLIC saturn::core)
