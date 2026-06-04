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
 *   - SaveManager                  (SaveManager::Save(*GetScene(), slot) / Load(*GetScene(), slot) — persists scene + StoryState + Player positions; project path set by the editor)
 *   - Full Raylib API              (Vector2, Color, IsKeyDown, DrawCircle, etc.)
 *   - Circle, Rectangle, Plane     (entity types for Spawn<T>())
 *   - InputManager                 (InputManager::Get().IsDown("Jump") — named action map shared with the editor)
 *   - CameraComponent              (GetComponent<CameraComponent>() — AddTrauma, ZoomTo, SetFollowTarget, etc.)
 *   - AudioSourceComponent         (GetComponent<AudioSourceComponent>() — Play, Stop, Pause, Resume, IsPlaying; routes through a mixer bus)
 *   - AudioMixer                   (AudioMixer::Get().SetBusVolume("Music", 0.3f); .master = 0.5f — global bus + master volumes)
 *   - TextRendererComponent        (GetComponent<TextRendererComponent>() — world-space text; set .text / .color)
 *   - ParticleSystemComponent      (GetComponent<ParticleSystemComponent>() — Play, Stop, Clear)
 *   - TilemapComponent             (GetComponent<TilemapComponent>() — SetTile, GetTile, Fill, Clear; set collisionEnabled for solid tiles; SetIndexPassable / SetIndexOneWay to mark decorative or one-way-platform tiles)
 *   - AnimatorComponent            (GetComponent<AnimatorComponent>() — Play("walk"))
 *   - RigidbodyComponent           (GetComponent<RigidbodyComponent>() — set entity->velocity for physics-driven movement; freezeRotation, gravityScale, isKinematic)
 *   - Collider2D / Box / Circle    (GetComponent<BoxCollider2D>() / CircleCollider2D — size/radius/offset/isTrigger)
 *   - CameraShakeEvent             (Events::Publish(GameEvents::CameraShakeEvent{0.5f}) from any script)
 *   - OnGUI()                      (NativeScript hook — screen-space UI drawn each frame in Play/Pause)
 *   - Screen                       (Screen::Width()/Height()/MousePosition() — viewport-space UI metrics)
 *   - GUI                          (GUI::Box/Label/Button/Image — immediate-mode widgets in viewport pixels)
 *   - DialogueManager              (DialogueManager::Get().Start("intro") — runtime dialogue from dialogue/<name>.json)
 *   - QuestManager                 (QuestManager::Get().Start("find_sword") / IsComplete(...) — quests from quests/<id>.json; progress lives in StoryState)
 *   - InteractableComponent        (GetComponent<InteractableComponent>() — prompt/radius/setFlag/toggleFlag/dialogueId/eventTag)
 *   - PlayerInteractorComponent    (GetComponent<PlayerInteractorComponent>() — actionName/requireTag)
 *   - Parallax                     (GetScene()->SetParallaxEnabled(true); SetParallaxFactor(-1, 0.3f); SetParallaxAnchor({x,y}) — per-depthLayer draw-only scroll rate; layers align at the anchor; layer 0 locked at 1.0)
 *   - Light2DComponent             (GetComponent<Light2DComponent>() — type/color/intensity/radius/coneAngle/castShadows)
 *   - PolygonCollider2D            (GetComponent<PolygonCollider2D>() — points[] in local space; isTrigger/offset)
 *   - EdgeCollider2D               (GetComponent<EdgeCollider2D>() — points[] open polyline; getWorldPoints())
 *   - AudioListenerComponent       (GetComponent<AudioListenerComponent>() — marks the "ear" for spatial AudioSource)
 *   - DistanceJoint2D              (GetComponent<DistanceJoint2D>() — Connect(entity); distance/maxDistanceOnly/dampingRatio)
 *   - HingeJoint2D                 (GetComponent<HingeJoint2D>() — Connect(entity); useMotor/motorSpeed/useLimits/min/maxAngle)
 *   - SpringJoint2D                (GetComponent<SpringJoint2D>() — Connect(entity); restLength/stiffness/damping)
 *   - SortingGroup                 (GetComponent<SortingGroup>() — sortingLayer/sortingOrder applied to all children)
 *   - FlipComponent                (GetComponent<FlipComponent>() — flipX/flipY, or mode=AutoByVelocity)
 *   - LineRendererComponent        (GetComponent<LineRendererComponent>() — SetEndpoints(a,b)/SetPoints(...)/AddPoint/Clear/SetColor; local-space)
 *   - PathFollowerComponent        (GetComponent<PathFollowerComponent>() — Play/Pause/Stop/GoToWaypoint/IsMoving; waypoints[]/speed/loopMode)
 *   - TimerComponent               (GetComponent<TimerComponent>() — Restart/Pause/Resume/Stop/IsFinished/Remaining/Progress; fires eventTag / setFlagOnComplete)
 *   - AreaEffect2DComponent        (GetComponent<AreaEffect2DComponent>() — force field over Rigidbodies; Box/Circle, Directional/Radial, strength)
 *   - NavigationAgent2DComponent   (GetComponent<NavigationAgent2DComponent>() — SetDestination({x,y}) / SetTarget(entity) / Stop / HasArrived; grid A* around colliders)
 *   - PostProcessComponent         (GetComponent<PostProcessComponent>() — set effect + params at runtime; chained over the viewport)
 *   - TrailRendererComponent       (GetComponent<TrailRendererComponent>() — Emit(bool)/ClearTrail(); time/width/color taper, world-space ribbon)
 *   - SpawnPointComponent          (GetComponent<SpawnPointComponent>() — spawnId/prefabName/Position()/Rotation(); query with FindObjectsOfType)
 *   - CheckpointComponent          (GetComponent<CheckpointComponent>() — zone that sets a flag / auto-saves / fires NarrativeEvent when a tagged entity enters)
 *   - PhysicsMaterial2DComponent   (GetComponent<PhysicsMaterial2DComponent>() — bounciness/linearDrag/angularDrag applied to the Rigidbody; Apply())
 *   - NavigationRegion2DComponent  (GetComponent<NavigationRegion2DComponent>() — restricts NavigationAgent2D pathfinding to a rectangle)
 *   - DecalComponent               (GetComponent<DecalComponent>() — texture stamp with tint/additive/fade-out; Load())
 *   - SpriteSheetComponent         (GetComponent<SpriteSheetComponent>() — SetFrame(i)/FrameCount(); slices the Sprite Renderer texture into a grid)
 */

