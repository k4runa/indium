#pragma once
/**
 * IndiumEngine.hpp — Public scripting API for Indium.
 *
 * This is the only file your scripts need to include:
 *
 *   #include "IndiumEngine.hpp"
 *
 * Provides:
 *   - NativeScript base class      (OnStart, OnUpdate, OnDestroy, OnDraw, OnGUI)
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
 *   - InputManager                 (InputManager::Get().IsDown("Jump") — named action map shared with the editor)
 *   - CameraComponent              (GetComponent<CameraComponent>() — AddTrauma, ZoomTo, SetFollowTarget, etc.)
 *   - AudioSourceComponent         (GetComponent<AudioSourceComponent>() — Play, Stop, Pause, Resume, IsPlaying)
 *   - TextRendererComponent        (GetComponent<TextRendererComponent>() — world-space text; set .text / .color)
 *   - ParticleSystemComponent      (GetComponent<ParticleSystemComponent>() — Play, Stop, Clear)
 *   - TilemapComponent             (GetComponent<TilemapComponent>() — SetTile, GetTile, Fill, Clear)
 *   - AnimatorComponent            (GetComponent<AnimatorComponent>() — Play("walk"))
 *   - CameraShakeEvent             (Events::Publish(GameEvents::CameraShakeEvent{0.5f}) from any script)
 *   - OnGUI()                      (NativeScript hook — screen-space UI drawn each frame in Play/Pause)
 *   - Screen                       (Screen::Width()/Height()/MousePosition() — viewport-space UI metrics)
 *   - GUI                          (GUI::Box/Label/Button/Image — immediate-mode widgets in viewport pixels)
 *   - DialogueManager              (DialogueManager::Get().Start("intro") — runtime dialogue from dialogue/<name>.json)
 *   - InteractableComponent        (GetComponent<InteractableComponent>() — prompt/radius/setFlag/toggleFlag/dialogueId/eventTag)
 *   - PlayerInteractorComponent    (GetComponent<PlayerInteractorComponent>() — actionName/requireTag)
 */

#include "raylib.h"
#include "raymath.h"
#include "NativeScript.hpp"
#include "InputManager.hpp"
#include "Screen.hpp"
#include "GUI.hpp"
#include "DialogueManager.hpp"
#include "../2D/component/CameraComponent.hpp"
#include "../2D/component/InteractableComponent.hpp"
#include "../2D/component/PlayerInteractorComponent.hpp"
#include "../2D/component/AudioSourceComponent.hpp"
#include "../2D/component/TextRendererComponent.hpp"
#include "../2D/component/ParticleSystemComponent.hpp"
#include "../2D/component/TilemapComponent.hpp"
#include "../2D/component/AnimatorComponent.hpp"
#include "../2D/entity/Circle.hpp"
#include "../2D/entity/Rectangle.hpp"
#include "../2D/entity/Plane.hpp"
