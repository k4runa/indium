#pragma once
#include <string>

// Forward declarations — avoids pulling Entity/Scene headers into every subscriber.
namespace Indium { struct Entity; }

namespace Indium::GameEvents
{
    // Fired when two entities overlap (from physics/bouncer components).
    struct CollisionEvent
    {
        Entity* a = nullptr;
        Entity* b = nullptr;
    };

    // Fired when an entity enters a trigger zone (Task 2.1).
    struct TriggerEnterEvent
    {
        Entity* trigger = nullptr;
        Entity* other   = nullptr;
    };

    // Fired when an entity exits a trigger zone.
    struct TriggerExitEvent
    {
        Entity* trigger = nullptr;
        Entity* other   = nullptr;
    };

    // Fired when the engine enters Play mode.
    struct GameStartEvent {};

    // Fired when the engine exits Play mode (Stop pressed).
    struct GameStopEvent {};

    // General-purpose narrative signal — scripts fire this to drive story beats.
    struct NarrativeEvent
    {
        std::string tag;   // e.g. "player_met_npc_alice", "door_opened"
        Entity*     source = nullptr;
    };

    // Fired whenever a story flag/variable is set, changed, or removed.
    struct StoryStateChangedEvent
    {
        std::string key;
    };
}
