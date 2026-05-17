#pragma once
/**
 * IndiumEngine.hpp — Public scripting API for Indium.
 *
 * This is the only file your scripts need to include:
 *
 *   #include "IndiumEngine.hpp"
 *
 * Provides:
 *   - NativeScript base class      (OnStart, OnUpdate, OnDestroy, OnDraw)
 *   - entity                       (pointer to the owning entity)
 *   - GetComponent<T>()            (Unity-style component access)
 *   - Destroy() / Destroy(target)  (destroy entity at end of frame)
 *   - Spawn<T>(name)               (spawn a new entity into the scene)
 *   - FindByName(name)             (find entity by name)
 *   - FindById(id)                 (find entity by ID)
 *   - GetScene()                   (access the active scene directly)
 *   - IND_PROP(type, name, def)    (inspector-visible property)
 *   - REGISTER_SCRIPT(ClassName)   (register script with engine)
 *   - INDIUM_EXPORT_SCRIPTS()      (place once in your Exports.cpp)
 *   - StoryState                   (global narrative / flag blackboard)
 *   - SaveManager                  (slot-based save / load)
 *   - Full Raylib API              (Vector2, Color, IsKeyDown, DrawCircle, etc.)
 *   - Circle, Rectangle, Plane     (entity types for Spawn<T>())
 *   - CameraComponent              (GetComponent<CameraComponent>() — AddTrauma, ZoomTo, SetFollowTarget, etc.)
 *   - CameraShakeEvent             (Events::Publish(GameEvents::CameraShakeEvent{0.5f}) from any script)
 */

#include "raylib.h"
#include "raymath.h"
#include "NativeScript.hpp"
#include "../2D/component/CameraComponent.hpp"
#include "../2D/entity/Circle.hpp"
#include "../2D/entity/Rectangle.hpp"
#include "../2D/entity/Plane.hpp"
