#include "doctest.h"
#include "2D/component/MeshRendererComponent.hpp"
#include "2D/entity/EntityFactory.hpp"
#include "scene/Scene.hpp"

using namespace Indium;

// These cases exercise only the data layer (serialize / deserialize / clone / factory
// dispatch). The test harness runs headless with no GL context, so nothing here may
// trigger a GPU upload: a missing model path makes AssetManager::GetModel return a
// 0-mesh model (no UploadMesh), and RenderIfNeeded()/the render target are never touched.
// Silence the expected "failed to load model" warnings for missing paths.
static void quietLog() { SetTraceLogLevel(LOG_ERROR); }

TEST_CASE("MeshRenderer serializes all authored fields")
{
    MeshRendererComponent m;
    m.modelPath     = "hero.glb";
    m.tint          = { 255, 128, 64, 200 };
    m.eulerRotation = { 15.0f, 30.0f, 45.0f };
    m.modelScale    = 1.5f;
    m.rtResolution  = 384;

    nlohmann::json j = m.serialize();
    CHECK(j["type"]         == "MeshRenderer");
    CHECK(j["modelPath"]    == "hero.glb");
    CHECK(j["modelScale"].get<float>() == doctest::Approx(1.5f));
    CHECK(j["rtResolution"].get<int>() == 384);
    CHECK(j["tint"][0].get<int>() == 255);
    CHECK(j["tint"][3].get<int>() == 200);
    CHECK(j["eulerRotation"][1].get<float>() == doctest::Approx(30.0f));
}

TEST_CASE("MeshRenderer deserialize round-trips its fields")
{
    quietLog();
    MeshRendererComponent src;
    src.modelPath     = "ghost.glb";   // intentionally missing — stays unloaded, no GL
    src.tint          = { 10, 20, 30, 40 };
    src.eulerRotation = { 5.0f, 6.0f, 7.0f };
    src.modelScale    = 2.5f;
    src.rtResolution  = 128;

    MeshRendererComponent dst;
    dst.deserialize(src.serialize());

    CHECK(dst.modelPath        == "ghost.glb");
    CHECK(dst.tint.r           == 10);
    CHECK(dst.tint.a           == 40);
    CHECK(dst.eulerRotation.z  == doctest::Approx(7.0f));
    CHECK(dst.modelScale       == doctest::Approx(2.5f));
    CHECK(dst.rtResolution     == 128);
    CHECK(dst.getName()        == "MeshRenderer");
}

TEST_CASE("MeshRenderer clone copies fields but not GPU runtime state")
{
    MeshRendererComponent a;
    a.modelPath     = "x.glb";
    a.tint          = { 1, 2, 3, 4 };
    a.eulerRotation = { 9.0f, 8.0f, 7.0f };
    a.modelScale    = 4.0f;
    a.rtResolution  = 512;
    // Pretend the original is a live, rendered instance.
    a.modelLoaded   = true;
    a.rtValid_      = true;   // FreeTarget_ guards on IsWindowReady(), so this is safe headless
    a.dirty_        = false;

    auto clone = a.clone();
    auto* b = dynamic_cast<MeshRendererComponent*>(clone.get());
    REQUIRE(b != nullptr);

    // Authored fields copied.
    CHECK(b->modelPath       == "x.glb");
    CHECK(b->tint.g          == 2);
    CHECK(b->eulerRotation.x == doctest::Approx(9.0f));
    CHECK(b->modelScale      == doctest::Approx(4.0f));
    CHECK(b->rtResolution    == 512);

    // GPU-owned runtime state must NOT be shared — the clone rebuilds lazily.
    CHECK(b->rtValid_    == false);
    CHECK(b->dirty_      == true);
    CHECK(b->modelLoaded == false);
}

TEST_CASE("EntityFactory reconstructs a MeshRenderer from scene JSON")
{
    quietLog();
    EntityFactory factory;
    nlohmann::json j = {
        { "type", "Rectangle" },
        { "components", nlohmann::json::array({
            { { "type", "MeshRenderer" }, { "modelPath", "ghost.glb" },
              { "modelScale", 3.0f },     { "rtResolution", 64 } }
        }) }
    };

    auto entity = factory.LoadEntity(j);
    REQUIRE(entity != nullptr);

    MeshRendererComponent* mesh = nullptr;
    for (auto& c : entity->components)
        if (c->getName() == "MeshRenderer") mesh = dynamic_cast<MeshRendererComponent*>(c.get());

    REQUIRE(mesh != nullptr);
    CHECK(mesh->modelPath    == "ghost.glb");
    CHECK(mesh->modelScale   == doctest::Approx(3.0f));
    CHECK(mesh->rtResolution == 64);
}
