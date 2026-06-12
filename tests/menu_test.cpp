#include "doctest.h"
#include "MenuManager.hpp"
#include "GameSettings.hpp"
#include "AudioMixer.hpp"
#include "InputManager.hpp"
#include <filesystem>
#include <fstream>

using namespace Indium;
namespace fs = std::filesystem;
using Page = MenuManager::Page;

// The Menu/Settings/Mixer/Input singletons are process-wide and shared across cases,
// so put every store this suite touches back into a known state at the start of each
// case. GameSettings::Load with an empty project path is a no-op that clears dirty.
static void freshMenus()
{
    MenuManager::Get().Reset();
    GameSettings::Get().SetProjectPath("");
    GameSettings::Get().Load();

    auto& mix  = AudioMixer::Get();
    mix.master = 1.0f;
    for (auto& [name, v] : mix.buses) v = 1.0f;

    auto& im = InputManager::Get();
    im.GetActions().clear();
    im.SetAction("Jump", KEY_SPACE);
    im.SetAction("Interact", KEY_E);
    im.SetMouseAction("Fire", MOUSE_BUTTON_LEFT);
}

TEST_CASE("menu pages open, close and report blocking")
{
    freshMenus();
    auto& m = MenuManager::Get();

    CHECK(m.Current() == Page::None);
    CHECK_FALSE(m.IsOpen());
    CHECK_FALSE(m.BlocksGameplay());

    m.OpenTitle();
    CHECK(m.Current() == Page::Title);
    CHECK(m.IsOpen());
    CHECK(m.BlocksGameplay());

    m.OpenPause();
    CHECK(m.Current() == Page::Pause);
    CHECK(m.BlocksGameplay());

    m.Resume();
    CHECK(m.Current() == Page::None);
    CHECK_FALSE(m.BlocksGameplay());

    m.OpenPause();
    m.StartRebind("Jump");
    m.Reset();
    CHECK(m.Current() == Page::None);
    CHECK(m.RebindingAction().empty());
}

TEST_CASE("settings returns to whichever page opened it")
{
    freshMenus();
    auto& m = MenuManager::Get();

    m.OpenPause();
    m.OpenSettings();
    CHECK(m.Current() == Page::Settings);
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Pause);

    m.OpenTitle();
    m.OpenSettings();
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Title);

    // Opened from a script with nothing up: Back/Esc closes the menu entirely.
    m.Reset();
    m.OpenSettings();
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::None);
}

TEST_CASE("escape layers innermost-first: rebind, settings, pause, then nothing")
{
    freshMenus();
    auto& m = MenuManager::Get();

    m.OpenPause();
    m.OpenSettings();
    m.StartRebind("Jump");

    // 1st Esc only cancels the armed rebind — still on Settings.
    CHECK(m.OnEscape());
    CHECK(m.RebindingAction().empty());
    CHECK(m.Current() == Page::Settings);

    // 2nd backs out of Settings to Pause; 3rd resumes; a 4th is not consumed
    // (that's the editor's cue to open the pause menu).
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Pause);
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::None);
    CHECK_FALSE(m.OnEscape());

    // The title screen consumes Esc but stays put (deliberately inert).
    m.OpenTitle();
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Title);
}

TEST_CASE("settings round-trip restores audio volumes and rebinds")
{
    freshMenus();
    const fs::path dir = fs::temp_directory_path() / "indium_menu_test_roundtrip";
    fs::create_directories(dir);

    auto& gs  = GameSettings::Get();
    auto& mix = AudioMixer::Get();
    auto& im  = InputManager::Get();
    gs.SetProjectPath(dir.string());

    mix.master = 0.5f;
    mix.SetBusVolume("Music", 0.25f);
    im.SetAction("Jump", KEY_W);
    gs.MarkDirty();
    CHECK(gs.IsDirty());
    CHECK(gs.Save());
    CHECK_FALSE(gs.IsDirty());   // Save clears dirty

    // Mutate the live state, then Load must restore the saved values.
    mix.master = 1.0f;
    mix.SetBusVolume("Music", 1.0f);
    im.SetAction("Jump", KEY_SPACE);
    CHECK(gs.Load());
    CHECK(mix.master == doctest::Approx(0.5f));
    CHECK(mix.BusVolume("Music") == doctest::Approx(0.25f));
    CHECK(im.GetActions().at("Jump").key == KEY_W);

    fs::remove_all(dir);
}

