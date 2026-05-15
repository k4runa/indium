<![CDATA[<div align="center">

# Indium

A modular 2D game engine built with C++17, Raylib, and Dear ImGui.

[![License: MIT](https://img.shields.io/badge/License-MIT-white.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Raylib](https://img.shields.io/badge/Raylib-4.x-green.svg)](https://www.raylib.com)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg)]()

</div>

---

Indium is a lightweight 2D engine with an integrated editor. It uses an Entity-Component architecture where behavior is composed through attachable modules rather than deep inheritance trees. The editor provides a real-time viewport, a scene hierarchy, and a property inspector — all rendered through Dear ImGui via rlImGui.

## Features

- **Integrated Editor** — Hierarchy panel, viewport with coordinate-mapped mouse interaction, and a property inspector with per-component UI
- **Entity-Component System** — Entities are containers; logic lives in composable `Component` modules (Rigidbody, Bouncer, etc.)
- **Play / Stop** — Scene snapshot and restore system for non-destructive simulation testing
- **2D Physics** — Gravity, impulse-based collision response, SAT (Separating Axis Theorem) for OBB, and Circle-Polygon narrow-phase detection
- **Entity Primitives** — Circle, Rectangle, and Plane with full rotation, scale, and color support
- **Factory Pattern** — Standardized entity creation with tracked instance counts
- **Dark / Light Themes** — Switchable editor themes with hand-tuned color palettes
- **JSON Configuration** — External `config.json` for window size, FPS target, and title
- **Cross-Platform Build** — CMake-based, tested on Linux and macOS

## Project Structure

```
Indium/
├── src/
│   ├── main.cpp              # Entry point, lifecycle management
│   └── Config.hpp            # JSON config loader
├── core/
│   ├── Entity.hpp            # Base entity with transform, components, and inspector
│   ├── Component.hpp         # Abstract component interface
│   └── scene/
│       └── Scene.hpp         # Entity container, snapshot save/restore
├── 2D/
│   ├── entity/
│   │   ├── Circle.hpp        # Circle primitive
│   │   ├── Rectangle.hpp     # Rectangle primitive with OBB vertices
│   │   ├── Plane.hpp         # Thin surface primitive (floors, walls)
│   │   └── EntityFactory.hpp # Centralized entity creation
│   └── component/
│       ├── RigidbodyComponent.hpp / .cpp   # Gravity, collisions, SAT
│       └── BouncerComponent.hpp            # Velocity-based edge bouncing
├── editor/
│   └── Editor.hpp            # Editor orchestration, UI panels, theming
├── include/                  # Vendored dependencies (ImGui, rlImGui, nlohmann/json)
├── tools/                    # clang-format and clang-tidy runner scripts
├── .github/workflows/        # CI for Ubuntu, macOS, Windows
├── config.json               # Runtime configuration
├── CMakeLists.txt            # Build system
└── run.sh                    # Build and run helper script
```

## Dependencies

| Dependency | Purpose | Included |
|---|---|---|
| [Raylib](https://www.raylib.com) | Rendering, input, windowing | System install required |
| [Dear ImGui](https://github.com/ocornut/imgui) | Editor UI | Vendored in `include/` |
| [rlImGui](https://github.com/raylib-extras/rlImGui) | ImGui-Raylib bridge | Vendored in `include/` |
| [nlohmann/json](https://github.com/nlohmann/json) | Config parsing | Vendored in `include/` |

## Building

### Prerequisites

- CMake 3.10+
- A C++17-compatible compiler (GCC, Clang, or AppleClang)
- Raylib installed and discoverable by CMake (`find_package(raylib)`)

### Build and Run

```bash
./run.sh
```

This will configure CMake, build with all available cores, and launch the engine.

For a clean rebuild:

```bash
./run.sh --clean
```

### Manual Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./Indium
```

## Architecture

```
main.cpp
  └── Editor
        ├── Scene
        │     └── Entity[]
        │           └── Component[]
        ├── Viewport       (off-screen RenderTexture → ImGui panel)
        ├── Hierarchy       (entity list, context menus)
        └── Inspector       (property editing, component management)
```

The main loop separates **Update** (input, physics, component ticks) from **Run** (render world to texture, draw UI). The viewport renders the game world into an off-screen buffer that is then displayed inside an ImGui window, with mouse coordinates mapped back to world space for accurate interaction.

Components follow a strict interface: `update()`, `inspect()`, `clone()`, and `getName()`. The `clone()` method enables the Play/Stop snapshot system — the scene is deep-copied before simulation and restored on stop.

## Configuration

Edit `config.json` in the project root:

```json
{
    "screenWidth": 1920,
    "screenHeight": 1040,
    "targetFps": 300,
    "windowTitle": "Indium - Game Engine",
    "showFps": false
}
```

## License

[MIT](LICENSE)
]]>
