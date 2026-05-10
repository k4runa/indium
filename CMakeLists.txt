cmake_minimum_required(VERSION 3.10)

project(Indium VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(Indium src/main.cpp)
find_package(raylib REQUIRED)

target_include_directories(Indium PRIVATE include)
target_link_libraries(Indium
    raylib
    GL
    m
    pthread
    dl
    rt
)
