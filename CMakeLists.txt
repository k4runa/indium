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
    editor/panels/QuestPanel.cpp
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
    $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>
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

# Test-only sources live in tests/ and are not present in every checkout.
# Guard the test target on their presence so a missing test file warns and
# skips IndiumTests instead of failing configure (which would also block the
# main Indium build).
set(TEST_ONLY_SOURCES
    tests/entity_test.cpp
    tests/engine_tests.cpp
    tests/quest_test.cpp
    tests/main.cpp
)

set(_indium_tests_available TRUE)
set(_indium_missing_tests "")
foreach(_test_src IN LISTS TEST_ONLY_SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_test_src}")
        set(_indium_tests_available FALSE)
        list(APPEND _indium_missing_tests "${_test_src}")
    endif()
endforeach()

if(_indium_tests_available)
    set(TEST_SOURCES
        include/rlImGui.cpp
        include/imgui.cpp
        include/imgui_draw.cpp
        include/imgui_widgets.cpp
        include/imgui_tables.cpp
        include/imgui_demo.cpp

        ${TEST_ONLY_SOURCES}
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
else()
    message(WARNING
        "Indium: test sources missing (${_indium_missing_tests}) — skipping the "
        "IndiumTests target. The main Indium build is unaffected.")
endif()

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
    # Remove the console window so double-clicking the exe opens only the
    # raylib window (no black cmd.exe behind it).
    if(MINGW)
        target_link_options(Indium PRIVATE -mwindows)
    endif()
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
