#include "doctest.h"
#include "Exporter.hpp"
#include "AssetManager.hpp"
#include <filesystem>
#include <fstream>

using namespace Indium;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
    /** Self-cleaning temp dir per case, mirroring save_test's SaveSandbox. */
    struct ExportSandbox
    {
        fs::path dir;

        explicit ExportSandbox(const char* name)
        {
            dir = fs::temp_directory_path() / name;
            fs::remove_all(dir);
            fs::create_directories(dir);
        }
        ~ExportSandbox() { fs::remove_all(dir); }

        static void Write(const fs::path& p, const std::string& text)
        {
            fs::create_directories(p.parent_path());
            std::ofstream f(p);
            f << text;
        }

        /** A minimal but realistic project tree + a fake player binary. Returns
         *  {projectPath, playerBinaryPath}. */
        std::pair<std::string, std::string> MakeProject() const
        {
            fs::path proj = dir / "MyGame";
            Write(proj / "project.indp",
                  R"({"projectName":"My Game!","engineVersion":"1.0.0","defaultScene":"Scenes/main.scene"})");
            // A scene whose sprite uses an ABSOLUTE path inside the project (what the
            // editor's file browser stores) plus one already-relative path and one
            // absolute path OUTSIDE the project (e.g. a system font) that must survive.
            json scene = {
                { "entities", json::array({
                    { { "name", "Player" },
                      { "components", json::array({
                          { { "type", "SpriteRenderer" },
                            { "texturePath", (proj / "Assets" / "hero.png").string() } },
                          { { "type", "Text" },
                            { "fontPath", "/usr/share/fonts/external.ttf" } },
                          { { "type", "AudioSource" },
                            { "filePath", "Assets/step.wav" } }
                      }) } }
                }) }
            };
            Write(proj / "Scenes" / "main.scene", scene.dump(2));
            Write(proj / "Assets" / "hero.png", "png-bytes");
            Write(proj / "dialogue" / "intro.json", R"({"start":"a","nodes":{"a":{"text":"hi"}}})");
            Write(proj / "scripts" / "PlayerMovement.cpp", "// source, must not ship");
            Write(proj / "saves" / "slot_1.json", R"({"scene":"main"})");
            Write(proj / ".clangd", "CompileFlags:");
            Write(proj / "libscripts_42.dylib", "fake-dylib");
            Write(proj / "input.json", R"({"actions":{}})");

            fs::path player = dir / "IndiumPlayer";
            Write(player, "fake-player-binary");
            return { proj.string(), player.string() };
        }
    };
}

TEST_CASE("Exporter::RelativizePaths rewrites in-project absolutes only")
{
    const std::string root = "/tmp/proj";
    json j = {
        { "tex",  "/tmp/proj/Assets/hero.png" },          // inside  -> relative
        { "font", "/usr/share/fonts/a.ttf" },             // outside -> untouched
        { "rel",  "Assets/step.wav" },                    // already relative -> untouched
        { "rootSelf", "/tmp/proj" },                      // the root itself, not a child -> untouched
        { "sibling",  "/tmp/project-other/x.png" },       // prefix-sibling dir -> untouched
        { "num", 42 }, { "flag", true },
        { "nested", { { "deep", json::array({ "/tmp/proj/Scenes/a.scene", 7 }) } } }
    };

    json out = Exporter::RelativizePaths(j, root);
    CHECK(out["tex"]      == "Assets/hero.png");
    CHECK(out["font"]     == "/usr/share/fonts/a.ttf");
    CHECK(out["rel"]      == "Assets/step.wav");
    CHECK(out["rootSelf"] == "/tmp/proj");
    CHECK(out["sibling"]  == "/tmp/project-other/x.png");
    CHECK(out["num"]      == 42);
    CHECK(out["flag"]     == true);
    CHECK(out["nested"]["deep"][0] == "Scenes/a.scene");
    CHECK(out["nested"]["deep"][1] == 7);
}

TEST_CASE("Exporter::SanitizeName keeps filesystem-safe characters")
{
    CHECK(Exporter::SanitizeName("My Game!")       == "My Game");
    CHECK(Exporter::SanitizeName("a/b\\c:d")       == "abcd");
    CHECK(Exporter::SanitizeName("  spaced  ")     == "spaced");
    CHECK(Exporter::SanitizeName("dash-under_1")   == "dash-under_1");
    CHECK(Exporter::SanitizeName("!!!")            == "Game");
    CHECK(Exporter::SanitizeName("")               == "Game");
}

