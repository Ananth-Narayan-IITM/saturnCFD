add_library(saturnBurger STATIC
  SaturnBurger.cpp
  BurgersCase.cpp
  Register.cpp
)
add_library(saturn::Burger ALIAS saturnBurger)

target_link_libraries(saturnBurger PUBLIC saturn::Simulation)
