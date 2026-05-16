#pragma once
/**
 * IndiumEngine.hpp — Public scripting API for Indium.
 *
 * This is the only file your scripts need to include:
 *
 *   #include "IndiumEngine.hpp"
 *
 * Provides:
 *   - NativeScript base class      (OnStart, OnUpdate, OnDestroy)
 *   - entity                       (pointer to the owning entity)
 *   - GetComponent<T>()            (Unity-style component access)
 *   - Destroy()                    (destroy this entity at end of frame)
 *   - GetScene()                   (access the active scene)
 *   - IND_PROP(type, name, def)    (inspector-visible property)
 *   - REGISTER_SCRIPT(ClassName)   (register script with engine)
 *   - INDIUM_EXPORT_SCRIPTS()      (place once in your Exports.cpp)
 *   - StoryState                   (global narrative / flag blackboard)
 *   - SaveManager                  (slot-based save / load)
 *   - Full Raylib API              (Vector2, Color, IsKeyDown, etc.)
 */

#include "raylib.h"
#include "raymath.h"
#include "NativeScript.hpp"
