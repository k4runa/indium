cmake_minimum_required(VERSION 3.10)

project(Indium VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Source files: main + rlImGui + ImGui core files
set(SOURCES
    src/main.cpp
    include/rlImGui.cpp
    include/imgui.cpp
    include/imgui_draw.cpp
    include/imgui_widgets.cpp
    include/imgui_tables.cpp
    include/imgui_demo.cpp
    2D/component/RigidbodyComponent.cpp
)

add_executable(Indium ${SOURCES})
set_target_properties(Indium PROPERTIES ENABLE_EXPORTS TRUE)

find_package(raylib REQUIRED)

# --- Script toolchain (baked in for the runtime script compiler) ---
# ScriptManager compiles user scripts out-of-process. It must reuse the exact
# compiler CMake picked (ABI compatibility) and know where raylib's headers
# live (Homebrew installs them outside the default search path on macOS).
target_compile_definitions(Indium PRIVATE
    INDIUM_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
)

get_target_property(_indium_raylib_includes raylib INTERFACE_INCLUDE_DIRECTORIES)
if(_indium_raylib_includes)
    list(GET _indium_raylib_includes 0 _indium_raylib_inc0)
    target_compile_definitions(Indium PRIVATE
        INDIUM_RAYLIB_INCLUDE_DIR="${_indium_raylib_inc0}"
    )
endif()

# Expose the include folders so headers can be found
target_include_directories(Indium PRIVATE
    include
    core
    2D
    2D/component
    2D/entity
    src
    .
)

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
        GL
        m
        pthread
        dl
        rt
    )
endif()
