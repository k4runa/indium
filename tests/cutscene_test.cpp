#include "doctest.h"
#include "CutsceneManager.hpp"
#include "StoryState.hpp"
#include "scene/Scene.hpp"
#include "Entity.hpp"
#include <memory>

using namespace Indium;

// These exercise the exact serialization the editor's Cutscene panel uses
// (CutsceneManager::ToJson / FromJson) and then feed the result through the runtime
// player (PlayCutscene + Update) to prove a panel-authored timeline samples and
// triggers correctly in game — the same "write-format is an untested hand-mirror of
// the parser" gap the dialogue tests close.

namespace
{
    // CutsceneManager + StoryState are process-wide singletons shared across cases.
    // Reset StoryState and re-arm its NarrativeEvent subscription (another test calls
    // EventBus::Clear(), which would otherwise leave StoryFlag/onComplete writes unrecorded),
    // exactly as the editor's Play setup does.
    void freshStart()
    {
        StoryState::Get().Clear();
        StoryState::Get().SubscribeToEvents();
        CutsceneManager::Get().End();
    }

    // A scene with a single named entity the Transform tests can drive.
    Entity* addEntity(Scene& s, const std::string& name, Vector2 pos = {0, 0})
    {
        auto e = std::make_unique<Entity>();
        e->id       = s.nextEntityId++;
        e->name     = name;
        e->position = pos;
        Entity* ptr = e.get();
        s.entities.push_back(std::move(e));
        return ptr;
    }

