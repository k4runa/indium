cmake_minimum_required(VERSION 3.10)

project(Indium VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Source files: main + rlImGui + ImGui core files
set(SOURCES
    src/main.cpp
    include/rlImGui.cpp
    editor/Editor.cpp
    editor/panels/ConsolePanel.cpp
    editor/panels/StoryStatePanel.cpp
    editor/panels/InputManagerPanel.cpp
    editor/panels/InspectorPanel.cpp
    editor/panels/ContentBrowserPanel.cpp
    editor/panels/MainMenuBar.cpp
    editor/panels/HierarchyPanel.cpp
    editor/panels/ViewportPanel.cpp
    editor/panels/EditorTheme.cpp
    editor/actions/EditorActions.cpp
    include/imgui.cpp
    include/imgui_draw.cpp
    include/imgui_widgets.cpp
    include/imgui_tables.cpp
    include/imgui_demo.cpp
    2D/component/RigidbodyComponent.cpp
    2D/component/AnimatorComponent.cpp
    2D/component/CameraComponent.cpp
    2D/component/TilemapComponent.cpp
    2D/component/ParticleSystemComponent.cpp
    2D/component/TriggerComponent.cpp
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

enable_testing()

set(TEST_SOURCES

    include/rlImGui.cpp
    include/imgui.cpp
    include/imgui_draw.cpp
    include/imgui_widgets.cpp
    include/imgui_tables.cpp
    include/imgui_demo.cpp

    tests/entity_test.cpp
    tests/engine_tests.cpp
    tests/main.cpp
    2D/component/RigidbodyComponent.cpp
    2D/component/AnimatorComponent.cpp
    2D/component/CameraComponent.cpp
    2D/component/TilemapComponent.cpp
    2D/component/ParticleSystemComponent.cpp
    2D/component/TriggerComponent.cpp
)

add_executable(IndiumTests ${TEST_SOURCES})

target_include_directories(IndiumTests PRIVATE include core 2D 2D/component 2D/entity src tests .)
if(APPLE)
    target_link_libraries(IndiumTests raylib "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
elseif(WIN32)
    target_link_libraries(IndiumTests raylib opengl32 gdi32 winmm)
else()
    target_link_libraries(IndiumTests raylib GL m pthread dl rt)
endif()

add_test(NAME indium_tests COMMAND IndiumTests)

# Link Raylib and required system libraries
if(APPLE)
    target_link_libraries(Indium
        raylib
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(Indium
        raylib
        opengl32
        gdi32
        winmm
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
