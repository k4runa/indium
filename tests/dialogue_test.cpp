#include "doctest.h"
#include "DialogueManager.hpp"
#include "StoryState.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <string>

using namespace Indium;

// These exercise the exact serialization the editor's Dialogue panel uses
// (DialogueManager::ToJson / FromJson — the panel's loadFile/saveFile delegate to
// them) and then feed that output through the runtime loader (Start) to prove a
// panel-authored file branches correctly in game. Closes the "write-format is an
// untested hand-mirror of the parser" gap noted when the panel landed.

namespace
{
    // A small branching dialogue that touches every field: a requireFlag-gated
    // choice, a setFlag choice, a plain end choice, and a narration chain.
    void makeSampleDoc(std::string& start, std::vector<DialogueNode>& nodes)
    {
        start = "greet";
        nodes.clear();

        DialogueNode greet;
        greet.id = "greet"; greet.speaker = "Alice"; greet.text = "Have we met before?";
        greet.portrait = "portraits/alice.png"; // exercises the portrait field round-trip
        greet.choices = {
            { "In the village.",  "remember", "",                "met_in_village" }, // gated
            { "I don't think so.", "intro",   "denied_meeting",  ""               }, // sets a flag
            { "[Leave]",           "",        "",                ""               }, // ends
        };
        DialogueNode intro;
        intro.id = "intro"; intro.speaker = "Alice"; intro.text = "Well met. I'm Alice."; intro.next = "outro";
        DialogueNode outro;
        outro.id = "outro"; outro.speaker = "Alice"; outro.text = "Be seeing you."; outro.next = "";
        DialogueNode remember;
        remember.id = "remember"; remember.speaker = "Alice"; remember.text = "Good to see you again!"; remember.next = "";

        nodes = { greet, intro, outro, remember };
    }

    std::map<std::string, DialogueNode> byId(const std::vector<DialogueNode>& v)
    {
        std::map<std::string, DialogueNode> m;
        for (const auto& n : v) m[n.id] = n;
        return m;
    }

    // Writes a dialogue JSON to <tmp>/dialogue/<name>.json and returns the project dir.
    std::filesystem::path writeTempDialogue(const nlohmann::json& j, const std::string& name)
    {
        static int counter = 0;
        std::filesystem::path dir =
            std::filesystem::temp_directory_path() / ("indium_dlg_test_" + std::to_string(counter++));
        std::filesystem::create_directories(dir / "dialogue");
        std::ofstream(dir / "dialogue" / (name + ".json")) << j.dump(2) << std::endl;
        return dir;
    }
}

TEST_CASE("dialogue document round-trips through ToJson/FromJson")
{
    std::string start;
    std::vector<DialogueNode> nodes;
    makeSampleDoc(start, nodes);

    // Save (panel format) then load it straight back.
    const nlohmann::json j = DialogueManager::ToJson(start, nodes);
    std::string start2;
    std::vector<DialogueNode> nodes2;
    DialogueManager::FromJson(j, start2, nodes2);

    CHECK(start2 == start);
    REQUIRE(nodes2.size() == nodes.size());

    // Node order isn't preserved (JSON object -> sorted keys), so compare by id.
    const auto a = byId(nodes);
    const auto b = byId(nodes2);
    for (const auto& [id, n] : a)
    {
        REQUIRE(b.count(id) == 1);
        const DialogueNode& m = b.at(id);
        CHECK(m.speaker  == n.speaker);
        CHECK(m.text     == n.text);
        CHECK(m.portrait == n.portrait);
        CHECK(m.setFlag  == n.setFlag);
        CHECK(m.next     == n.next);
        REQUIRE(m.choices.size() == n.choices.size());
        for (std::size_t i = 0; i < n.choices.size(); ++i)
        {
            CHECK(m.choices[i].text        == n.choices[i].text);
            CHECK(m.choices[i].next        == n.choices[i].next);
            CHECK(m.choices[i].setFlag     == n.choices[i].setFlag);
            CHECK(m.choices[i].requireFlag == n.choices[i].requireFlag);
        }
    }
}

TEST_CASE("empty-id nodes are dropped on save; optional empty fields are omitted")
{
    std::vector<DialogueNode> nodes;
    DialogueNode named;   named.id = "a"; named.text = "hi";              // kept
    DialogueNode unnamed; unnamed.id = "";  unnamed.text = "lost";       // dropped (no id)
    nodes = { named, unnamed };

    const nlohmann::json j = DialogueManager::ToJson("a", nodes);
    REQUIRE(j["nodes"].is_object());
    CHECK(j["nodes"].size() == 1);
    CHECK(j["nodes"].contains("a"));
    // Empty optionals must not be written (keeps authored files clean / matches runtime).
    CHECK_FALSE(j["nodes"]["a"].contains("next"));
    CHECK_FALSE(j["nodes"]["a"].contains("setFlag"));
    CHECK_FALSE(j["nodes"]["a"].contains("choices"));
    CHECK_FALSE(j["nodes"]["a"].contains("portrait"));
}