TEST_CASE("missing settings file leaves the authored action map intact")
{
    freshMenus();
    auto& gs = GameSettings::Get();
    gs.SetProjectPath((fs::temp_directory_path() / "indium_menu_test_missing").string());

    CHECK_FALSE(gs.Load());   // no such directory / file
    const auto& actions = InputManager::Get().GetActions();
    CHECK(actions.size() == 3);
    CHECK(actions.at("Jump").key == KEY_SPACE);
    CHECK(AudioMixer::Get().master == doctest::Approx(1.0f));
}

TEST_CASE("partial settings only override named actions that already exist")
{
    freshMenus();
    const fs::path dir = fs::temp_directory_path() / "indium_menu_test_partial";
    fs::create_directories(dir);
    {
        // No "audio" section at all; input names one real action and one unknown.
        std::ofstream out(dir / "settings.json");
        out << R"({ "input": { "actions": {
                  "Jump":  { "key": 87, "mouseBtn": -1, "useMouse": false },
                  "Ghost": { "key": 65, "mouseBtn": -1, "useMouse": false }
              } } })";
    }

    auto& mix  = AudioMixer::Get();
    mix.master = 0.7f;   // must survive a file with no audio section
    auto& gs   = GameSettings::Get();
    gs.SetProjectPath(dir.string());
    CHECK(gs.Load());

    const auto& actions = InputManager::Get().GetActions();
    CHECK(actions.at("Jump").key == KEY_W);                       // named + existing: overridden
    CHECK(actions.at("Interact").key == KEY_E);                   // not named: untouched
    CHECK(actions.at("Fire").useMouse);                           // not named: untouched
    CHECK(actions.count("Ghost") == 0);                           // unknown: never added
    CHECK(mix.master == doctest::Approx(0.7f));                   // absent section: untouched

    fs::remove_all(dir);
}

TEST_CASE("save/load slot pages open from pause or title and Esc returns to the opener")
{
    freshMenus();
    auto& m = MenuManager::Get();

    m.OpenPause();
    m.OpenSaveGame();
    CHECK(m.Current() == Page::SaveGame);
    CHECK(m.BlocksGameplay());
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Pause);

    m.OpenLoadGame();
    CHECK(m.Current() == Page::LoadGame);
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Pause);

    m.OpenTitle();
    m.OpenLoadGame();
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Title);

    // Full layering from a slot page: back to Pause, resume, then unconsumed.
    m.OpenPause();
    m.OpenLoadGame();
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::Pause);
    CHECK(m.OnEscape());
    CHECK(m.Current() == Page::None);
    CHECK_FALSE(m.OnEscape());
}

TEST_CASE("pending save/load actions are taken exactly once and cleared by Reset")
{
    freshMenus();
    auto& m = MenuManager::Get();
    using Type = MenuManager::MenuAction::Type;

    CHECK(m.TakePendingAction().type == Type::None);

    m.RequestSave(2);
    MenuManager::MenuAction a = m.TakePendingAction();
    CHECK(a.type == Type::Save);
    CHECK(a.slot == 2);
    CHECK(m.TakePendingAction().type == Type::None);   // taken once

    m.RequestLoad(0);
    a = m.TakePendingAction();
    CHECK(a.type == Type::Load);
    CHECK(a.slot == 0);

    m.RequestLoad(1);
    m.Reset();                                         // Play start / Stop
    CHECK(m.TakePendingAction().type == Type::None);
}

TEST_CASE("menu save/load customization clamps and resets to defaults")
{
    freshMenus();
    auto& m = MenuManager::Get();

    CHECK(m.AllowManualSave());
    CHECK(m.AllowLoad());
    CHECK(m.ManualSlotCount() == 3);

    m.SetAllowManualSave(false);
    m.SetAllowLoad(false);
    m.SetManualSlotCount(99);
    CHECK_FALSE(m.AllowManualSave());
    CHECK_FALSE(m.AllowLoad());
    CHECK(m.ManualSlotCount() == 8);    // clamped
    m.SetManualSlotCount(0);
    CHECK(m.ManualSlotCount() == 1);    // clamped

    m.Reset();                          // a new Play must not inherit the last game's setup
    CHECK(m.AllowManualSave());
    CHECK(m.AllowLoad());
    CHECK(m.ManualSlotCount() == 3);
}
