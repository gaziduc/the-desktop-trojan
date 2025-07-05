cmake_minimum_required(VERSION 3.31)
project(the-desktop-trojan)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(submodules)
add_subdirectory(src)