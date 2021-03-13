cmake_minimum_required(VERSION 3.10)

project("poisson-disk-generator")

add_executable(Poisson Poisson.cpp PoissonGenerator.h)

set_property(TARGET Poisson PROPERTY CXX_STANDARD 11)
set_property(TARGET Poisson PROPERTY CXX_STANDARD_REQUIRED ON)
