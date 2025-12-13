cmake_minimum_required(VERSION 3.22)

project("poisson-disk-generator")

add_library(PoissonGenerator INTERFACE PoissonGenerator.h)

add_executable(Poisson Poisson.cpp)

target_link_libraries(Poisson PRIVATE PoissonGenerator)

if(MSVC)
  target_compile_definitions(Poisson PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET PoissonGenerator PROPERTY CXX_STANDARD 17)
set_property(TARGET PoissonGenerator PROPERTY CXX_STANDARD_REQUIRED ON)
