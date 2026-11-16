cmake_minimum_required(VERSION 3.10)
project(HelloWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")

add_executable(hello src/main.cpp src/Sudoku.cpp)
target_include_directories(hello PRIVATE src)
target_link_options(hello PRIVATE -flto)
