cmake_minimum_required(VERSION 3.30)
project(quilibrium_package_consumer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

find_package(Quilibrium CONFIG REQUIRED)

add_executable(quilibrium_cpp_package_consumer main.cpp)
target_link_libraries(quilibrium_cpp_package_consumer PRIVATE Quilibrium::SDK)

if(TARGET Quilibrium::C)
  add_executable(quilibrium_c_package_consumer main.c)
  target_link_libraries(quilibrium_c_package_consumer PRIVATE Quilibrium::C)
endif()
