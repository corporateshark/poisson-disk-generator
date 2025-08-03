cmake_minimum_required(VERSION 3.17)

project("poisson-disk-generator")

add_executable(Poisson Poisson.cpp PoissonGenerator.h)

if(MSVC)
  target_compile_definitions(Poisson PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET Poisson PROPERTY CXX_STANDARD 11)
set_property(TARGET Poisson PROPERTY CXX_STANDARD_REQUIRED ON)
