#include "doctest.h"
#include "SaveManager.hpp"
#include "StoryState.hpp"
#include "EventBus.hpp"
#include "scene/Scene.hpp"
#include "../core/Entity.hpp"
#include <filesystem>
#include <fstream>

using namespace Indium;
namespace fs = std::filesystem;

// SaveManager state (project path, autosave config) is process-wide, so each case
// points it at its own temp dir and restores a clean slate at the end. The autosave
// condition cases also re-arm the StoryStateChangedEvent subscription, since another
// test's EventBus::Clear() may have wiped it (same trap as quest_test's freshStart).
namespace
{
    struct SaveSandbox
    {
        fs::path dir;

        explicit SaveSandbox(const char* name)
        {
            dir = fs::temp_directory_path() / name;
            fs::remove_all(dir);
            fs::create_directories(dir);
            SaveManager::SetProjectPath(dir.string());
            SaveManager::ResetAutosaveConfig();
            StoryState::Get().Clear();
        }

        ~SaveSandbox()
        {
            SaveManager::SetProjectPath("");
            SaveManager::ResetAutosaveConfig();
            StoryState::Get().Clear();
            fs::remove_all(dir);
        }

        fs::path slotPath(int slot) const
        {
            return dir / "saves" / ("slot_" + std::to_string(slot) + ".json");
        }

        // Rewrites a slot file's savedAt so ordering tests don't need to sleep.
        void patchSavedAt(int slot, long long savedAt) const
        {
            nlohmann::json j;
            { std::ifstream in(slotPath(slot)); in >> j; }
            j["savedAt"] = savedAt;
            { std::ofstream out(slotPath(slot)); out << j; }
        }
    };

    Scene makeScene(const char* name)
    {
        Scene scene;
        scene.name = name;
        auto player = std::make_unique<Entity>();
        player->id       = 7;
        player->tag      = "Player";
        player->position = { 12.0f, 34.0f };
        scene.entities.push_back(std::move(player));
        return scene;
    }
}

TEST_CASE("Save stamps savedAt and ListSlots reports slot/scene/savedAt sorted")
{
    SaveSandbox sb("indium_save_test_list");
    Scene scene = makeScene("level1");

    CHECK(SaveManager::Save(scene, 2));
    scene.name = "level2";
    CHECK(SaveManager::Save(scene, 0));

    const auto slots = SaveManager::ListSlots();
    REQUIRE(slots.size() == 2);
    CHECK(slots[0].slot == 0);          // sorted by slot, not write order
    CHECK(slots[0].scene == "level2");
    CHECK(slots[0].savedAt > 0);
    CHECK(slots[1].slot == 2);
    CHECK(slots[1].scene == "level1");
    CHECK(slots[1].savedAt > 0);

    nlohmann::json j;
    { std::ifstream in(sb.slotPath(2)); in >> j; }
    CHECK(j.contains("savedAt"));
    CHECK(j["savedAt"].get<long long>() > 0);
}

TEST_CASE("legacy save without savedAt still loads and lists via mtime fallback")
{
    SaveSandbox sb("indium_save_test_legacy");
    fs::create_directories(sb.dir / "saves");
    {
        std::ofstream out(sb.slotPath(2));
        out << R"({ "scene": "oldlevel", "storyState": {}, "players": [] })";
    }

    Scene scene = makeScene("level1");
    CHECK(SaveManager::Load(scene, 2));
    CHECK(scene._hasPendingRestore);
    CHECK(scene._pendingSceneLoad == "oldlevel");

    const auto slots = SaveManager::ListSlots();
    REQUIRE(slots.size() == 1);
    CHECK(slots[0].slot == 2);
    CHECK(slots[0].scene == "oldlevel");
    CHECK(slots[0].savedAt > 0);        // fell back to the file's mtime
}

TEST_CASE("MostRecentSlot picks the newest savedAt across autosave and manual slots")
{
    SaveSandbox sb("indium_save_test_recent");
    Scene scene = makeScene("level1");

    CHECK(SaveManager::Save(scene, 0));
    CHECK(SaveManager::Save(scene, 2));
    sb.patchSavedAt(0, 1000);
    sb.patchSavedAt(2, 2000);
    CHECK(SaveManager::MostRecentSlot() == 2);

    sb.patchSavedAt(0, 3000);
    CHECK(SaveManager::MostRecentSlot() == 0);
}