#include "raylib.h"
#include "raymath.h"
#include "NativeScript.hpp"
#include "AudioMixer.hpp"
#include "InputManager.hpp"
#include "Screen.hpp"
#include "GUI.hpp"
#include "DialogueManager.hpp"
#include "QuestManager.hpp"
#include "../2D/component/CameraComponent.hpp"
#include "../2D/component/InteractableComponent.hpp"
#include "../2D/component/PlayerInteractorComponent.hpp"
#include "../2D/component/AudioSourceComponent.hpp"
#include "../2D/component/TextRendererComponent.hpp"
#include "../2D/component/ParticleSystemComponent.hpp"
#include "../2D/component/TilemapComponent.hpp"
#include "../2D/component/AnimatorComponent.hpp"
#include "../2D/component/RigidbodyComponent.hpp"
#include "../2D/component/Collider2D.hpp"
#include "../2D/component/Light2DComponent.hpp"
#include "../2D/component/AudioListenerComponent.hpp"
#include "../2D/component/Joint2D.hpp"
#include "../2D/component/SortingGroup.hpp"
#include "../2D/component/FlipComponent.hpp"
#include "../2D/component/LineRendererComponent.hpp"
#include "../2D/component/PathFollowerComponent.hpp"
#include "../2D/component/TimerComponent.hpp"
#include "../2D/component/AreaEffect2DComponent.hpp"
#include "../2D/component/NavigationAgent2DComponent.hpp"
#include "../2D/component/PostProcessComponent.hpp"
#include "../2D/component/TrailRendererComponent.hpp"
#include "../2D/component/SpawnPointComponent.hpp"
#include "../2D/component/CheckpointComponent.hpp"
#include "../2D/component/PhysicsMaterial2DComponent.hpp"
#include "../2D/component/NavigationRegion2DComponent.hpp"
#include "../2D/component/DecalComponent.hpp"
#include "../2D/component/SpriteSheetComponent.hpp"
#include "../2D/entity/Circle.hpp"
#include "../2D/entity/Rectangle.hpp"
#include "../2D/entity/Plane.hpp"
