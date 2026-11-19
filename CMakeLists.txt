cmake_minimum_required(VERSION 3.14)
project(conjuntos_disjuntos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

include_directories(include bench)

add_executable(dsu_bench bench/bench.cpp)
add_executable(test_dsu tests/test_dsu.cpp)
add_executable(componentes examples/componentes.cpp)
add_executable(kruskal examples/kruskal.cpp)

enable_testing()
add_test(NAME test_dsu COMMAND test_dsu)