TEST_CASE("DeleteSlot removes the save; no saves means no most-recent slot")
{
    SaveSandbox sb("indium_save_test_delete");
    Scene scene = makeScene("level1");

    CHECK(SaveManager::MostRecentSlot() == -1);   // saves/ doesn't even exist yet

    CHECK(SaveManager::Save(scene, 1));
    REQUIRE(SaveManager::ListSlots().size() == 1);
    CHECK(SaveManager::DeleteSlot(1));
    CHECK(SaveManager::ListSlots().empty());
    CHECK(SaveManager::MostRecentSlot() == -1);
    CHECK_FALSE(SaveManager::DeleteSlot(1));      // already gone
}

TEST_CASE("empty project path fails gracefully")
{
    SaveManager::SetProjectPath("");
    SaveManager::ResetAutosaveConfig();
    Scene scene = makeScene("level1");

    CHECK_FALSE(SaveManager::Save(scene, 0));
    CHECK_FALSE(SaveManager::Load(scene, 0));
    CHECK(SaveManager::ListSlots().empty());
    CHECK(SaveManager::MostRecentSlot() == -1);
}

TEST_CASE("FormatTimestamp formats epochs and rejects zero")
{
    CHECK(SaveManager::FormatTimestamp(0) == "");
    CHECK(SaveManager::FormatTimestamp(-5) == "");
    const std::string s = SaveManager::FormatTimestamp(1765000000);
    CHECK_FALSE(s.empty());
    CHECK(s.find("20") == 0);           // starts with a year
}

TEST_CASE("autosave config resets to defaults and the master switch gates requests")
{
    SaveSandbox sb("indium_save_test_config");

    SaveManager::SetAutosaveEnabled(false);
    SaveManager::SetAutosaveOnSceneSwitch(false);
    SaveManager::SetAutosaveSlot(5);
    SaveManager::ResetAutosaveConfig();
    CHECK(SaveManager::AutosaveEnabled());
    CHECK(SaveManager::AutosaveOnSceneSwitch());
    CHECK(SaveManager::AutosaveSlot() == SaveManager::kAutosaveSlot);

    // Disabled = RequestAutosave is a no-op, nothing to consume.
    SaveManager::SetAutosaveEnabled(false);
    SaveManager::RequestAutosave();
    CHECK_FALSE(SaveManager::ConsumeAutosaveRequest());

    // Enabled = one request, consumed exactly once.
    SaveManager::SetAutosaveEnabled(true);
    SaveManager::RequestAutosave();
    CHECK(SaveManager::ConsumeAutosaveRequest());
    CHECK_FALSE(SaveManager::ConsumeAutosaveRequest());

    // A pending request does not survive ResetAutosaveConfig.
    SaveManager::RequestAutosave();
    SaveManager::ResetAutosaveConfig();
    CHECK_FALSE(SaveManager::ConsumeAutosaveRequest());
}

TEST_CASE("autosave conditions edge-fire on StoryState changes and re-arm")
{
    SaveSandbox sb("indium_save_test_conditions");
    EventBus::Get().Clear();
    SaveManager::SubscribeToEvents();

    SaveManager::AddAutosaveCondition("coins >= 5");

    StoryState::Get().Set("coins", 3);
    CHECK_FALSE(SaveManager::ConsumeAutosaveRequest());

    StoryState::Get().Set("coins", 7);              // false -> true: fires
    CHECK(SaveManager::ConsumeAutosaveRequest());

    StoryState::Get().Set("coins", 8);              // still true: no re-fire
    CHECK_FALSE(SaveManager::ConsumeAutosaveRequest());

    StoryState::Get().Set("coins", 2);              // re-arms
    StoryState::Get().Set("coins", 9);              // fires again
    CHECK(SaveManager::ConsumeAutosaveRequest());

    // A condition that is already true when added doesn't fire until it cycles.
    SaveManager::AddAutosaveCondition("coins >= 1");
    StoryState::Get().Set("coins", 10);
    CHECK_FALSE(SaveManager::ConsumeAutosaveRequest());

    EventBus::Get().Clear();
}