    Cutscene moverCutscene()
    {
        Cutscene cs;
        cs.name           = "test";
        cs.duration       = 2.0f;
        cs.onCompleteFlag = "intro_done";

        CutsceneTrack t;
        t.type            = CutsceneTrackType::Transform;
        t.target          = "Mover";
        t.animatePosition = true;
        t.keys = {
            { 0.0f, { 0.0f,   0.0f }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
            { 2.0f, { 400.0f, 200.0f }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
        };
        cs.tracks.push_back(std::move(t));
        return cs;
    }
}

TEST_CASE("cutscene JSON round-trips through ToJson/FromJson")
{
    Cutscene cs;
    cs.duration       = 4.0f;
    cs.loop           = true;
    cs.pausesGameplay = true;
    cs.letterbox      = false;   // non-default → must survive the round-trip
    cs.onCompleteFlag = "seen";

    CutsceneTrack tr;
    tr.type            = CutsceneTrackType::Transform;
    tr.target          = "Hero";
    tr.animatePosition = true;
    tr.animateRotation = true;
    tr.animateScale    = false;
    tr.keys = {
        { 0.0f, { 10.0f, 20.0f }, 0.0f,  { 1, 1 }, 1.0f, CutsceneEasing::EaseInOut },
        { 2.0f, { 50.0f, 20.0f }, 90.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
    };

    CutsceneTrack cam;
    cam.type   = CutsceneTrackType::Camera;
    cam.target = "";
    cam.keys = {
        { 0.0f, { 0, 0 }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::EaseOut },
        { 3.0f, { 0, 0 }, 0.0f, { 1, 1 }, 1.6f, CutsceneEasing::EaseInOut },
    };

    CutsceneTrack flag;
    flag.type   = CutsceneTrackType::StoryFlag;
    flag.events = { { 3.5f, "arrived", "", true } };

    cs.tracks = { tr, cam, flag };

    const Cutscene back = CutsceneManager::FromJson(CutsceneManager::ToJson(cs));

    CHECK(back.duration       == doctest::Approx(4.0f));
    CHECK(back.loop           == true);
    CHECK(back.pausesGameplay == true);
    CHECK(back.letterbox      == false);
    CHECK(back.onCompleteFlag == "seen");
    REQUIRE(back.tracks.size() == 3);

    CHECK(back.tracks[0].type            == CutsceneTrackType::Transform);
    CHECK(back.tracks[0].target          == "Hero");
    CHECK(back.tracks[0].animatePosition == true);
    CHECK(back.tracks[0].animateRotation == true);
    CHECK(back.tracks[0].animateScale    == false);
    REQUIRE(back.tracks[0].keys.size() == 2);
    CHECK(back.tracks[0].keys[0].pos.x   == doctest::Approx(10.0f));
    CHECK(back.tracks[0].keys[0].easing  == CutsceneEasing::EaseInOut);
    CHECK(back.tracks[0].keys[1].rot     == doctest::Approx(90.0f));

    CHECK(back.tracks[1].type          == CutsceneTrackType::Camera);
    CHECK(back.tracks[1].keys[1].zoom  == doctest::Approx(1.6f));

    CHECK(back.tracks[2].type              == CutsceneTrackType::StoryFlag);
    REQUIRE(back.tracks[2].events.size() == 1);
    CHECK(back.tracks[2].events[0].time       == doctest::Approx(3.5f));
    CHECK(back.tracks[2].events[0].a          == "arrived");
    CHECK(back.tracks[2].events[0].fireOnSkip == true);
}

TEST_CASE("transform track interpolates position across keys")
{
    freshStart();
    Scene scene;
    Entity* mover = addEntity(scene, "Mover", { 999, 999 });

    CutsceneManager::Get().PlayCutscene(moverCutscene());

    CutsceneManager::Get().Update(0.0f, &scene);   // begin_: snap to the t=0 pose
    CHECK(mover->position.x == doctest::Approx(0.0f));
    CHECK(mover->position.y == doctest::Approx(0.0f));

    CutsceneManager::Get().Update(1.0f, &scene);   // t=1, halfway, Linear
    CHECK(mover->position.x == doctest::Approx(200.0f));
    CHECK(mover->position.y == doctest::Approx(100.0f));

    CutsceneManager::Get().Update(1.0f, &scene);   // t=2, end
    CHECK(mover->position.x == doctest::Approx(400.0f));
    CHECK(mover->position.y == doctest::Approx(200.0f));
    CHECK_FALSE(CutsceneManager::Get().IsActive());        // completed
    CHECK(StoryState::Get().HasFlag("intro_done"));        // onCompleteFlag fired
}

TEST_CASE("playhead clamps to duration without overshooting")
{
    freshStart();
    Scene scene;
    Entity* mover = addEntity(scene, "Mover");

    Cutscene cs = moverCutscene();
    CutsceneManager::Get().PlayCutscene(cs);

    CutsceneManager::Get().Update(0.0f, &scene);
    CutsceneManager::Get().Update(99.0f, &scene);   // huge dt overshoots the 2s duration

    CHECK(CutsceneManager::Get().Time() == doctest::Approx(2.0f));
    CHECK(mover->position.x == doctest::Approx(400.0f));
    CHECK_FALSE(CutsceneManager::Get().IsActive());
}

TEST_CASE("story-flag trigger fires exactly once when the playhead crosses it")
{
    freshStart();
    Scene scene;   // trigger tracks need no entities

    Cutscene cs;
    cs.duration = 3.0f;
    CutsceneTrack t;
    t.type   = CutsceneTrackType::StoryFlag;
    t.events = { { 1.0f, "reached", "", false } };
    cs.tracks.push_back(std::move(t));

    CutsceneManager::Get().PlayCutscene(cs);

    CutsceneManager::Get().Update(0.0f, &scene);
    CutsceneManager::Get().Update(0.5f, &scene);   // t=0.5, before the event
    CHECK_FALSE(StoryState::Get().HasFlag("reached"));

    CutsceneManager::Get().Update(1.0f, &scene);   // t=1.5, crosses 1.0
    CHECK(StoryState::Get().HasFlag("reached"));

    StoryState::Get().ClearFlag("reached");
    CutsceneManager::Get().Update(0.5f, &scene);   // t=2.0, must not re-fire
    CHECK_FALSE(StoryState::Get().HasFlag("reached"));
}

TEST_CASE("skip snaps to the end and fires only fireOnSkip triggers")
{
    freshStart();
    Scene scene;
    Entity* mover = addEntity(scene, "Mover");

    Cutscene cs;
    cs.duration = 5.0f;

    CutsceneTrack move;
    move.type            = CutsceneTrackType::Transform;
    move.target          = "Mover";
    move.animatePosition = true;
    move.keys = {
        { 0.0f, { 0.0f,   0.0f }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
        { 5.0f, { 500.0f, 0.0f }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
    };

    CutsceneTrack critical;   // must survive a skip
    critical.type   = CutsceneTrackType::StoryFlag;
    critical.events = { { 4.0f, "critical", "", true } };

    CutsceneTrack skippable; // must be dropped on skip
    skippable.type   = CutsceneTrackType::StoryFlag;
    skippable.events = { { 2.0f, "skippable", "", false } };

    cs.tracks = { move, critical, skippable };

    CutsceneManager::Get().PlayCutscene(cs);
    CutsceneManager::Get().Update(0.0f, &scene);   // begin
    CutsceneManager::Get().Skip();
    CutsceneManager::Get().Update(0.016f, &scene); // skip resolves here

    CHECK(mover->position.x == doctest::Approx(500.0f));   // snapped to final
    CHECK(StoryState::Get().HasFlag("critical"));          // fireOnSkip kept
    CHECK_FALSE(StoryState::Get().HasFlag("skippable"));   // non-skip dropped
    CHECK_FALSE(CutsceneManager::Get().IsActive());
}

TEST_CASE("looping cutscene wraps the playhead and re-fires openers")
{
    freshStart();
    Scene scene;

    Cutscene cs;
    cs.duration = 2.0f;
    cs.loop     = true;
    CutsceneTrack t;
    t.type   = CutsceneTrackType::StoryFlag;
    t.events = { { 0.5f, "tick", "", false } };
    cs.tracks.push_back(std::move(t));

    CutsceneManager::Get().PlayCutscene(cs);
    CutsceneManager::Get().Update(0.0f, &scene);
    CutsceneManager::Get().Update(1.0f, &scene);   // t=1.0, fires tick (0.5)
    CHECK(StoryState::Get().HasFlag("tick"));

    StoryState::Get().ClearFlag("tick");
    CutsceneManager::Get().Update(1.5f, &scene);   // t=2.5 -> wraps to 0.5, re-fires tick
    CHECK(StoryState::Get().HasFlag("tick"));
    CHECK(CutsceneManager::Get().IsActive());      // a loop never completes

    CutsceneManager::Get().End();
}

TEST_CASE("SampleCutscene poses entities (unsorted-tolerant) without firing triggers")
{
    freshStart();
    Scene scene;
    Entity* mover = addEntity(scene, "Mover", { 0, 0 });

    Cutscene cs;
    cs.duration = 2.0f;

    CutsceneTrack move;
    move.type = CutsceneTrackType::Transform; move.target = "Mover"; move.animatePosition = true;
    // Intentionally out of time order — the editor keeps keys unsorted; SampleCutscene
    // must still interpolate correctly (it sorts a local copy).
    move.keys = {
        { 2.0f, { 400.0f, 0.0f }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
        { 0.0f, { 0.0f,   0.0f }, 0.0f, { 1, 1 }, 1.0f, CutsceneEasing::Linear },
    };

    CutsceneTrack flag;   // a trigger that must NOT fire during a scrub
    flag.type   = CutsceneTrackType::StoryFlag;
    flag.events = { { 0.5f, "fired", "", false } };

    cs.tracks = { move, flag };

    CutsceneManager::SampleCutscene(cs, 1.0f, &scene);
    CHECK(mover->position.x == doctest::Approx(200.0f));  // midpoint, despite unsorted keys
    CHECK_FALSE(StoryState::Get().HasFlag("fired"));      // scrubbing never fires triggers
}
