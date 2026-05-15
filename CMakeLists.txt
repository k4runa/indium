cmake_minimum_required(VERSION 3.10)
project(Indium VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Source files: main + rlImGui + ImGui core files
set(SOURCES
    src/main.cpp
    include/rlImGui.cpp
    include/imgui.cpp
    include/imgui_draw.cpp
    include/imgui_widgets.cpp
    include/imgui_tables.cpp
    include/imgui_demo.cpp
    Component/RigidbodyComponent.cpp
)

add_executable(Indium ${SOURCES})

find_package(raylib REQUIRED)

# Expose the include folder so headers can be found
target_include_directories(Indium PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link Raylib and required system libraries
if(APPLE)
    target_link_libraries(Indium
        raylib
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
else()
    target_link_libraries(Indium
        raylib
        GL m pthread dl rt
    )
endif()