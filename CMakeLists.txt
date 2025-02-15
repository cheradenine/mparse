cmake_minimum_required(VERSION 3.22)  # Specify minimum CMake version

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(cparse)  # Set the project name

# Add an executable target
add_executable(cparser main.cpp)
