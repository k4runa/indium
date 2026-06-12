# Indium Scripting API Reference

This is the reference for Indium's C++ scripting API — everything you can call from a script that derives from `Indium::NativeScript`. For a feature overview, build instructions, and the JSON authoring formats (dialogue, quests, items, cutscenes), see the [README](../README.md).

All types live in the `Indium` namespace. Scripts are hot-reloaded native code: the engine compiles your `.cpp` into a shared library and loads it at runtime, so everything below is plain C++20 with zero binding overhead.

---

## Table of Contents

- [Anatomy of a Script](#anatomy-of-a-script)
- [Lifecycle Hooks](#lifecycle-hooks)
- [Collision & Trigger Callbacks](#collision--trigger-callbacks)
- [Entity](#entity)
- [NativeScript Helpers](#nativescript-helpers)
- [Coroutines](#coroutines)
- [StoryState — the Narrative Blackboard](#storystate--the-narrative-blackboard)
- [Story Expressions & Interpolation](#story-expressions--interpolation)
- [InputManager](#inputmanager)
- [DialogueManager](#dialoguemanager)
- [QuestManager](#questmanager)
- [ItemManager](#itemmanager)
- [CutsceneManager](#cutscenemanager)
- [SaveManager](#savemanager)
- [MenuManager & GameSettings](#menumanager--gamesettings)
- [AudioMixer](#audiomixer)
- [EventBus & Game Events](#eventbus--game-events)
- [Time & Screen](#time--screen)
- [Runtime GUI](#runtime-gui)
- [Logger](#logger)

---

## Anatomy of a Script

```cpp
#include "NativeScript.hpp"

class Player : public Indium::NativeScript
{
    IND_PROP(float, speed, 200.0f);   // editable in the Inspector, serialized with the scene
    IND_PROP(bool,  canJump, true);

    REGISTER_SCRIPT(Player)            // registers the class with the engine

    void OnUpdate(float dt) override
    {
        if (InputManager::Get().IsDown("right"))
            entity->position.x += speed * dt;
    }
};

INDIUM_EXPORT_SCRIPTS(Player)          // once per file, lists every script class in it
```

### Macros

| Macro | Purpose |
|---|---|
| `IND_PROP(type, name, default)` | Declares a member variable that appears in the Inspector and serializes with the scene. Supported types: `float`, `int`, `bool`, `std::string`, `Vector2`, `Color`. |
| `REGISTER_SCRIPT(ClassName)` | Registers the class so the engine can instantiate it by name. Place inside the class body. |
| `INDIUM_EXPORT_SCRIPTS(...)` | Exports the registered scripts from the compiled library. Place once at file scope, after all script classes. |

### The `entity` pointer

Every script has an `entity` member — a typed alias for the owning `Entity*`. See [Entity](#entity) for its fields.

---

## Lifecycle Hooks

Override any of these; all are optional.

| Hook | When it runs |
|---|---|
| `void OnAwake()` | Once, before the first frame. |
| `void OnStart()` | Once, on the first frame. |
| `void OnUpdate(float dt)` | Every frame. `dt` is already scaled by `Time::scale`. |
| `void OnFixedUpdate(float fixedDt)` | Every fixed physics step (60 Hz). |
| `void OnLateUpdate(float dt)` | After all scripts' `OnUpdate`. |
| `void OnDraw() const` | During the world-space draw phase. |
| `void OnGUI()` | Screen-space UI pass, each frame in Play/Pause, after the world is drawn. Draw HUDs/menus here with the [GUI helpers](#runtime-gui). |
| `void OnDestroy()` | When the entity is destroyed (also stops the script's coroutines). |

> **Scene context:** the scene-touching helpers (`Spawn`, `Destroy`, `FindByName`, `LoadScene`, physics queries, …) only work *inside* lifecycle hooks and collision/trigger callbacks. Called from anywhere else (e.g. a constructor), they return `nullptr` / silently no-op because no scene is bound.

---

## Collision & Trigger Callbacks

Dispatched by the physics step. The scene context is bound inside these, so all helpers work.

| Callback | Fires |
|---|---|
| `void OnCollisionEnter2D(Entity* other)` | First frame two non-trigger rigidbodies begin overlapping. |
| `void OnCollisionStay2D(Entity* other)` | Every physics step they remain overlapping. |
| `void OnCollisionExit2D(Entity* other)` | First frame they stop overlapping. |
| `void OnTriggerEnter2D(Entity* other)` | Another entity enters a `TriggerComponent` attached to this entity. |
| `void OnTriggerExit2D(Entity* other)` | Another entity exits that trigger. |

---

## Entity

Public fields you'll touch from scripts:

| Field | Type | Meaning |
|---|---|---|
| `id` | `int` | Unique scene-wide id. |
| `name` | `std::string` | Display name; used by `FindByName` and cutscene binding. |
| `tag` | `std::string` | Tag (default `"Untagged"`); editable set lives in `TagRegistry`. |
| `layer` | `int` | Collision layer 0–31, used with layer masks. |
| `isActive` | `bool` | Inactive entities don't update, draw, or collide. Prefer `setActive()`. |
| `position` | `Vector2` | Local position (world position if no parent). |
| `scale` | `Vector2` | Local scale. |
| `rotation` | `float` | Local rotation in degrees. |
| `velocity` | `Vector2` | Linear velocity, read/written by physics. |
| `sortingOrder` | `int` | Draw-order tiebreak within a depth layer. |
| `depthLayer` | `int` | Coarse depth layer for 2.5D ordering. |
| `parent` | `Entity*` | Parent in the hierarchy (transforms propagate). Prefer `setParent()`. |

Key methods:

```cpp
template<typename T> T*  getComponent();          // first component of type T, or nullptr
template<typename T> bool hasComponent() const;
template<typename T, typename... Args>
T*  addComponent(Args&&... args);                 // construct + attach (see also NativeScript::AddComponent)
void setActive(bool active);                      // toggles the whole hierarchy's effective active state
void setParent(Entity* newParent);
Vector2 getGlobalPosition() const;                // parent-chain-resolved world transform
Vector2 getGlobalScale() const;
float   getGlobalRotation() const;
```

---

## NativeScript Helpers

All of these are methods on `NativeScript`, callable as plain functions inside your script.

### Components

```cpp
template<typename T> T* GetComponent();        // component of type T on this entity, or nullptr
template<typename T> T* AddComponent();        // add to this entity at runtime (calls the component's start)
template<typename T> T* FindObjectOfType();    // first component of type T anywhere in the scene
template<typename T> std::vector<T*> FindObjectsOfType();  // all of them
```

### Entities & scene

```cpp
void    Destroy();                              // schedule this entity for destruction (end of frame)
void    Destroy(Entity* target);                // schedule any entity
template<typename T>
T*      Spawn(const std::string& name = "",
              Vector2 position = {0,0},
              float rotation = 0.0f);           // spawn a new entity (e.g. Spawn<Circle>("Bullet"))
Entity* InstantiatePrefab(const std::string& name);  // from <project>/prefabs/<name>.prefab
Entity* FindByName(const std::string& name) const;
Entity* FindById(int id) const;
Entity* GetMainCamera() const;                  // first entity with a CameraComponent
Scene*  GetScene() const;
void    LoadScene(const std::string& sceneName); // transition at end of frame; name without path/extension
static void Quit();                             // immediately exits the application
```

### Physics queries

`layerMask` is a bitmask over collision layers; `-1` (default) matches everything.

```cpp
RaycastHit2D Raycast(Vector2 origin, Vector2 direction, float maxDist, int layerMask = -1) const;
std::vector<Entity*> OverlapCircle(Vector2 center, float radius, int layerMask = -1) const;
std::vector<Entity*> OverlapBox(Vector2 center, Vector2 size, int layerMask = -1) const;
```

`RaycastHit2D` converts to `bool` (did it hit?) and carries:

```cpp
Entity* entity;    // what was hit (nullptr = miss)
Vector2 point;     // world-space hit point
Vector2 normal;    // surface normal at the hit
float   distance;  // distance from origin
```

```cpp
if (auto hit = Raycast(entity->position, {1, 0}, 500.0f))
    Logger::Event("Game", "hit %s at distance %.1f", hit.entity->name.c_str(), hit.distance);
```

---

## Coroutines

C++20 coroutines for multi-frame sequences without manual state machines.

```cpp
void StartCoroutine(CoroutineTask task);
void StopAllCoroutines();
int  CoroutineCount() const;
```

A coroutine is a member function returning `CoroutineTask` that uses `co_await` with one of three awaitables:

| Awaitable | Resumes |
|---|---|
| `WaitForSeconds{ float }` | After N seconds of scaled game time. |
| `WaitForFrames{ int }` | After N frames. |
| `WaitUntil{ std::function<bool()> }` | When the predicate first returns true. |

```cpp
CoroutineTask DoorSequence()
{
    co_await Indium::WaitForSeconds{1.0f};
    co_await Indium::WaitUntil{[] { return StoryState::Get().HasFlag("has_key"); }};
    if (auto* door = FindByName("Door")) door->setActive(false);
}
```

Coroutines are owned by the script: they stop automatically when the entity is destroyed.

---

## StoryState — the Narrative Blackboard

`StoryState::Get()` is a global key/value store (`bool`, `int`, `float`, `std::string`) that survives scene switches. Per-scene authored values seed into it on Play start and on every scene switch; runtime values are discarded when Play stops. Saves serialize it in full.

```cpp
void  Set(const std::string& key, StoryValue value);   // StoryValue = variant<bool,int,float,string>
bool  Has(const std::string& key) const;
void  Remove(const std::string& key);

bool        GetBool  (const std::string& key, bool def = false) const;
int         GetInt   (const std::string& key, int def = 0) const;
float       GetFloat (const std::string& key, float def = 0.0f) const;
std::string GetString(const std::string& key, const std::string& def = "") const;

// Boolean-flag convenience
void SetFlag  (const std::string& name);   // Set(name, true)
void ClearFlag(const std::string& name);   // Set(name, false)
bool HasFlag  (const std::string& name) const;
```

Every `Set`/`Remove` publishes a `StoryStateChangedEvent` (see [EventBus](#eventbus--game-events)). `NarrativeEvent` tags fired by any system are automatically recorded as flags.

Reserved key prefixes maintained by other systems:

| Prefix | Owner | Example |
|---|---|---|
| `quest.<id>.state`, `quest.<id>.step` | QuestManager | `quest.find_sword.state` |
| `item.<id>` | ItemManager (player inventory counts) | `item.gold` |

---

## Story Expressions & Interpolation

Free functions in `StoryState.hpp`, used by dialogue `requireFlag`, quest `completeWhen`, and available to scripts directly:

```cpp
bool StoryEval(const std::string& expr, const StoryState& st = StoryState::Get());
std::string StoryInterpolate(const std::string& src, const StoryState& st = StoryState::Get());
```

**`StoryEval` grammar** — empty string is true (no gate); a bare identifier means `GetBool(key)`; otherwise:

- Comparisons: `==` `!=` `<` `<=` `>` `>=` against numbers, `true`/`false`, `"quoted strings"`, or barewords — e.g. `item.gold >= 10`, `chapter == 3`.
- Boolean logic: `&&`, `||`, `!`, parentheses — e.g. `met_alice && (item.key >= 1 || lockpicked)`.
- Never throws: unknown keys read false, malformed literals compare false.

**`StoryInterpolate`** replaces `{key}` tokens with the formatted StoryState value (`"You have {item.gold} gold."`). Unknown keys are left literal so typos stay visible; `{{`/`}}` emit literal braces.

---

## InputManager

Action-based input. Bindings are configured in the editor's Input Manager panel (or `input.json`) and rebindable in the in-game settings menu.

```cpp
auto& in = InputManager::Get();

bool IsPressed (const std::string& action) const;   // went down this frame
bool IsDown    (const std::string& action) const;   // held
bool IsReleased(const std::string& action) const;   // went up this frame

// Binding management (the editor and settings menu use these; scripts rarely need them)
void SetAction     (const std::string& name, int key);   // raylib key code
void SetMouseAction(const std::string& name, int btn);   // raylib mouse button
void RemoveAction  (const std::string& name);
bool HasAction     (const std::string& name) const;
void Load(const std::string& path);
void Save(const std::string& path) const;
```

There is no axis API; compose movement from two actions:

```cpp
float x = (in.IsDown("right") ? 1.0f : 0.0f) - (in.IsDown("left") ? 1.0f : 0.0f);
entity->position.x += x * speed * dt;
```

---

## DialogueManager

Runs branching dialogue authored in `dialogue/<name>.json` (format: see [README — Authoring Dialogue](../README.md#authoring-dialogue)). The engine draws the dialogue box in the screen-space UI pass and handles input — most games only ever call `Start`.

```cpp
auto& dm = DialogueManager::Get();

bool Start(const std::string& name);   // begin dialogue/<name>.json; false if missing/invalid
bool IsActive() const;                 // a dialogue is on screen
void Advance();                        // advance a narration node (engine calls this on Space/Enter/click)
void Choose(int visibleIndex);         // pick a visible choice (engine calls this on click / number keys)
void End();                            // force-close
```

---

## QuestManager

Data-driven quests from `quests/<id>.json` (format: see [README — Authoring Quests](../README.md#authoring-quests)). Progress lives in StoryState under `quest.<id>.*`, so it saves/loads for free. Quests advance automatically when their objectives' flags/conditions become true; the calls below are for driving them explicitly.

```cpp
auto& qm = QuestManager::Get();

void Start(const std::string& id);
void CompleteObjective(const std::string& id, const std::string& objectiveId);
void Advance(const std::string& id);     // complete the current objective (sequential mode)
void Complete(const std::string& id);    // finish the whole quest (fires completeFlag + rewards)
void Abandon(const std::string& id);

QuestState StateOf(const std::string& id) const;   // NotStarted / Active / Complete
bool IsActive  (const std::string& id) const;
bool IsComplete(const std::string& id) const;
int  CurrentStep(const std::string& id) const;     // index of the active objective

bool LogOpen() const;                    // the in-game quest-log overlay
void SetLogOpen(bool open);
```

---

## ItemManager

Item definitions from `items/<id>.json` (format: see [README — Authoring Items](../README.md#authoring-items)). The player inventory is counts in StoryState under `item.<id>` — items with no definition act as unlimited counters (handy for `gold`/`score`).

```cpp
auto& im = ItemManager::Get();

void Give(const std::string& id, int n = 1);       // clamped to the item's stack cap
bool Take(const std::string& id, int n = 1);       // false (and no change) if fewer than n held
bool Has (const std::string& id, int n = 1) const;
int  Count(const std::string& id) const;
void SetCount(const std::string& id, int n);
void Remove(const std::string& id);                // take all

bool PanelOpen() const;                            // the in-game inventory overlay
void SetPanelOpen(bool open);                      // also toggled by the "Inventory" action (default I)
```

Non-player containers (chests, NPC stock) hold their own stacks via `InventoryComponent` instead.

---

## CutsceneManager

Plays keyframed timelines from `cutscenes/<name>.json` (format: see [README — Authoring Cutscenes](../README.md#authoring-cutscenes)). Cutscenes advance on real (unscaled) time, so a cutscene with `pausesGameplay` keeps running while the scene is frozen.

```cpp
auto& cm = CutsceneManager::Get();

bool  Play(const std::string& name);   // begin cutscenes/<name>.json; false if missing/invalid
void  Pause();
void  Resume();
void  Skip();                          // what Esc does: snap to final values, fire fireOnSkip triggers
void  End();                           // stop immediately
bool  IsActive() const;                // playing or paused
bool  IsPlaying() const;               // playing and not paused
float Time() const;                    // playhead in seconds
```

On completion a cutscene can set `onCompleteFlag` / publish `onCompleteEvent`, so quests and dialogue can react without any script glue.

---

## SaveManager

Slot-based persistence (`saves/slot_<n>.json` alongside the project). A save captures the full StoryState; loading restores it. All methods are static.

```cpp
bool SaveManager::Save(const Scene& scene, int slot = 0);
bool SaveManager::Load(Scene& scene, int slot = 0);
bool SaveManager::SlotExists(int slot = 0);
bool SaveManager::DeleteSlot(int slot = 0);
```

From a script, get the scene via `GetScene()`:

```cpp
if (auto* s = GetScene()) SaveManager::Save(*s, 0);
```

---

## MenuManager & GameSettings

The built-in player-facing front end: title screen, pause menu, and settings (audio buses, key rebinding). The engine wires Esc and draws the menus in the UI pass; scripts mostly query state or push pages.

```cpp
auto& mm = MenuManager::Get();

void OpenTitle();
void OpenPause();
void OpenSettings();
void Resume();                       // close whatever is open
bool IsOpen() const;
bool BlocksGameplay() const;         // true while any page is open — gate your gameplay input on this
MenuManager::Page Current() const;   // None / Title / Pause / Settings
void SetTitle(const std::string& t); // title-screen heading (defaults to "Indium")
```

`GameSettings::Get()` persists menu-changed settings (volumes, bindings) to the project; it loads automatically and saves when dirty — scripts normally don't touch it.

---

## AudioMixer

Named volume buses; `AudioSourceComponent`s route through a bus and `"master"` scales everything.

```cpp
auto& mix = AudioMixer::Get();

float BusVolume(const std::string& bus) const;      // 0..1, unknown bus = 1
void  SetBusVolume(const std::string& bus, float v);
float Effective(const std::string& bus) const;      // bus volume × master
```

The settings menu exposes `master`, `music`, and `sfx` sliders backed by these.

---

## EventBus & Game Events

A global publish/subscribe bus, shared across the script-library boundary. Subscriptions are RAII: keep the returned `SubscriptionHandle` alive (e.g. as a script member); destroying it unsubscribes.

```cpp
// namespace Indium::Events
template<typename T> SubscriptionHandle Subscribe(std::function<void(const T&)> handler);
template<typename T> void Publish(const T& event);
```

Built-in events (`core/events/GameEvents.hpp`, namespace `GameEvents`):

| Event | Payload | Fired when |
|---|---|---|
| `CollisionEvent` | `Entity* a, b` | Two rigidbodies collide. |
| `TriggerEnterEvent` / `TriggerExitEvent` | `Entity* trigger, other` | Trigger overlap begins / ends. |
| `GameStartEvent` / `GameStopEvent` | — | Play starts / stops. |
| `NarrativeEvent` | `std::string tag` | A story beat fires (dialogue `setFlag`, interactables, cutscene `Event` tracks…). The tag is auto-recorded as a StoryState flag. |
| `StoryStateChangedEvent` | `std::string key` | Any StoryState value changes. |
| `CameraShakeEvent` | intensity/duration | Something requests camera shake. |

```cpp
class Achievements : public Indium::NativeScript
{
    REGISTER_SCRIPT(Achievements)
    Indium::SubscriptionHandle sub_;

    void OnStart() override
    {
        sub_ = Indium::Events::Subscribe<GameEvents::NarrativeEvent>(
            [](const GameEvents::NarrativeEvent& e) { /* react to e.tag */ });
    }
};
```

Publish your own event types the same way — any copyable struct works.

---

## Time & Screen

**`Time`** — static fields:

| Field | Meaning |
|---|---|
| `Time::scale` | Time multiplier. `1.0` normal, `0.5` half-speed, `0` frozen (cutscenes with `pausesGameplay` set this). |
| `Time::elapsed` | Total play time in seconds, reset on Play start. |
| `Time::delta` | This frame's dt, already scaled. |

**`Screen`** — static accessors for the game viewport, primarily for `OnGUI`:

```cpp
int     Screen::Width();          // viewport size in screen pixels
int     Screen::Height();
Vector2 Screen::MousePosition();  // mouse in viewport space
bool    Screen::MousePressed();   // went down this frame
bool    Screen::MouseDown();      // held
bool    Screen::DebugGizmos();    // editor "draw debug gizmos" toggle
bool    Screen::Ticking();        // true in Play, false while paused
```

---

## Runtime GUI

Immediate-mode screen-space drawing for `OnGUI()`. Coordinates are viewport pixels — size layouts off `Screen::Width()`/`Height()`. All functions live in namespace `Indium::GUI`.

```cpp
void  Box(::Rectangle r, Color bg, Color border = BLANK, float borderPx = 0.0f);
void  Label(const char* text, float x, float y, int size, Color c);
void  LabelCentered(const char* text, ::Rectangle within, int size, Color c);
float LabelWrapped(const char* text, ::Rectangle area, int size, Color c,
                   int lineSpacing = 4, int maxChars = -1, int* outRevealable = nullptr);
                   // word-wraps; returns full text height. maxChars enables typewriter reveal.
void  Image(Texture2D tex, ::Rectangle dst, Color tint = WHITE);
bool  Button(::Rectangle r, const char* label, int size = 20);  // true on click; hover/press styled
```

```cpp
void OnGUI() override
{
    float w = (float)Screen::Width();
    GUI::Box({10, 10, 200, 40}, {0, 0, 0, 180});
    GUI::Label(TextFormat("Gold: %d", ItemManager::Get().Count("gold")), 20, 20, 20, WHITE);

    if (GUI::Button({w - 110, 10, 100, 30}, "Menu"))
        MenuManager::Get().OpenPause();
}
```

---

## Logger

Engine-wide logging; output lands in the editor's Console panel and the log file.

```cpp
Logger::Event("Game", "picked up %s x%d", id.c_str(), n);   // printf-style, with a category
```
