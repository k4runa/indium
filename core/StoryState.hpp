#pragma once
#include <map>
#include <string>
#include <variant>
#include <type_traits>
#include <vector>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
        for (const auto& [key, value] : m) j[key] = StoryValueToJson(value);
        return j;
    }

    inline std::map<std::string, StoryValue> StoryValueMapFromJson(const nlohmann::json& j)
    {
        std::map<std::string, StoryValue> m;
        if (j.is_object())
        {
            for (auto it = j.begin(); it != j.end(); ++it) { m[it.key()] = StoryValueFromJson(it.value()); }
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
            if (values_.erase(key) > 0) NotifyChange(key);
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

        /** @brief (Re)establishes the subscription to NarrativeEvent.
         *  Called once at construction, and must be called again after
         *  EventBus::Clear() wipes every channel on Stop — this singleton is
         *  long-lived and would otherwise silently lose its handler and stop
         *  recording story beats on the next Play. */
        void SubscribeToEvents()
        {
            // NarrativeEvent tags are recorded as boolean flags so story beats
            // fired by scripts immediately become readable story state.
            narrativeSub_ = Events::Subscribe<GameEvents::NarrativeEvent>(
                [this](const GameEvents::NarrativeEvent& e) { if (!e.tag.empty()) SetFlag(e.tag); });
        }

        /** @brief Writes authored scene values into the blackboard, overwriting any
         *  existing entries. Called on Play start and on each scene switch so a
         *  scene's declared starting values always take effect. */
        void Seed(const std::map<std::string, StoryValue>& authored)
        {
            for (const auto& [key, value] : authored) values_.insert_or_assign(key, value);
        }

        nlohmann::json serialize() const { return StoryValueMapToJson(values_); }

        void deserialize(const nlohmann::json& j) { values_ = StoryValueMapFromJson(j); }

    private:
        StoryState() { SubscribeToEvents(); }
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
            for (std::size_t i = 0; i < pendingNotifications_.size(); ++i) Events::Publish(GameEvents::StoryStateChangedEvent{ pendingNotifications_[i] });
            pendingNotifications_.clear();
            notifying_ = false;
        }

        std::map<std::string, StoryValue> values_;
        SubscriptionHandle                narrativeSub_;
        std::vector<std::string>          pendingNotifications_;
        bool                              notifying_ = false;
    };

    // =====================================================================================
    //  Story expression helpers — PURE (no raylib), operate only on StoryState. Shared by
    //  DialogueManager (dialogue text {interpolation} + choice requireFlag conditions) and
    //  reusable by QuestManager (whose objective gating is flag-only today). Unit-tested in
    //  tests/dialogue_test.cpp.
    // =====================================================================================

    /** @brief A StoryValue as display text: bool->"true"/"false", int/float minimal, string as-is. */
    inline std::string StoryFormat(const StoryValue& v)
    {
        std::string out;
        std::visit([&](auto&& a)
        {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, bool>)             out = a ? "true" : "false";
            else if constexpr (std::is_same_v<T, int>)         out = std::to_string(a);
            else if constexpr (std::is_same_v<T, float>)       { char b[32]; std::snprintf(b, sizeof(b), "%.6g", (double)a); out = b; }
            else if constexpr (std::is_same_v<T, std::string>) out = a;
        }, v);
        return out;
    }

    /** @brief True if the whole string parses as a number (int/float/scientific, optional sign). */
    inline bool StoryIsNumber(const std::string& s)
    {
        if (s.empty()) return false;
        char* end = nullptr;
        std::strtod(s.c_str(), &end);
        return end == s.c_str() + s.size();
    }

    /** @brief Coerce a StoryValue to a double for numeric comparison: bool->0/1, int/float as-is,
     *  numeric string parsed, non-numeric string -> NaN (so any comparison against it is false). */
    inline double StoryToNumber(const StoryValue& v)
    {
        double out = std::numeric_limits<double>::quiet_NaN();
        std::visit([&](auto&& a)
        {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, bool>)             out = a ? 1.0 : 0.0;
            else if constexpr (std::is_same_v<T, int>)         out = (double)a;
            else if constexpr (std::is_same_v<T, float>)       out = (double)a;
            else if constexpr (std::is_same_v<T, std::string>) { if (StoryIsNumber(a)) out = std::strtod(a.c_str(), nullptr); }
        }, v);
        return out;
    }

    namespace detail
    {
        /** @brief Recursive-descent evaluator for dialogue/quest conditions. Grammar:
         *    or      := and ("||" and)*
         *    and     := unary ("&&" unary)*
         *    unary   := "!" unary | primary
         *    primary := "(" or ")" | key OP literal | key
         *  OP is == != <= >= < >; a bare `key` means GetBool(key). Never throws: unknown keys
         *  read false, malformed literals compare false. */
        struct StoryCondEval
        {
            const std::string& s;
            const StoryState&  st;
            std::size_t        pos = 0;

            StoryCondEval(const std::string& src, const StoryState& state) : s(src), st(state) {}

            void skipWs() { while (pos < s.size() && std::isspace((unsigned char)s[pos])) ++pos; }

            bool consume(const char* tok)
            {
                skipWs();
                const std::size_t n = std::strlen(tok);
                if (s.compare(pos, n, tok) == 0) { pos += n; return true; }
                return false;
            }

            // A key/identifier: letters, digits, '_' and '.' (keys like quest.x.state).
            std::string readKey()
            {
                skipWs();
                const std::size_t start = pos;
                while (pos < s.size() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_' || s[pos] == '.')) ++pos;
                return s.substr(start, pos - start);
            }

            // A comparison operator, longest-match first; "" if none.
            std::string readOp()
            {
                skipWs();
                for (const char* op : { "==", "!=", "<=", ">=" }) if (s.compare(pos, 2, op) == 0) { pos += 2; return op; }
                for (const char* op : { "<", ">" })               if (s.compare(pos, 1, op) == 0) { pos += 1; return op; }
                return {};
            }

            // RHS literal: a "quoted string", or a bareword/number up to the next operator/paren/space.
            std::string readLiteral(bool& quoted)
            {
                skipWs();
                quoted = false;
                if (pos < s.size() && s[pos] == '"')
                {
                    quoted = true; ++pos;
                    const std::size_t start = pos;
                    while (pos < s.size() && s[pos] != '"') ++pos;
                    std::string out = s.substr(start, pos - start);
                    if (pos < s.size()) ++pos; // closing quote
                    return out;
                }
                const std::size_t start = pos;
                while (pos < s.size() && !std::isspace((unsigned char)s[pos]) &&
                       s[pos] != '&' && s[pos] != '|' && s[pos] != ')' && s[pos] != '(') ++pos;
                return s.substr(start, pos - start);
            }

            StoryValue lookup(const std::string& key) const
            {
                auto it = st.Values().find(key);
                return it == st.Values().end() ? StoryValue{ false } : it->second;
            }

            bool comparison(const std::string& key, const std::string& op, const std::string& rhs, bool quoted)
            {
                const StoryValue lv = lookup(key);
                if (op == "==" || op == "!=")
                {
                    bool eq;
                    if (!quoted && (rhs == "true" || rhs == "false")) eq = (st.GetBool(key, false) == (rhs == "true"));
                    else if (!quoted && StoryIsNumber(rhs))
                    {
                        // Tolerant compare: StoryValue floats promote to double, so an exact == against
                        // the parsed literal would fail for values like 0.1 that aren't representable.
                        // A NaN left side (non-numeric string) stays unequal, exactly as before.
                        const double an = StoryToNumber(lv);
                        const double bn = std::strtod(rhs.c_str(), nullptr);
                        const double scale = std::fmax(1.0, std::fmax(std::fabs(an), std::fabs(bn)));
                        eq = std::fabs(an - bn) <= 1e-6 * scale;
                    }
                    else                                              eq = (StoryFormat(lv) == rhs);
                    return op == "==" ? eq : !eq;
                }
                const double a = StoryToNumber(lv);
                const double b = StoryIsNumber(rhs) ? std::strtod(rhs.c_str(), nullptr)
                                                    : std::numeric_limits<double>::quiet_NaN();
                if (op == "<")  return a <  b;
                if (op == "<=") return a <= b;
                if (op == ">")  return a >  b;
                if (op == ">=") return a >= b;
                return false;
            }

            bool parsePrimary()
            {
                skipWs();
                if (consume("(")) { bool v = parseOr(); consume(")"); return v; }
                const std::string key = readKey();
                const std::string op  = readOp();
                if (op.empty()) return st.GetBool(key, false); // bare flag
                bool quoted = false;
                const std::string rhs = readLiteral(quoted);
                return comparison(key, op, rhs, quoted);
            }
            bool parseUnary() { skipWs(); return consume("!") ? !parseUnary() : parsePrimary(); }
            bool parseAnd()   { bool v = parseUnary(); while (consume("&&")) { bool r = parseUnary(); v = v && r; } return v; }
            bool parseOr()    { bool v = parseAnd();   while (consume("||")) { bool r = parseAnd();   v = v || r; } return v; }
        };
    }

    /** @brief Evaluate a dialogue/quest condition against StoryState. Empty == true (no gate).
     *  A bare identifier is a boolean flag; otherwise AND/OR of comparisons (see StoryCondEval). */
    inline bool StoryEval(const std::string& expr, const StoryState& st = StoryState::Get())
    {
        std::size_t a = 0;           while (a < expr.size() && std::isspace((unsigned char)expr[a]))     ++a;
        std::size_t b = expr.size(); while (b > a          && std::isspace((unsigned char)expr[b - 1])) --b;
        if (a >= b) return true; // empty / whitespace -> no condition

        // Back-compat: an expression with no operators is a bare flag, looked up exactly (the old
        // gate was HasFlag(requireFlag)). Preserves flag names that aren't plain identifiers — e.g.
        // "talked-to-bob" — which the tokenizer would otherwise truncate at the first '-'.
        const std::string trimmed = expr.substr(a, b - a);
        if (trimmed.find_first_of("!&|()<>=") == std::string::npos) return st.GetBool(trimmed, false);

        detail::StoryCondEval ev(expr, st);
        return ev.parseOr();
    }

    /** @brief Replace {key} tokens with StoryFormat of the StoryState value. Unknown keys are
     *  left literal (so typos stay visible). `{{` / `}}` emit literal braces. */
    inline std::string StoryInterpolate(const std::string& src, const StoryState& st = StoryState::Get())
    {
        std::string out;
        out.reserve(src.size());
        for (std::size_t i = 0; i < src.size(); )
        {
            const char ch = src[i];
            if (ch == '{' && i + 1 < src.size() && src[i + 1] == '{') { out += '{'; i += 2; continue; }
            if (ch == '}' && i + 1 < src.size() && src[i + 1] == '}') { out += '}'; i += 2; continue; }
            if (ch == '{')
            {
                std::size_t j = i + 1;
                while (j < src.size() && (std::isalnum((unsigned char)src[j]) || src[j] == '_' || src[j] == '.')) ++j;
                if (j < src.size() && src[j] == '}' && j > i + 1)
                {
                    const std::string key = src.substr(i + 1, j - (i + 1));
                    auto it = st.Values().find(key);
                    if (it != st.Values().end()) { out += StoryFormat(it->second); i = j + 1; continue; }
                }
                out += ch; ++i; continue; // unknown token: leave the '{' literal, keep scanning
            }
            out += ch; ++i;
        }
        return out;
    }
}