TEST_CASE("a panel-authored dialogue loads and branches at runtime")
{
    // Process-wide singletons: start clean and re-arm StoryState's NarrativeEvent
    // subscription (a dialogue choice's setFlag is recorded through it).
    StoryState::Get().Clear();
    StoryState::Get().SubscribeToEvents();

    std::string start;
    std::vector<DialogueNode> nodes;
    makeSampleDoc(start, nodes);
    const std::filesystem::path proj = writeTempDialogue(DialogueManager::ToJson(start, nodes), "test");

    auto& dm = DialogueManager::Get();
    dm.End();
    dm.SetProjectPath(proj.string());

    REQUIRE(dm.Start("test"));
    REQUIRE(dm.IsActive());
    REQUIRE(dm.Current() != nullptr);
    CHECK(dm.Current()->id == "greet");
    CHECK(dm.Current()->speaker == "Alice");

    SUBCASE("requireFlag hides a choice until its flag is set")
    {
        // "In the village." is gated on met_in_village -> only 2 of 3 visible.
        CHECK(dm.VisibleChoices().size() == 2);
        StoryState::Get().SetFlag("met_in_village");
        CHECK(dm.VisibleChoices().size() == 3);
    }

    SUBCASE("choosing a setFlag option records the flag and follows next")
    {
        // Visible order with the gate closed: [I don't think so.(0), [Leave](1)].
        dm.Choose(0);
        CHECK(StoryState::Get().HasFlag("denied_meeting"));
        REQUIRE(dm.Current() != nullptr);
        CHECK(dm.Current()->id == "intro");

        // Narration advances along `next` and ends on next == "".
        dm.Advance();
        REQUIRE(dm.Current() != nullptr);
        CHECK(dm.Current()->id == "outro");
        dm.Advance();
        CHECK_FALSE(dm.IsActive());
    }

    SUBCASE("a gated branch is reachable once its flag is set")
    {
        StoryState::Get().SetFlag("met_in_village");
        // Now visible: [In the village.(0), I don't think so.(1), [Leave](2)].
        dm.Choose(0);
        REQUIRE(dm.Current() != nullptr);
        CHECK(dm.Current()->id == "remember");
        dm.Advance(); // remember.next == "" -> ends
        CHECK_FALSE(dm.IsActive());
    }

    dm.End();
    std::error_code ec;
    std::filesystem::remove_all(proj, ec);
}

TEST_CASE("NormalizeStart keeps a valid start and repairs a missing one")
{
    std::vector<DialogueNode> nodes;
    DialogueNode a; a.id = "a";
    DialogueNode b; b.id = "b";
    nodes = { a, b };

    CHECK(DialogueManager::NormalizeStart("b", nodes) == "b");    // valid start preserved
    CHECK(DialogueManager::NormalizeStart("gone", nodes) == "a"); // missing id -> first node
    CHECK(DialogueManager::NormalizeStart("", nodes) == "a");     // empty -> first node

    std::vector<DialogueNode> none;
    CHECK(DialogueManager::NormalizeStart("x", none).empty());    // no nodes -> ""
}

TEST_CASE("a dialogue whose start references a missing node does not run")
{
    // The editor self-heals a deleted start (NormalizeStart), but a hand-edited file
    // can still ship a dangling start — the runtime must fail closed, not crash.
    StoryState::Get().Clear();
    StoryState::Get().SubscribeToEvents();

    std::string start;
    std::vector<DialogueNode> nodes;
    makeSampleDoc(start, nodes);
    nlohmann::json j = DialogueManager::ToJson(start, nodes);
    j["start"] = "ghost"; // no such node

    const std::filesystem::path proj = writeTempDialogue(j, "bad");

    auto& dm = DialogueManager::Get();
    dm.End();
    dm.SetProjectPath(proj.string());

    CHECK_FALSE(dm.Start("bad")); // GoTo(missing) -> End, so Start reports inactive
    CHECK_FALSE(dm.IsActive());

    std::error_code ec;
    std::filesystem::remove_all(proj, ec);
}

// --- Presentation-polish helpers (StoryState.hpp) -----------------------------------------
// StoryFormat / StoryInterpolate / StoryEval are the pure backbone of dialogue {interpolation}
// and requireFlag conditions. They read the global StoryState singleton, so each case clears
// it first. (The on-screen typewriter + portrait rendering need a GL context and are verified
// live in the editor, not here.)

