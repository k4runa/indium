#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include "Time.hpp"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    // Update() binds tracks to live entities, so it needs a Scene — but only the
    // .cpp touches it. Forward-declare here so this header stays light enough to be
    // pulled into compiled scripts (which only ever Play()/Stop() a cutscene by id),
    // exactly like DialogueManager / StoryState.
    struct Scene;

    /** @brief Interpolation curve between two keyframes. Step holds the earlier value. */
    enum class CutsceneEasing { Linear, EaseIn, EaseOut, EaseInOut, Step };

    /**
     * @brief What a track does. The first two are *interpolated* (sampled every
     * frame); the rest are *trigger* tracks (fire once when the playhead crosses
     * an event time).
     */
    enum class CutsceneTrackType { Transform, Camera, Dialogue, Audio, Animation, Activation, StoryFlag, Event, Particle };

    /**
     * @brief One keyframe on an interpolated track. A Transform key uses pos/rot/scale
     * (gated by the track's channel toggles); a Camera key uses pos (look-at) + zoom + rot.
     * Unused fields are simply ignored, so one struct serves both track types.
     */
    struct CutsceneKey
    {
        float          time   = 0.0f;
        Vector2        pos     = { 0.0f, 0.0f };
        float          rot     = 0.0f;
        Vector2        scale   = { 1.0f, 1.0f };
        float          zoom    = 1.0f;
        CutsceneEasing easing  = CutsceneEasing::Linear;   // curve out of this key
    };

    /**
     * @brief A point-in-time action on a trigger track. `a` is the primary payload,
     * interpreted per track type:
     *   Dialogue   → dialogue id          Audio/Particle → "play" | "stop"
     *   Animation  → clip name            Activation     → "show" | "hide"
     *   StoryFlag  → flag name (b="false" clears)         Event → NarrativeEvent tag
     */
    struct CutsceneEvent
    {
        float       time       = 0.0f;
        std::string a;
        std::string b;
        bool        fireOnSkip = false;   // still fired when the player skips the cutscene
    };

    /** @brief A lane of keyframes (interpolated) or events (trigger) bound to one target. */
    struct CutsceneTrack
    {
        CutsceneTrackType type        = CutsceneTrackType::Transform;
        std::string       target;             // entity name (or tag); "" on a Camera track = primary camera
        bool              targetIsTag = false;
        bool              muted       = false;

        // Transform channel toggles — which of pos/rot/scale this track writes.
        bool animatePosition = true;
        bool animateRotation = false;
        bool animateScale    = false;

        std::vector<CutsceneKey>   keys;      // interpolated tracks (Transform, Camera)
        std::vector<CutsceneEvent> events;    // trigger tracks (everything else)

        [[nodiscard]] bool isInterpolated() const
        {
            return type == CutsceneTrackType::Transform || type == CutsceneTrackType::Camera;
        }
    };

    /** @brief A whole timeline document (one cutscenes/<name>.json file). */
    struct Cutscene
    {
        std::string                name;             // file stem; not part of the JSON body
        float                      duration = 5.0f;  // seconds
        bool                       loop     = false;
        bool                       pausesGameplay = false;  // freeze the scene (Time::scale=0) while playing
        std::string                onCompleteFlag;          // StoryState flag set when the cutscene finishes
        std::string                onCompleteEvent;         // NarrativeEvent tag published when it finishes
        std::vector<CutsceneTrack> tracks;
    };

    /**
     * @brief Global runtime cutscene player. Header-only singleton shared across the
     * script dylib boundary (like DialogueManager / StoryState), so a compiled script
     * can `CutsceneManager::Get().Play("intro")` and drive the same instance the editor
     * ticks.
     *
     * Data lives in <project>/cutscenes/<name>.json. The trigger-facing API (Play/Stop/
     * Skip/IsPlaying) and the document round-trip (ToJson/FromJson, shared with the editor
     * panel) are inline here; the per-frame Update(dt, Scene*) that samples tracks onto
     * entities lives in CutsceneManager.cpp (engine-only) so the component headers it needs
     * never leak into scripts.
     */
    class CutsceneManager
    {
    public:
        CutsceneManager(const CutsceneManager&)            = delete;
        CutsceneManager& operator=(const CutsceneManager&) = delete;
        CutsceneManager(CutsceneManager&&)                 = delete;
        CutsceneManager& operator=(CutsceneManager&&)      = delete;

        static CutsceneManager& Get() { static CutsceneManager inst; return inst; }

        /** @brief Where cutscene files are loaded from. Set by the editor on project open / Play. */
        void SetProjectPath(const std::string& path) { projectPath_ = path; }

        /** @brief Loads <project>/cutscenes/<name>.json and begins playback at t=0.
         *  Scene-touching setup (capture camera follow, apply the t=0 pose) is deferred to
         *  the first Update so this is safe to call from a script's OnUpdate. Returns false
         *  (and logs) if the file is missing or malformed. */
        bool Play(const std::string& name)
        {
            if (projectPath_.empty()) { TraceLog(LOG_WARNING, "CUTSCENE: no project path set"); return false; }

            const std::string path =
                (std::filesystem::path(projectPath_) / "cutscenes" / (name + ".json")).string();
            std::ifstream f(path);
            if (!f.is_open()) { TraceLog(LOG_WARNING, "CUTSCENE: cannot open %s", path.c_str()); return false; }

            nlohmann::json j;
            try { f >> j; } catch (...) { TraceLog(LOG_WARNING, "CUTSCENE: invalid JSON in %s", path.c_str()); return false; }

            Cutscene cs = FromJson(j);
            cs.name = name;
            return PlayCutscene(cs);
        }

        /** @brief Begins playback of an in-memory cutscene (used by the editor preview and tests). */
        bool PlayCutscene(const Cutscene& cs)
        {
            current_       = cs;
            time_          = 0.0f;
            prevTime_      = 0.0f;
            active_        = true;
            paused_        = false;
            pendingStart_  = true;   // begin_() runs on the next Update, when a Scene is in hand
            skipRequested_ = false;
            return true;
        }

        /** @brief Stops any running cutscene and drops the loaded document. Scene-side
         *  cleanup (restore camera follow / Time::scale) only matters when a cutscene ends
         *  mid-game, which Update handles with a Scene in hand; calling End() on a Play→Stop
         *  or scene switch is safe because the scene is being torn down / snapshot-restored. */
        void End()
        {
            // A cutscene stopped while it had frozen gameplay must hand Time::scale back —
            // finish_() does this on natural completion, but End() covers an editor Stop /
            // scene switch mid-cutscene (where there's no Scene to restore camera follow,
            // and the scene snapshot/reload handles entity state anyway).
            if (didPauseGameplay_) { Time::scale = savedTimeScale_; didPauseGameplay_ = false; }
            active_        = false;
            paused_        = false;
            pendingStart_  = false;
            skipRequested_ = false;
            time_          = 0.0f;
            prevTime_      = 0.0f;
            savedCameraFollow_.clear();
            current_       = Cutscene{};
        }

        /** @brief Pauses / resumes the playhead without ending the cutscene. */
        void Pause()  { if (active_) paused_ = true; }
        void Resume() { if (active_) paused_ = false; }

        /** @brief Fast-forward to the end: snap interpolated tracks to their final values
         *  and fire only `fireOnSkip` trigger events (so story flags stay consistent).
         *  Deferred to the next Update so it's callable from a script. */
        void Skip() { if (active_) skipRequested_ = true; }

        [[nodiscard]] bool        IsPlaying() const { return active_ && !paused_; }
        [[nodiscard]] bool        IsActive()  const { return active_; }
        [[nodiscard]] float       Time()      const { return time_; }
        [[nodiscard]] const Cutscene& Current() const { return current_; }

        /**
         * @brief Advances the playhead by `dt` (real, UNSCALED seconds — so a cutscene
         * with pausesGameplay can freeze the scene yet keep playing), samples interpolated
         * tracks onto their bound entities, and fires trigger events crossed this frame.
         * Defined in CutsceneManager.cpp. No-op when inactive or paused.
         */
        void Update(float dt, Scene* scene);

        /**
         * @brief Samples interpolated tracks at the given time onto bound entities WITHOUT
         * firing triggers or advancing the playhead — used by the editor timeline to scrub a
         * preview. Defined in CutsceneManager.cpp.
         */
        void SampleAt(float t, Scene* scene);

        // --- Document (de)serialization ------------------------------------------
        // Shared by the editor's Cutscene panel and the runtime loader so both speak one
        // on-disk format (mirrors DialogueManager::ToJson / FromJson).

        static nlohmann::json ToJson(const Cutscene& cs)
        {
            nlohmann::json j;
            j["duration"]       = cs.duration;
            j["loop"]           = cs.loop;
            j["pausesGameplay"] = cs.pausesGameplay;
            if (!cs.onCompleteFlag.empty())  j["onCompleteFlag"]  = cs.onCompleteFlag;
            if (!cs.onCompleteEvent.empty()) j["onCompleteEvent"] = cs.onCompleteEvent;

            nlohmann::json tracks = nlohmann::json::array();
            for (const auto& t : cs.tracks)
            {
                nlohmann::json tj;
                tj["type"]   = TrackTypeToStr(t.type);
                tj["target"] = t.target;
                if (t.targetIsTag) tj["targetIsTag"] = true;
                if (t.muted)       tj["muted"]       = true;

                if (t.type == CutsceneTrackType::Transform)
                {
                    nlohmann::json ch = nlohmann::json::array();
                    if (t.animatePosition) ch.push_back("position");
                    if (t.animateRotation) ch.push_back("rotation");
                    if (t.animateScale)    ch.push_back("scale");
                    tj["channels"] = ch;
                }

                if (t.isInterpolated())
                {
                    nlohmann::json keys = nlohmann::json::array();
                    for (const auto& k : t.keys)
                    {
                        nlohmann::json kj;
                        kj["t"]     = k.time;
                        kj["pos"]   = { k.pos.x, k.pos.y };
                        kj["rot"]   = k.rot;
                        kj["scale"] = { k.scale.x, k.scale.y };
                        kj["zoom"]  = k.zoom;
                        kj["ease"]  = EasingToStr(k.easing);
                        keys.push_back(std::move(kj));
                    }
                    tj["keys"] = std::move(keys);
                }
                else
                {
                    nlohmann::json evs = nlohmann::json::array();
                    for (const auto& e : t.events)
                    {
                        nlohmann::json ej;
                        ej["t"] = e.time;
                        if (!e.a.empty())   ej["a"] = e.a;
                        if (!e.b.empty())   ej["b"] = e.b;
                        if (e.fireOnSkip)   ej["fireOnSkip"] = true;
                        evs.push_back(std::move(ej));
                    }
                    tj["events"] = std::move(evs);
                }
                tracks.push_back(std::move(tj));
            }
            j["tracks"] = std::move(tracks);
            return j;
        }

        static Cutscene FromJson(const nlohmann::json& j)
        {
            Cutscene cs;
            cs.duration       = j.value("duration", 5.0f);
            cs.loop           = j.value("loop", false);
            cs.pausesGameplay = j.value("pausesGameplay", false);
            cs.onCompleteFlag  = j.value("onCompleteFlag",  std::string{});
            cs.onCompleteEvent = j.value("onCompleteEvent", std::string{});

            if (j.contains("tracks") && j["tracks"].is_array())
            {
                for (const auto& tj : j["tracks"])
                {
                    CutsceneTrack t;
                    t.type        = TrackTypeFromStr(tj.value("type", std::string("Transform")));
                    t.target      = tj.value("target", std::string{});
                    t.targetIsTag = tj.value("targetIsTag", false);
                    t.muted       = tj.value("muted", false);

                    if (tj.contains("channels") && tj["channels"].is_array())
                    {
                        t.animatePosition = t.animateRotation = t.animateScale = false;
                        for (const auto& c : tj["channels"])
                        {
                            const std::string s = c.get<std::string>();
                            if      (s == "position") t.animatePosition = true;
                            else if (s == "rotation") t.animateRotation = true;
                            else if (s == "scale")    t.animateScale    = true;
                        }
                    }

                    if (tj.contains("keys") && tj["keys"].is_array())
                        for (const auto& kj : tj["keys"])
                        {
                            CutsceneKey k;
                            k.time   = kj.value("t", 0.0f);
                            if (kj.contains("pos")   && kj["pos"].is_array()   && kj["pos"].size()   == 2) { k.pos.x   = kj["pos"][0];   k.pos.y   = kj["pos"][1]; }
                            if (kj.contains("scale") && kj["scale"].is_array() && kj["scale"].size() == 2) { k.scale.x = kj["scale"][0]; k.scale.y = kj["scale"][1]; }
                            k.rot    = kj.value("rot", 0.0f);
                            k.zoom   = kj.value("zoom", 1.0f);
                            k.easing = EasingFromStr(kj.value("ease", std::string("Linear")));
                            t.keys.push_back(k);
                        }

                    if (tj.contains("events") && tj["events"].is_array())
                        for (const auto& ej : tj["events"])
                        {
                            CutsceneEvent e;
                            e.time       = ej.value("t", 0.0f);
                            e.a          = ej.value("a", std::string{});
                            e.b          = ej.value("b", std::string{});
                            e.fireOnSkip = ej.value("fireOnSkip", false);
                            t.events.push_back(std::move(e));
                        }

                    SortTrack(t);
                    cs.tracks.push_back(std::move(t));
                }
            }
            return cs;
        }

        /** @brief Sorts a track's keys/events by time (keeps sampling + crossing detection valid). */
        static void SortTrack(CutsceneTrack& t)
        {
            std::sort(t.keys.begin(),   t.keys.end(),   [](const CutsceneKey& a,   const CutsceneKey& b)   { return a.time < b.time; });
            std::sort(t.events.begin(), t.events.end(), [](const CutsceneEvent& a, const CutsceneEvent& b) { return a.time < b.time; });
        }

        // --- enum <-> string (used by ToJson/FromJson and the editor panel) ---

        static const char* TrackTypeToStr(CutsceneTrackType t)
        {
            switch (t)
            {
                case CutsceneTrackType::Transform:  return "Transform";
                case CutsceneTrackType::Camera:     return "Camera";
                case CutsceneTrackType::Dialogue:   return "Dialogue";
                case CutsceneTrackType::Audio:      return "Audio";
                case CutsceneTrackType::Animation:  return "Animation";
                case CutsceneTrackType::Activation: return "Activation";
                case CutsceneTrackType::StoryFlag:  return "StoryFlag";
                case CutsceneTrackType::Event:      return "Event";
                case CutsceneTrackType::Particle:   return "Particle";
            }
            return "Transform";
        }
        static CutsceneTrackType TrackTypeFromStr(const std::string& s)
        {
            if (s == "Camera")     return CutsceneTrackType::Camera;
            if (s == "Dialogue")   return CutsceneTrackType::Dialogue;
            if (s == "Audio")      return CutsceneTrackType::Audio;
            if (s == "Animation")  return CutsceneTrackType::Animation;
            if (s == "Activation") return CutsceneTrackType::Activation;
            if (s == "StoryFlag")  return CutsceneTrackType::StoryFlag;
            if (s == "Event")      return CutsceneTrackType::Event;
            if (s == "Particle")   return CutsceneTrackType::Particle;
            return CutsceneTrackType::Transform;
        }
        static const char* EasingToStr(CutsceneEasing e)
        {
            switch (e)
            {
                case CutsceneEasing::Linear:    return "Linear";
                case CutsceneEasing::EaseIn:    return "EaseIn";
                case CutsceneEasing::EaseOut:   return "EaseOut";
                case CutsceneEasing::EaseInOut: return "EaseInOut";
                case CutsceneEasing::Step:      return "Step";
            }
            return "Linear";
        }
        static CutsceneEasing EasingFromStr(const std::string& s)
        {
            if (s == "EaseIn")    return CutsceneEasing::EaseIn;
            if (s == "EaseOut")   return CutsceneEasing::EaseOut;
            if (s == "EaseInOut") return CutsceneEasing::EaseInOut;
            if (s == "Step")      return CutsceneEasing::Step;
            return CutsceneEasing::Linear;
        }

        /** @brief Maps a normalized segment position [0,1] through an easing curve. */
        static float ApplyEasing(float u, CutsceneEasing e)
        {
            if (u < 0.0f) u = 0.0f; else if (u > 1.0f) u = 1.0f;
            switch (e)
            {
                case CutsceneEasing::Linear:    return u;
                case CutsceneEasing::EaseIn:    return u * u;
                case CutsceneEasing::EaseOut:   return 1.0f - (1.0f - u) * (1.0f - u);
                case CutsceneEasing::EaseInOut: return (u < 0.5f) ? (2.0f * u * u)
                                                                  : (1.0f - 0.5f * (2.0f - 2.0f * u) * (2.0f - 2.0f * u));
                case CutsceneEasing::Step:      return 0.0f;   // hold the earlier key until the next one
            }
            return u;
        }

    private:
        CutsceneManager()  = default;
        ~CutsceneManager() = default;

        // --- Scene-touching internals (defined in CutsceneManager.cpp) ---
        void begin_(Scene* scene);                 // capture camera follow / Time::scale, apply t=0 pose
        void finish_(Scene* scene);                // restore follow / Time::scale, fire onComplete
        void fireWindow_(Scene* scene, float from, float to, bool skipping); // fire triggers in (from, to]

        std::string projectPath_;
        Cutscene    current_;
        float       time_          = 0.0f;   // current playhead (seconds)
        float       prevTime_      = 0.0f;   // playhead last frame (for trigger crossing)
        bool        active_        = false;
        bool        paused_        = false;
        bool        pendingStart_  = false;  // begin_() owed on the next Update
        bool        skipRequested_ = false;

        // Captured at begin_() so finish_() can put the world back when a cutscene that
        // pauses gameplay / drives the camera ends mid-game.
        float                            savedTimeScale_  = 1.0f;
        bool                             didPauseGameplay_ = false;
        std::vector<std::pair<int,bool>> savedCameraFollow_; // (camera entity id, prior followEnabled)
    };
}
