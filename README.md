<div align="center">

# Indium Engine

### Narrative-First 2D Game Engine

Indium is a feature-rich, modular 2D game engine built with C++20, Raylib, and Dear ImGui. It provides a unified development environment where high-performance native execution meets flexible, narrative-driven game design.

[![License: MIT](https://img.shields.io/badge/License-MIT-white.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform: macOS | Linux | Windows](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey.svg)]()

</div>

---

## Unified interface, native power

Indium is more than just a renderer; it's a complete development environment. It provides a comprehensive set of tools designed to let developers focus on gameplay and storytelling without reinventing the core foundations.

### High-Performance Native Scripting
Indium features a robust **Dynamic Scripting API** based on C++20. Scripts are compiled into native shared libraries (`.dylib` on macOS, `.so` on Linux, `.dll` on Windows) and hot-reloaded by the engine at runtime. The engine compiles them with the exact toolchain it was built with — Apple clang, clang, or g++ — so iteration is near-instant while maintaining the raw performance of native C++.

The scripting API is modeled after Unity's MonoBehaviour pattern. Scripts expose editable properties to the Inspector via the `IND_PROP` macro without any boilerplate.

### Narrative-First Architecture
At its core, Indium is built for stories. The integrated **StoryState Blackboard** provides a global, persistent space for narrative flags and variables. Combined with per-scene authored state and a multi-slot **Save/Load system**, creating complex branching narratives is part of the engine's DNA.

### Visual Editor & Tooling
The Indium Editor offers a professional-grade suite of tools:
- **Scene Hierarchy & Inspector:** Deep-dive into entity properties and component state with real-time editing and full undo/redo support.
- **Viewport Interaction:** Pixel-accurate mouse mapping, camera panning, and specialized 2.5D depth ordering (Y-Sorting).
- **Asset Management:** An integrated Content Browser for quick access to scenes, textures, scripts, and prefabs.
- **Integrated Console:** Real-time log capture from both the engine core and your custom scripts.
- **Play / Pause / Stop:** Non-destructive simulation using scene snapshots — your authored state is never overwritten during testing.

---

## Features at a Glance

### Physics & Collision
- **SAT-Based OBB Detection:** Accurate narrow-phase polygon-polygon and circle-polygon collision.
- **Impulse Resolution:** Restitution (bounciness), mass ratios, and angular impulse per collision.
- **Baumgarte Stabilization:** Position correction to eliminate penetration jitter.
- **Sleep State:** Bodies with negligible velocity are suspended automatically to save CPU.
- **Spatial Grid Broad-Phase:** O(N) collision candidate lookup instead of O(N²) brute force.
- **32 Collision Layers:** Per-body layer assignment and bitmask filtering.
- **Collision & Trigger Callbacks:** `OnCollisionEnter2D` / `Stay` / `Exit` and `OnTriggerEnter2D` / `Exit` dispatched directly to scripts.
- **Physics Queries:** `Raycast`, `OverlapCircle`, and `OverlapBox` callable from any script.

### Scripting API
Scripts derive from `NativeScript` and override lifecycle hooks:

| Hook | When it runs |
|---|---|
| `OnAwake` | Once, before the first frame |
| `OnStart` | Once, on the first frame |
| `OnUpdate(dt)` | Every frame |
| `OnFixedUpdate(fixedDt)` | Every fixed physics step (60 Hz) |
| `OnLateUpdate(dt)` | After all updates |
| `OnDraw` | During the draw phase (world space) |
| `OnGUI` | Screen-space UI pass, each frame in Play/Pause (after the world is drawn) |
| `OnDestroy` | When the entity is destroyed |

Additional script capabilities:
- **Coroutines:** `StartCoroutine(task)` with `co_await WaitForSeconds{n}`, `WaitForFrames{n}`, and `WaitUntil{fn}`.
- **Entity Management:** `Spawn<T>()`, `Destroy()`, `FindByName()`, `FindById()`.
- **Component Queries:** `GetComponent<T>()`, `AddComponent<T>()`, `FindObjectOfType<T>()`, `FindObjectsOfType<T>()`.
- **Scene Transitions:** `LoadScene("level2")` — transitions at end of frame.
- **Prefab Instantiation:** `InstantiatePrefab("enemy")` — spawns from a saved `.prefab` file.
- **Input:** `InputManager::Get().IsActionPressed("jump")` with JSON-configured key/mouse bindings.
- **Runtime UI:** override `OnGUI()` to draw a screen-space HUD / menu with `GUI::Box`, `GUI::Label`, `GUI::Button`, `GUI::Image`, sized via `Screen::Width()` / `Screen::Height()`.
- **Application control:** `Quit()` immediately exits.

### Built-in Components

| Component | Description |
|---|---|
| `RigidbodyComponent` | Physics body: Dynamic / Kinematic / Static. Gravity, drag, mass, bounciness, angular velocity. |
| `BoxCollider2D` | Rectangle collision shape with offset and OBB support. |
| `CircleCollider2D` | Circle collision shape. Circle-polygon narrow phase included. |
| `TriggerComponent` | Non-physical overlap zone; fires `OnTriggerEnter2D` / `Exit` on scripts. |
| `CameraComponent` | Attaches the game camera to an entity for smooth following. |
| `AnimatorComponent` | Frame-based sprite sheet animation with configurable speed and looping. |
| `SpriteRendererComponent` | Renders a texture on an entity with tint and scaling. |
| `ShapeRendererComponent` | Draws a solid rectangle or circle primitive using the entity's color and scale. |
| `TextRendererComponent` | Renders a text string in world space; supports custom TTF/OTF fonts, size, letter spacing, alignment, and color. |
| `AudioSourceComponent` | Plays audio files. Two modes: **Sound** (in-memory SFX, can overlap) and **Music** (disk-streamed for long tracks). |
| `ParticleSystemComponent` | CPU particle emitter. Configurable shape (Point / Circle / Rectangle), lifetime, velocity, color gradient, and size over lifetime. |
| `TilemapComponent` | Grid-based tilemap renderer. Loads a tileset texture and renders a `cols × rows` tile grid. Tile index -1 is empty. |
| `BouncerComponent` | Simple wall-bounce behaviour; keeps entities within world bounds. |
| `InteractableComponent` | Marks an entity as interactable: prompt, radius, and an optional flag (set or **toggle on/off**) / dialogue / event fired on interact. |
| `PlayerInteractorComponent` | Attach to the player; shows the nearest interactable's prompt and triggers it on the Interact action (falls back to `E`). |

### Scene System
- **Multi-Scene Workflow:** Create, open, and switch between scenes from the editor.
- **Runtime Scene Loading:** `LoadScene("name")` transitions at end of frame from any script.
- **Y-Sorting:** Automatic depth ordering for isometric and top-down perspectives.
- **Entity Hierarchy:** Parent/child transforms with recursive position, rotation, and scale propagation.
- **Prefab System:** Save any entity as a `.prefab` file and instantiate it at runtime via scripts or the editor.

### Narrative & Persistence
- **StoryState Blackboard:** Global key-value store (bool, int, float, string) for narrative flags and game variables. Authored per-scene starting values that are seeded when Play begins.
- **Dialogue System:** Branching dialogue graphs authored as `dialogue/<name>.json` and run at runtime via `DialogueManager::Get().Start("name")`. Choices read/write StoryState (`requireFlag` / `setFlag`) and fire `NarrativeEvent`s; the box renders in the screen-space UI pass.
- **Interaction:** `InteractableComponent` + `PlayerInteractorComponent` provide "press to interact" prompts that set flags, start dialogues, or fire events — complementing the automatic `TriggerComponent` zones.
- **Quest System:** Data-driven quests authored as `quests/<id>.json`, with progress stored in StoryState — so it saves/loads and seeds per-scene for free. Sequential or parallel objectives; quests advance automatically when dialogue/interaction set objective flags, or are driven directly from scripts via `QuestManager::Get().Start("id")`. An editor **Quests** panel authors and live-debugs them.
- **Save/Load Manager:** Slot-based persistence (`slot_0.json`, `slot_1.json`, …) stored alongside the project. Saves and restores the full StoryState.

---

## Getting Started

### Prerequisites

To build Indium from source, you will need:
- **CMake** 3.10 or higher
- **GCC / Clang / MSVC** with C++20 support
- **Raylib 5.0+** installed on your system

### Build and Run

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

### Running Tests

```bash
mkdir build && cd build
cmake ..
make IndiumTests -j$(nproc)
ctest --output-on-failure
```

---

## Writing a Script

```cpp
#include "NativeScript.hpp"

class Player : public Indium::NativeScript
{
    IND_PROP(float, speed, 200.0f);
    IND_PROP(bool,  doubleJump, false);

    REGISTER_SCRIPT(Player)

    void OnStart() override
    {
        StartCoroutine(IntroSequence());
    }

    void OnUpdate(float dt) override
    {
        if (InputManager::Get().IsActionPressed("jump"))
            entity->velocity.y = -400.0f;

        entity->position.x += InputManager::Get().GetAxis("horizontal") * speed * dt;
    }

    void OnCollisionEnter2D(Entity* other) override
    {
        if (other->name == "Coin")
        {
            StoryState::Get().Set("coins", StoryState::Get().GetInt("coins") + 1);
            Destroy(other);
        }
    }

    CoroutineTask IntroSequence()
    {
        co_await Indium::WaitForSeconds{1.5f};
        auto* txt = GetComponent<TextRendererComponent>();
        if (txt) txt->text = "Go!";
    }
};

INDIUM_EXPORT_SCRIPTS(Player)
```

---

## Authoring Dialogue

Dialogue lives in `dialogue/<name>.json` and runs via `DialogueManager::Get().Start("name")` — from a script, or automatically by setting an `InteractableComponent`'s **Dialogue Id**:

```json
{
  "start": "greet",
  "nodes": {
    "greet": {
      "speaker": "Alice",
      "text": "Have we met before?",
      "choices": [
        { "text": "In the village.",   "next": "remember", "requireFlag": "met_in_village" },
        { "text": "I don't think so.",  "next": "intro",    "setFlag": "denied_meeting" },
        { "text": "[Leave]",            "next": "" }
      ]
    },
    "intro":    { "speaker": "Alice", "text": "Well met. I'm Alice.", "next": "" },
    "remember": { "speaker": "Alice", "text": "Good to see you again!", "next": "" }
  }
}
```

- A node with **no visible choices** is narration — advance with **Space / Enter / click**; choice nodes accept **mouse or number keys 1–9**.
- `requireFlag` hides a choice until that StoryState flag is set; `setFlag` sets a flag when chosen (and fires a `NarrativeEvent`).
- `next: ""` ends the dialogue. The box is drawn by the engine in the screen-space UI pass during Play.

---

## Authoring Quests

Quests live in `quests/<id>.json` and track progress in the StoryState blackboard, so they persist through Save/Load automatically. Start one from a script with `QuestManager::Get().Start("find_sword")`, or set `"autoStart": true`.

```json
{
  "id": "find_sword",
  "title": "The Lost Blade",
  "mode": "sequential",
  "objectives": [
    { "id": "talk_smith", "desc": "Speak to the blacksmith",    "completeFlag": "talked_to_smith" },
    { "id": "get_blade",  "desc": "Find the blade in the cave", "completeFlag": "has_blade" }
  ],
  "completeFlag": "quest_find_sword_done",
  "rewards": ["gold_reward_given"]
}
```

- An **objective** completes when its `completeFlag` becomes true in StoryState — which dialogue choices (`setFlag`) and `InteractableComponent`s already do, so quests advance with no extra glue. Scripts can also call `CompleteObjective(id, objId)`, `Advance(id)`, or `Complete(id)`.
- `"mode": "sequential"` completes objectives in order; `"parallel"` completes them in any order. When a quest finishes it sets its `completeFlag` (which can gate other quests or dialogue) and any `rewards`.
- Progress lives under the `quest.<id>.*` StoryState keys. The editor's **Quests** panel lists definitions, shows live state during Play, and can start / advance / complete them for testing.

---

## Authoring Cutscenes

Cutscenes are keyframed timelines that move entities, animate the camera, and fire dialogue / audio / events over time — the data-driven alternative to hand-written coroutines for scripted story moments. They live in `cutscenes/<name>.json` and run via `CutsceneManager::Get().Play("name")` — from a script, or automatically by setting an `InteractableComponent`'s **Cutscene Id**.

```json
{
  "duration": 4.0,
  "loop": false,
  "pausesGameplay": true,
  "onCompleteFlag": "intro_seen",
  "tracks": [
    { "type": "Camera", "target": "",
      "keys": [
        { "t": 0.0, "pos": [0, 0],   "zoom": 1.0, "ease": "EaseInOut" },
        { "t": 3.0, "pos": [200, 0], "zoom": 1.4, "ease": "EaseInOut" }
      ] },
    { "type": "Transform", "target": "NPC_Alice", "channels": ["position"],
      "keys": [
        { "t": 0.0, "pos": [100, 200], "ease": "Linear" },
        { "t": 2.0, "pos": [400, 200], "ease": "EaseOut" }
      ] },
    { "type": "Dialogue",  "events": [ { "t": 1.0, "a": "intro" } ] },
    { "type": "StoryFlag", "events": [ { "t": 3.8, "a": "alice_arrived", "fireOnSkip": true } ] }
  ]
}
```

- There are two kinds of track. **Interpolated** tracks — `Transform` (moves an entity; `channels` picks `position`/`rotation`/`scale`) and `Camera` (drives the primary camera's look-at + `zoom`; an empty `target` binds the primary camera) — are sampled every frame with per-key `ease` (`Linear`/`EaseIn`/`EaseOut`/`EaseInOut`/`Step`). **Trigger** tracks — `Dialogue`, `Audio`, `Animation`, `Activation`, `StoryFlag`, `Event`, `Particle` — fire once when the playhead crosses an event's time. An event's `a` payload is type-specific: a dialogue id, `"play"`/`"stop"`, a clip name, `"show"`/`"hide"`, a flag, or a NarrativeEvent tag.
- Tracks bind to entities **by name** (or tag); the binding resolves when the cutscene plays. The player advances on real (unscaled) time, so `pausesGameplay` can freeze the scene (`Time::scale = 0`) while the cutscene keeps running. On completion it can set `onCompleteFlag` / publish `onCompleteEvent` (so quests and dialogue can react). Pressing **Esc** skips: interpolated tracks snap to their final values and only `fireOnSkip` triggers fire, so story state stays consistent.
- The editor's **Cutscenes** panel is a visual timeline: add tracks, bind targets, and drag keyframe diamonds / event markers on a zoomable ruler. Pose an entity in the viewport and **Add Key at playhead** captures its transform. Drag the playhead (or press ▶) to **scrub a non-destructive preview** in the editor; in Play, ▶ runs the real cutscene with a cinematic letterbox.

---

## Project Structure

```
Indium/
├── src/             Entry point (main.cpp) and Config
├── core/            Engine core
│   ├── scene/       Scene container, snapshot, physics step
│   ├── spatial/     SpatialGrid for broad-phase collision
│   ├── Entity.hpp   Base ECS entity
│   ├── NativeScript.hpp  Scripting base class + macros
│   ├── StoryState.hpp    Narrative blackboard
│   ├── SaveManager.hpp   Slot-based save/load
│   ├── InputManager.hpp  Action-based input
│   ├── PrefabManager.hpp Prefab read/write
│   ├── ScriptManager.hpp Hot-reload: compiles .so/.dll via dlopen/LoadLibrary
│   ├── AssetManager.hpp  Texture deduplication cache
│   ├── Coroutine.hpp     C++20 coroutine support
│   └── Time.hpp          Delta time utilities
├── 2D/
│   ├── entity/      Circle, Rectangle, Plane primitives + EntityFactory
│   └── component/   All built-in components (see table above)
├── editor/          Dear ImGui editor, launcher, and inspector
├── tools/           File browser and helper utilities
├── include/         Vendored: Dear ImGui, rlImGui, nlohmann/json, FontAwesome
└── tests/           Entity and physics unit tests
```

---

## License

Indium is completely free and open-source under the [MIT License](LICENSE).