TEST_CASE("Exporter::ExportGame produces a complete, portable package")
{
    ExportSandbox sb("indium_export_test");
    auto [proj, player] = sb.MakeProject();

    Exporter::Result res = Exporter::ExportGame(proj, "My Game!", player, proj + "/Export");
    REQUIRE_MESSAGE(res.ok, res.error);

    fs::path out(res.outputDir);
    CHECK(out.filename().string() == "My Game");

    // Player binary renamed after the game (and executable on POSIX).
    fs::path exe = out / "My Game";
    REQUIRE(fs::exists(exe));
#if !defined(_WIN32)
    CHECK((fs::status(exe).permissions() & fs::perms::owner_exec) != fs::perms::none);
#endif

    // game.json parses and points at data/.
    {
        std::ifstream f(out / "game.json");
        REQUIRE(f.is_open());
        json g; f >> g;
        CHECK(g["title"]   == "My Game!");
        CHECK(g["project"] == "data");
    }

    // Shipped: scenes, assets, dialogue, input.json, the prebuilt script library.
    CHECK(fs::exists(out / "data" / "project.indp"));
    CHECK(fs::exists(out / "data" / "Scenes" / "main.scene"));
    CHECK(fs::exists(out / "data" / "Assets" / "hero.png"));
    CHECK(fs::exists(out / "data" / "dialogue" / "intro.json"));
    CHECK(fs::exists(out / "data" / "input.json"));
    CHECK(fs::exists(out / "data" / "libscripts_42.dylib"));

    // Not shipped: script sources, saves, IDE configs, the Export dir itself.
    CHECK(!fs::exists(out / "data" / "scripts"));
    CHECK(!fs::exists(out / "data" / "saves"));
    CHECK(!fs::exists(out / "data" / ".clangd"));
    CHECK(!fs::exists(out / "data" / "Export"));

    // The script library must be at least as new as the player binary, or
    // ScriptManager's ABI-staleness check refuses to load it.
    CHECK(fs::last_write_time(out / "data" / "libscripts_42.dylib") >= fs::last_write_time(exe));

    // The copied scene's absolute in-project path became relative; the external
    // font path and the already-relative audio path survived untouched.
    {
        std::ifstream f(out / "data" / "Scenes" / "main.scene");
        json s; f >> s;
        const auto& comps = s["entities"][0]["components"];
        CHECK(comps[0]["texturePath"] == "Assets/hero.png");
        CHECK(comps[1]["fontPath"]    == "/usr/share/fonts/external.ttf");
        CHECK(comps[2]["filePath"]    == "Assets/step.wav");
    }

    SUBCASE("re-export over a previous export succeeds")
    {
        Exporter::Result again = Exporter::ExportGame(proj, "My Game!", player, proj + "/Export");
        CHECK_MESSAGE(again.ok, again.error);
        CHECK(fs::exists(fs::path(again.outputDir) / "game.json"));
    }
}

TEST_CASE("Exporter::ExportGame refuses bad inputs")
{
    ExportSandbox sb("indium_export_refuse_test");
    auto [proj, player] = sb.MakeProject();

    SUBCASE("missing player binary")
    {
        Exporter::Result res = Exporter::ExportGame(proj, "G", (sb.dir / "nope").string(),
                                                    proj + "/Export");
        CHECK(!res.ok);
        CHECK(res.error.find("IndiumPlayer") != std::string::npos);
    }

    SUBCASE("not a project")
    {
        Exporter::Result res = Exporter::ExportGame((sb.dir / "empty").string(), "G", player,
                                                    proj + "/Export");
        CHECK(!res.ok);
    }

    SUBCASE("output dir exists but is not a previous export")
    {
        fs::path occupied = fs::path(proj) / "Export" / "G";
        ExportSandbox::Write(occupied / "precious.txt", "user data");
        Exporter::Result res = Exporter::ExportGame(proj, "G", player, proj + "/Export");
        CHECK(!res.ok);
        CHECK(fs::exists(occupied / "precious.txt"));   // nothing was deleted
    }
}

TEST_CASE("AssetManager resolves project-relative paths against the project root")
{
    ExportSandbox sb("indium_assetroot_test");
    ExportSandbox::Write(sb.dir / "proj" / "Assets" / "a.png", "x");

    auto& am = AssetManager::Get();
    am.SetProjectRoot((sb.dir / "proj").string());

    CHECK(am.ResolvePath("Assets/a.png") == (sb.dir / "proj" / "Assets" / "a.png").string());
    CHECK(am.ResolvePath("/abs/path.png") == "/abs/path.png");   // absolute passes through
    CHECK(am.ResolvePath("") == "");

    am.SetProjectRoot("");
    CHECK(am.ResolvePath("Assets/a.png") == "Assets/a.png");     // no root -> untouched
}
