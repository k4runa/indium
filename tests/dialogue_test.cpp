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
        CHECK(m.speaker == n.speaker);
        CHECK(m.text    == n.text);
        CHECK(m.setFlag == n.setFlag);
        CHECK(m.next    == n.next);
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