TEST_CASE("StoryFormat and StoryInterpolate render StoryState values into text")
{
    auto& s = StoryState::Get();
    s.Clear();
    s.Set("coins",      StoryValue{ 10 });
    s.Set("playerName", StoryValue{ std::string("Zak") });
    s.Set("met",        StoryValue{ true });
    s.Set("hp",         StoryValue{ 3.5f });

    CHECK(StoryFormat(StoryValue{ 10 })                == "10");
    CHECK(StoryFormat(StoryValue{ true })              == "true");
    CHECK(StoryFormat(StoryValue{ false })             == "false");
    CHECK(StoryFormat(StoryValue{ std::string("hi") }) == "hi");
    CHECK(StoryFormat(StoryValue{ 3.5f })              == "3.5");

    CHECK(StoryInterpolate("Hi {playerName}, {coins} coins") == "Hi Zak, 10 coins");
    CHECK(StoryInterpolate("hp={hp} met={met}")              == "hp=3.5 met=true");
    CHECK(StoryInterpolate("you have {gold}")                == "you have {gold}"); // unknown -> literal
    CHECK(StoryInterpolate("{{coins}} is literal")           == "{coins} is literal"); // brace escape
}

TEST_CASE("StoryEval evaluates flags, comparisons, and AND/OR conditions")
{
    auto& s = StoryState::Get();
    s.Clear();
    s.Set("met",        StoryValue{ true });
    s.Set("coins",      StoryValue{ 10 });
    s.Set("hp",         StoryValue{ 3.5f });
    s.Set("playerName", StoryValue{ std::string("Zak") });
    s.Set("chapter",    StoryValue{ 3 });
    s.Set("ratio",      StoryValue{ 0.1f });   // not exactly representable as a float
    s.Set("has-sword",  StoryValue{ true });   // flag name that isn't a bare identifier

    // Empty == no gate.
    CHECK(StoryEval(""));
    CHECK(StoryEval("   "));

    // Bare flag + negation (back-compatible with the old requireFlag semantics).
    CHECK(StoryEval("met"));
    CHECK_FALSE(StoryEval("missing"));
    CHECK_FALSE(StoryEval("!met"));
    CHECK(StoryEval("!missing"));

    // A flag whose name isn't a plain identifier is matched whole, not truncated at '-'.
    CHECK(StoryEval("has-sword"));

    // Numeric comparisons across int/float, with type coercion.
    CHECK(StoryEval("coins >= 10"));
    CHECK_FALSE(StoryEval("coins > 10"));
    CHECK(StoryEval("coins == 10"));
    CHECK(StoryEval("coins != 5"));
    CHECK_FALSE(StoryEval("coins < 5"));
    CHECK(StoryEval("hp < 4"));
    CHECK(StoryEval("chapter == 3"));
    CHECK(StoryEval("chapter >= 2"));
    CHECK(StoryEval("ratio == 0.1"));        // tolerant float equality (no exact-bit match needed)
    CHECK_FALSE(StoryEval("ratio == 0.2"));

    // String comparisons (quoted and bareword).
    CHECK(StoryEval("playerName == \"Zak\""));
    CHECK_FALSE(StoryEval("playerName == \"Bob\""));
    CHECK(StoryEval("playerName != \"Bob\""));
    CHECK(StoryEval("playerName == Zak"));

    // AND / OR / parens / precedence (&& binds tighter than ||).
    CHECK(StoryEval("met && coins >= 10"));
    CHECK_FALSE(StoryEval("met && coins > 10"));
    CHECK(StoryEval("coins > 10 || met"));
    CHECK(StoryEval("!met || coins == 10"));
    CHECK(StoryEval("(coins < 5 || met) && coins == 10"));
    CHECK_FALSE(StoryEval("coins < 5 && met || missing")); // (false) || false
}

TEST_CASE("a choice gated on an expression condition appears only when it holds")
{
    StoryState::Get().Clear();
    StoryState::Get().SubscribeToEvents();

    std::vector<DialogueNode> nodes;
    DialogueNode shop;
    shop.id = "shop"; shop.speaker = "Merchant"; shop.text = "What'll it be?";
    shop.choices = {
        { "Buy the sword (10g)", "bought", "", "coins >= 10" }, // gated on an expression
        { "Just looking",        "",       "", ""             },
    };
    DialogueNode bought; bought.id = "bought"; bought.text = "Pleasure doing business.";
    nodes = { shop, bought };

    const std::filesystem::path proj = writeTempDialogue(DialogueManager::ToJson("shop", nodes), "shop");
    auto& dm = DialogueManager::Get();
    dm.End();
    dm.SetProjectPath(proj.string());
    REQUIRE(dm.Start("shop"));

    // Not enough coins -> only the ungated choice is visible.
    CHECK(dm.VisibleChoices().size() == 1);
    StoryState::Get().Set("coins", StoryValue{ 10 });
    CHECK(dm.VisibleChoices().size() == 2);
    StoryState::Get().Set("coins", StoryValue{ 5 });
    CHECK(dm.VisibleChoices().size() == 1);

    dm.End();
    std::error_code ec;
    std::filesystem::remove_all(proj, ec);
}
