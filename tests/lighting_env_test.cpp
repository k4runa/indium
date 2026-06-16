#include "doctest.h"
#include "LightingEnvironment.hpp"
#include "scene/Scene.hpp"
#include "2D/component/Light2DComponent.hpp"
#include "2D/component/SpriteRendererComponent.hpp"
#include <memory>

using namespace Indium;

// Data-layer only: LightingEnvironment::Gather / LayerZ touch no GL, and the sprite
// normal-map round-trip uses a missing path (AssetManager::GetTexture returns id 0 with
// no upload), so this is headless-safe like mesh_test.
static void quiet() { SetTraceLogLevel(LOG_ERROR); }

static Entity* addLight(Scene& s, Vector2 pos, int layer)
{
    auto e = std::make_unique<Entity>();
    e->position   = pos;
    e->depthLayer = layer;
    e->addComponent<Light2DComponent>();   // Point, intensity 1 by default
    Entity* ptr = e.get();
    s.entities.push_back(std::move(e));
    return ptr;
}

TEST_CASE("LayerZ maps parallax depth to a signed Z (independent of the scroll switch)")
{
    Scene s;
    // Deliberately NOT enabling parallax scrolling: depth layers must still separate in Z
    // for lighting, otherwise moving a light to another layer changes nothing.
    CHECK_FALSE(s.parallaxEnabled);

    CHECK(LightingEnvironment::LayerZ(s, 0) == doctest::Approx(0.0f));
    // Foreground layers (parallax factor > 1) sit toward the viewer (-Z)...
    CHECK(LightingEnvironment::LayerZ(s, 1) < 0.0f);
    // ...background layers (factor < 1) sit deeper into the screen (+Z).
    CHECK(LightingEnvironment::LayerZ(s, -1) > 0.0f);
    // Monotonic: layer 2 is nearer (more negative) than layer 1.
    CHECK(LightingEnvironment::LayerZ(s, 2) < LightingEnvironment::LayerZ(s, 1));

    // An explicit per-layer override is honored even with scrolling off.
    s.parallaxByLayer[3] = 2.0f;
    CHECK(LightingEnvironment::LayerZ(s, 3) == doctest::Approx(LightingEnvironment::kZScale * (1.0f - 2.0f)));
}

TEST_CASE("Gather collects enabled lights with world position")
{
    Scene s;
    auto& env = LightingEnvironment::Get();

    env.Gather(s);
    CHECK_FALSE(env.Active());
    CHECK(env.Lights().empty());

    addLight(s, { 10.0f, 20.0f }, 0);
    s.RebuildHierarchy();
    env.Gather(s);

    CHECK(env.Active());
    REQUIRE(env.Lights().size() == 1);
    CHECK(env.Lights()[0].pos.x == doctest::Approx(10.0f));
    CHECK(env.Lights()[0].pos.y == doctest::Approx(20.0f));
    CHECK(env.Lights()[0].type == 0);          // Point
    CHECK(env.Lights()[0].intensity > 0.0f);
}

TEST_CASE("Gather caps at kMaxLights")
{
    Scene s;
    for (int i = 0; i < kMaxLights + 8; ++i) addLight(s, { (float)i, 0.0f }, 0);
    s.RebuildHierarchy();

    LightingEnvironment::Get().Gather(s);
    CHECK((int)LightingEnvironment::Get().Lights().size() == kMaxLights);
}

TEST_CASE("Sprite normal-map path round-trips through serialize")
{
    quiet();
    SpriteRendererComponent sp;
    sp.normalPath = "hero_normal.png";

    nlohmann::json j = sp.serialize();
    REQUIRE(j.contains("normalPath"));
    CHECK(j["normalPath"] == "hero_normal.png");

    SpriteRendererComponent dst;
    dst.deserialize(j);
    CHECK(dst.normalPath == "hero_normal.png");
}
