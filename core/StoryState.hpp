#pragma once
#include <map>
#include <string>
#include <variant>
#include <type_traits>
#include <vector>
#include "EventBus.hpp"
#include "events/GameEvents.hpp"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    /**
     * @brief A single story variable: a boolean flag, a counter, a number, or text.
     */
    using StoryValue = std::variant<bool, int, float, std::string>;

    /** @brief Encodes a StoryValue as a tagged JSON object: { "type": ..., "value": ... }. */
    inline nlohmann::json StoryValueToJson(const StoryValue& v)
    {
        nlohmann::json j;
        std::visit([&](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>)             { j["type"] = "bool";   j["value"] = arg; }
            else if constexpr (std::is_same_v<T, int>)         { j["type"] = "int";    j["value"] = arg; }
            else if constexpr (std::is_same_v<T, float>)       { j["type"] = "float";  j["value"] = arg; }
            else if constexpr (std::is_same_v<T, std::string>) { j["type"] = "string"; j["value"] = arg; }
        },v);
        return j;
    }

    /** @brief Decodes a StoryValue from its tagged JSON object form. */
    inline StoryValue StoryValueFromJson(const nlohmann::json& j)
    {
        const std::string type = j.value("type", "bool");
        if (type == "int")    return StoryValue{ j.value("value", 0) };
        if (type == "float")  return StoryValue{ j.value("value", 0.0f) };
        if (type == "string") return StoryValue{ j.value("value", std::string{}) };
        return StoryValue{ j.value("value", false) };
    }

    inline nlohmann::json StoryValueMapToJson(const std::map<std::string, StoryValue>& m)
    {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [key, value] : m)
            j[key] = StoryValueToJson(value);
        return j;
    }

    inline std::map<std::string, StoryValue> StoryValueMapFromJson(const nlohmann::json& j)
    {
        std::map<std::string, StoryValue> m;
        if (j.is_object())
        {
            for (auto it = j.begin(); it != j.end(); ++it)
            {
                m[it.key()] = StoryValueFromJson(it.value());
            }
        }
        return m;
    }

    /**
     * @brief Global story blackboard — a key/value store of flags and variables.
     *
     * This is the runtime backbone of story-driven games: NarrativeEvent tags
     * are written here as flags, and dialogue branches, triggers and scripts
     * read from it (e.g. "met_alice", "chapter == 3").
     *
     * The blackboard is global to the whole game and survives scene switches.
     * Authored starting values live per-scene (see Scene::storyState) and are
     * seeded in on Play; the live state here is discarded when Play stops.
     *
     * Modeled on EventBus: a header-only singleton, shared across the script
     * library boundary so compiled scripts read and write the same instance.
     */
    class StoryState
    {
    public:
        StoryState(const StoryState&)            = delete;
        StoryState& operator=(const StoryState&) = delete;
        StoryState(StoryState&&)                 = delete;
        StoryState& operator=(StoryState&&)      = delete;

        static StoryState& Get()
        {
            static StoryState instance;
            return instance;
        }

        // --- Generic access ---

        void Set(const std::string& key, StoryValue value)
        {
            values_[key] = std::move(value);
            NotifyChange(key);
        }

        [[nodiscard]] bool Has(const std::string& key) const
        {
            return values_.find(key) != values_.end();
        }

        void Remove(const std::string& key)
        {
            if (values_.erase(key) > 0)
                NotifyChange(key);
        }

        [[nodiscard]] bool GetBool(const std::string& key, bool def = false) const
        {
            auto it = values_.find(key);
            if (it == values_.end()) return def;
            if (const bool* p = std::get_if<bool>(&it->second)) return *p;
            return def;
        }

        [[nodiscard]] int GetInt(const std::string& key, int def = 0) const
        {
            auto it = values_.find(key);
            if (it == values_.end()) return def;
            if (const int* p = std::get_if<int>(&it->second)) return *p;
            return def;
        }

        [[nodiscard]] float GetFloat(const std::string& key, float def = 0.0f) const
        {
            auto it = values_.find(key);
            if (it == values_.end()) return def;
            if (const float* p = std::get_if<float>(&it->second)) return *p;
            return def;
        }

        [[nodiscard]] std::string GetString(const std::string& key, const std::string& def = "") const
        {
            auto it = values_.find(key);
            if (it == values_.end()) return def;
            if (const std::string* p = std::get_if<std::string>(&it->second)) return *p;
            return def;
        }

        // --- Flag convenience (boolean semantics) ---

        void SetFlag(const std::string& name)   { Set(name, StoryValue{ true }); }
        void ClearFlag(const std::string& name) { Set(name, StoryValue{ false }); }
        [[nodiscard]] bool HasFlag(const std::string& name) const { return GetBool(name, false); }

        // --- Lifecycle ---

        /** @brief All current entries — used by the editor panel. */
        [[nodiscard]] const std::map<std::string, StoryValue>& Values() const { return values_; }

        /** @brief Wipes all runtime values. Called when Play stops. */
        void Clear() { values_.clear(); }

        /** @brief Adds authored entries that are not already present (scene seeding). */
        void Seed(const std::map<std::string, StoryValue>& authored)
        {
            for (const auto& [key, value] : authored)
                values_.emplace(key, value);
        }

        nlohmann::json serialize() const { return StoryValueMapToJson(values_); }

        void deserialize(const nlohmann::json& j) { values_ = StoryValueMapFromJson(j); }

    private:
        StoryState()
        {
            // NarrativeEvent tags are recorded as boolean flags so story beats
            // fired by scripts immediately become readable story state.
            narrativeSub_ = Events::Subscribe<GameEvents::NarrativeEvent>(
                [this](const GameEvents::NarrativeEvent& e)
                {
                    if (!e.tag.empty()) SetFlag(e.tag);
                });
        }
        ~StoryState() = default;

        // Publish queued StoryStateChangedEvents without re-entering ourselves.
        // A subscriber that calls Set/Remove appends to pendingNotifications_;
        // the outermost call drains the queue so every change still fires
        // exactly one event in causal order.
        void NotifyChange(const std::string& key)
        {
            pendingNotifications_.push_back(key);
            if (notifying_) return;
            notifying_ = true;
            for (std::size_t i = 0; i < pendingNotifications_.size(); ++i)
                Events::Publish(GameEvents::StoryStateChangedEvent{ pendingNotifications_[i] });
            pendingNotifications_.clear();
            notifying_ = false;
        }

        std::map<std::string, StoryValue> values_;
        SubscriptionHandle                narrativeSub_;
        std::vector<std::string>          pendingNotifications_;
        bool                              notifying_ = false;
    };
}
