<div align="center">

![Indium Logo](/home/g4lice/.gemini/antigravity/brain/0e017886-7385-422d-a545-c9eeba1f437f/indium_engine_logo_1778945931479.png)

# Indium Engine

### Narrative-First 2D Game Engine

Indium is a feature-rich, modular 2D game engine built with C++20, Raylib, and Dear ImGui. It is designed to provide a unified environment where high-performance native execution meets flexible, narrative-driven game design.

[![License: MIT](https://img.shields.io/badge/License-MIT-white.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform: macOS | Linux](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux-lightgrey.svg)]()

</div>

---

## Unified interface, native power

Indium is more than just a renderer; it's a complete development environment. It provides a comprehensive set of tools designed to let developers focus on gameplay and storytelling without reinventing the core foundations.

### High-Performance Native Scripting
Indium features a robust **Dynamic Scripting API** based on C++20. Scripts are compiled into native shared libraries (`.dylib` on macOS, `.so` on Linux) and hot-reloaded by the engine at runtime. The engine compiles them with the exact toolchain it was built with — Apple clang, clang, or g++ — so iteration is near-instant while maintaining the raw performance of native C++.

### Narrative-First Architecture
At its core, Indium is built for stories. The integrated **StoryState Blackboard** provides a global, persistent space for narrative flags and variables. Combined with event-driven triggers and per-scene authored state, creating complex branching narratives is part of the engine's DNA.

### Visual Editor & Tooling
The Indium Editor offers a professional-grade suite of tools:
- **Scene Hierarchy & Inspector:** Deep-dive into entity properties and component state with real-time editing.
- **Viewport Interaction:** Pixel-accurate mouse mapping, camera panning, and specialized 2.5D depth ordering (Y-Sorting).
- **Asset Management:** An integrated Content Browser for quick access to scenes, textures, and scripts.
- **Integrated Console:** Real-time log capture from both the engine core and your custom scripts.

---

## 🛠 Features at a Glance

- **2D Physics & Collision:** Advanced SAT-based OBB detection, circle-polygon narrow phase, and customizable impulse responses.
- **2.5D Depth Engine:** Automatic Y-Sorting for isometric and top-down perspectives, bringing layers to your 2D worlds.
- **Multi-Scene Workflow:** Seamlessly switch between, create, and manage multiple scenes within a single project.
- **Animation System:** Frame-based sprite animation with a dedicated Animator Component.
- **Safe Lifecycle:** A non-destructive Play/Stop system that uses scene snapshots to ensure your authored state remains pristine during testing.
- **Save/Load System:** Built-in persistence layer for story variables and project state.

---

## Getting Started

### Prerequisites

To build Indium from source, you will need:
- **CMake** 3.10 or higher
- **GCC/Clang** with C++20 support
- **Raylib 5.0+** installed on your system

### Build and Run

The easiest way to get started is using the provided helper script:

```bash
# Build and launch the editor
./run.sh

# Perform a clean rebuild
./run.sh --clean
```

### Manual Compilation

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./Indium
```

---

## 🏗 Project Structure

- **`core/`**: The engine's heart — Script management, Story state, and Scene orchestration.
- **`2D/`**: Specialized modules for physics, primitives, and 2.5D rendering.
- **`editor/`**: The Dear ImGui-based visual toolset and launcher.
- **`include/`**: Vendored dependencies (Dear ImGui, rlImGui, nlohmann/json).

---

## License

Indium is completely free and open-source under the [MIT License](LICENSE).
