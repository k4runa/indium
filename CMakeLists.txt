cmake_minimum_required(VERSION 3.10)
project(Indium VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Derlenecek dosyalar: main + rlImGui + ImGui çekirdek dosyaları
set(SOURCES
    src/main.cpp
    include/rlImGui.cpp
    include/imgui.cpp
    include/imgui_draw.cpp
    include/imgui_widgets.cpp
    include/imgui_tables.cpp
    include/imgui_demo.cpp
)

add_executable(Indium ${SOURCES})

find_package(raylib REQUIRED)

# Header'ları bulması için include klasörünü göster
target_include_directories(Indium PRIVATE include)

# Raylib ve gerekli sistem kütüphanelerini bağla
target_link_libraries(Indium
    raylib
    GL
    m
    pthread
    dl
    rt
)
