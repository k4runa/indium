#include "doctest.h"
#include "../core/Entity.hpp"
#include "scene/Scene.hpp"
#include "2D/component/RigidbodyComponent.hpp"
#include "2D/component/TilemapComponent.hpp"
#include "2D/component/Collider2D.hpp"
// EntityFactory drags in editor-UI component headers that use FontAwesome icon
// macros; in the editor build rlImGui.h supplies them first, so include the icon
// header here before the factory to keep the (non-UI) test build self-contained.
#include "extras/IconsFontAwesome6.h"
#include "2D/entity/EntityFactory.hpp"
#include "core/NativeScript.hpp"
#include "core/StoryState.hpp"
#include "core/EventBus.hpp"

// ---------------------------------------------------------------------------
// Test 1: Scene serialize -> deserialize -> serialize produces identical JSON
// ---------------------------------------------------------------------------
TEST_CASE("Scene round-trip serialize/deserialize")
{
    struct MinimalFactory
    {
        std::unique_ptr<Indium::Entity> LoadEntity(const nlohmann::json& j)
        {
            auto e = std::make_unique<Indium::Entity>();
            e->deserialize(j);
            return e;
        }
        void RebuildEntityCounts(Indium::Scene&) {}
    };

    Indium::Scene scene1;
    scene1.worldSize          = {1280.0f, 720.0f};
    scene1.editorCameraTarget = {50.0f,   25.0f};
    scene1.editorCameraZoom   = 1.5f;
    scene1.nextEntityId       = 5;

    auto e    = std::make_unique<Indium::Entity>();
    e->id       = 1;
    e->name     = "Hero";
    e->position = {100.0f, 200.0f};
    e->scale    = {2.0f,   2.0f};
    scene1.entities.push_back(std::move(e));

    auto j1 = scene1.serialize();

    Indium::Scene scene2;
    MinimalFactory factory;
    scene2.deserialize(j1, factory);

    CHECK(j1 == scene2.serialize());
}

// ---------------------------------------------------------------------------
// Test 2: Entity::clone() preserves all scalar fields and component values
// ---------------------------------------------------------------------------
TEST_CASE("Entity clone preserves fields and components")
{
    Indium::Entity src;
    src.id           = 7;
    src.name         = "Bullet";
    src.position     = {300.0f, 150.0f};
    src.scale        = {4.0f,   4.0f};
    src.rotation     = 45.0f;
    src.velocity     = {10.0f, -5.0f};
    src.tag          = "Projectile";
    src.layer        = 3;
    src.isStatic     = true;
    src.sortingOrder = 2;
    src.depthLayer   = 1;
    src.yPivotOffset = 8.0f;

    auto* rb       = src.addComponent<Indium::RigidbodyComponent>();
    rb->mass       = 2.5f;
    rb->bounciness = 0.8f;

    auto dst = src.clone();

    CHECK(dst->id           == src.id);
    CHECK(dst->name         == src.name);
    CHECK(dst->position.x   == src.position.x);
    CHECK(dst->position.y   == src.position.y);
    CHECK(dst->scale.x      == src.scale.x);
    CHECK(dst->rotation     == src.rotation);
    CHECK(dst->velocity.x   == src.velocity.x);
    CHECK(dst->tag          == src.tag);
    CHECK(dst->layer        == src.layer);
    CHECK(dst->isStatic     == src.isStatic);
    CHECK(dst->sortingOrder == src.sortingOrder);
    CHECK(dst->depthLayer   == src.depthLayer);
    CHECK(dst->yPivotOffset == src.yPivotOffset);

    REQUIRE(dst->components.size() == 1);
    auto* rbDst = dst->getComponent<Indium::RigidbodyComponent>();
    REQUIRE(rbDst != nullptr);
    CHECK(rbDst->mass       == 2.5f);
    CHECK(rbDst->bounciness == 0.8f);
    CHECK(rbDst->owner      == dst.get());
}

// ---------------------------------------------------------------------------
// Test 3: ResolveScene pushes two overlapping dynamic bodies apart
// ---------------------------------------------------------------------------
TEST_CASE("RigidbodyComponent resolves overlapping AABB pair")
{
    Indium::Scene scene;

    auto e1 = std::make_unique<Indium::Entity>();
    auto e2 = std::make_unique<Indium::Entity>();

    // 50x50 boxes placed 20 px apart — overlap of 30 px
    e1->id = 1; e1->position = {100.0f, 100.0f}; e1->scale = {50.0f, 50.0f};
    e2->id = 2; e2->position = {120.0f, 100.0f}; e2->scale = {50.0f, 50.0f};

    e1->addComponent<Indium::RigidbodyComponent>();
    e2->addComponent<Indium::RigidbodyComponent>();

    scene.entities.push_back(std::move(e1));
    scene.entities.push_back(std::move(e2));

    float gapBefore = scene.entities[1]->position.x - scene.entities[0]->position.x;

    Indium::RigidbodyComponent::ResolveScene(&scene, 1.0f / 60.0f);

    float gapAfter = scene.entities[1]->position.x - scene.entities[0]->position.x;
    CHECK(gapAfter > gapBefore);
}

// ---------------------------------------------------------------------------
// Test 4: Subscribing inside a handler does not crash; new sub fires on the
//         next Publish (EventBus frozen-count guarantee)
// ---------------------------------------------------------------------------
TEST_CASE("EventBus subscribe during dispatch fires on next publish")
{
    struct PingEvent { int n; };

    Indium::EventBus::Get().Clear();

    int count = 0;
    bool innerRegistered = false;
    Indium::SubscriptionHandle outer, inner;

    outer = Indium::EventBus::Get().Subscribe<PingEvent>([&](const PingEvent&)
    {
        count++;
        if (!innerRegistered)
        {
            innerRegistered = true;
            inner = Indium::EventBus::Get().Subscribe<PingEvent>([&](const PingEvent&) { count++; });
        }
    });

    Indium::EventBus::Get().Publish(PingEvent{1});
    CHECK(count == 1); // inner registered during dispatch — frozen count means it fires next time

    Indium::EventBus::Get().Publish(PingEvent{2});
    CHECK(count == 3); // outer + inner both fire

    Indium::EventBus::Get().Clear();
}

// ---------------------------------------------------------------------------
// Test 5: StoryState::Seed overwrites runtime values; unrelated keys survive
// ---------------------------------------------------------------------------
TEST_CASE("StoryState Seed overwrites runtime values with authored values")
{
    auto& ss = Indium::StoryState::Get();
    ss.Clear();

    ss.Set("score", Indium::StoryValue{42});
    ss.Set("lives", Indium::StoryValue{3});

    std::map<std::string, Indium::StoryValue> authored = {
        {"score", Indium::StoryValue{0}},  // overwrite existing
        {"level", Indium::StoryValue{1}},  // add new key
    };

    ss.Seed(authored);

    CHECK(ss.GetInt("score") == 0); // authored wins over runtime
    CHECK(ss.GetInt("lives") == 3); // unrelated key unchanged
    CHECK(ss.GetInt("level") == 1); // new authored key seeded

    ss.Clear();
}

// ---------------------------------------------------------------------------
// Test 6: Destroying a parent removes its whole subtree without dereferencing a
//         freed parent. collectSubtree gathers the subtree parent-first, so the
//         flush must free it leaf-first; freeing parent-first dangles each
//         child's `parent` pointer (use-after-free at the unlink and in any
//         component destroy()). Run under AddressSanitizer to catch the
//         regression deterministically — the logical CHECKs below pass either
//         way, but ASan reports the freed-memory access on the unfixed code.
// ---------------------------------------------------------------------------
TEST_CASE("Scene destroy frees an entity subtree without dangling parent links")
{
    Indium::Scene scene;

    auto p = std::make_unique<Indium::Entity>(); p->id = 1; // parent
    auto c = std::make_unique<Indium::Entity>(); c->id = 2; // child of p
    auto g = std::make_unique<Indium::Entity>(); g->id = 3; // grandchild (child of c)
    auto s = std::make_unique<Indium::Entity>(); s->id = 4; // unrelated, no parent

    Indium::Entity* pPtr = p.get();
    Indium::Entity* cPtr = c.get();
    Indium::Entity* gPtr = g.get();

    cPtr->setParent(pPtr);
    gPtr->setParent(cPtr);

    scene.entities.push_back(std::move(p));
    scene.entities.push_back(std::move(c));
    scene.entities.push_back(std::move(g));
    scene.entities.push_back(std::move(s));

    scene.DestroyEntity(1);     // destroy the root of the subtree
    scene.Update(1.0f / 60.0f); // destroy queue is flushed at the end of Update

    CHECK(scene.FindEntity(1) == nullptr);
    CHECK(scene.FindEntity(2) == nullptr);
    CHECK(scene.FindEntity(3) == nullptr);
    REQUIRE(scene.FindEntity(4) != nullptr); // unrelated sibling survives
    CHECK(scene.entities.size() == 1);
    CHECK(scene.FindEntity(4)->parent == nullptr);
}

// ---------------------------------------------------------------------------
// Test 7: A parent and one of its children both queued in the same frame
//         (collectSubtree expands the child's subtree twice → duplicate IDs in
//         toRemove) must still resolve to a clean, crash-free removal.
// ---------------------------------------------------------------------------
TEST_CASE("Scene destroy handles a parent and its child queued together")
{
    Indium::Scene scene;

    auto p = std::make_unique<Indium::Entity>(); p->id = 1;
    auto c = std::make_unique<Indium::Entity>(); c->id = 2;

    c->setParent(p.get());
    scene.entities.push_back(std::move(p));
    scene.entities.push_back(std::move(c));

    scene.DestroyEntity(1);
    scene.DestroyEntity(2); // child explicitly queued as well
    scene.Update(1.0f / 60.0f);

    CHECK(scene.FindEntity(1) == nullptr);
    CHECK(scene.FindEntity(2) == nullptr);
    CHECK(scene.entities.empty());
}

// ---------------------------------------------------------------------------
// Test 8: Destroying a child unlinks it from the surviving parent's children
//         list (no dangling pointer left behind on the parent).
// ---------------------------------------------------------------------------
TEST_CASE("Scene destroy of a child unlinks it from the surviving parent")
{
    Indium::Scene scene;

    auto p  = std::make_unique<Indium::Entity>(); p->id = 1;
    auto c1 = std::make_unique<Indium::Entity>(); c1->id = 2;
    auto c2 = std::make_unique<Indium::Entity>(); c2->id = 3;

    Indium::Entity* pPtr = p.get();
    c1->setParent(pPtr);
    c2->setParent(pPtr);

    scene.entities.push_back(std::move(p));
    scene.entities.push_back(std::move(c1));
    scene.entities.push_back(std::move(c2));

    scene.DestroyEntity(2); // destroy only the first child
    scene.Update(1.0f / 60.0f);

    REQUIRE(scene.FindEntity(1) != nullptr);
    CHECK(scene.FindEntity(2) == nullptr);
    REQUIRE(scene.FindEntity(3) != nullptr);

    Indium::Entity* survivingParent = scene.FindEntity(1);
    REQUIRE(survivingParent->children.size() == 1);
    CHECK(survivingParent->children[0]->id == 3); // c1 removed, c2 still linked
}

// ---------------------------------------------------------------------------
// Test 9: ~Entity detaches itself from its parent's children list, so freeing
//         a child never leaves a dangling pointer behind on the parent.
// ---------------------------------------------------------------------------
TEST_CASE("Entity destructor detaches from its parent")
{
    auto p = std::make_unique<Indium::Entity>(); p->id = 1;
    auto c = std::make_unique<Indium::Entity>(); c->id = 2;

    c->setParent(p.get());
    REQUIRE(p->children.size() == 1);

    c.reset(); // destroy the child

    CHECK(p->children.empty()); // ~Entity removed it from the parent's list
}

// ---------------------------------------------------------------------------
// Test 10: ~Entity orphans its children (nulls their `parent`) so teardown is
//          order-independent — a parent freed BEFORE its still-live children
//          must not leave them pointing at freed memory. This is the case a
//          parent-only unlink destructor would use-after-free on; it fails here
//          both logically (the CHECK below) and under AddressSanitizer (clear()).
// ---------------------------------------------------------------------------
TEST_CASE("Entity teardown is order-independent when a parent is freed first")
{
    Indium::Scene scene;

    auto p = std::make_unique<Indium::Entity>(); p->id = 1;
    auto c = std::make_unique<Indium::Entity>(); c->id = 2;
    auto g = std::make_unique<Indium::Entity>(); g->id = 3;

    Indium::Entity* cPtr = c.get();
    Indium::Entity* gPtr = g.get();

    c->setParent(p.get());
    g->setParent(cPtr);

    scene.entities.push_back(std::move(p));
    scene.entities.push_back(std::move(c));
    scene.entities.push_back(std::move(g));

    // Free the parent first, while its descendants are still alive.
    scene.entities.erase(scene.entities.begin()); // frees id 1 (the parent)

    CHECK(cPtr->parent == nullptr); // (b) orphaned the child instead of dangling
    CHECK(gPtr->parent == cPtr);    // grandchild's link to the still-live child intact

    scene.entities.clear();         // freeing the rest must not touch freed memory
    CHECK(scene.entities.empty());
// Test 11: A dynamic body falling onto a collision-enabled tilemap is stopped on
//         the tile surface, and the solid tiles are visible to OverlapBox (so
//         ground checks work). Exercises greedy-merge + tile-vs-body resolution.
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap collision stops a falling body and is visible to OverlapBox")
{
    Indium::Scene scene;

    // A 10x1 strip of solid 32px tiles, top-left at world (0, 200).
    auto ground = std::make_unique<Indium::Entity>();
    ground->id = 1; ground->position = {0.0f, 200.0f};
    auto* tm = ground->addComponent<Indium::TilemapComponent>();
    tm->tileW = 32; tm->tileH = 32; tm->tileScale = 1.0f;
    tm->Resize(10, 1);
    tm->Fill(0);                  // every cell holds tile 0 → solid
    tm->collisionEnabled = true;
    scene.entities.push_back(std::move(ground));

    // The strip must greedy-merge into a single rect spanning it: {0, 200, 320, 32}.
    const auto& rects = scene.entities[0]->getComponent<Indium::TilemapComponent>()->GetSolidWorldRects();
    REQUIRE(rects.size() == 1);
    CHECK(rects[0].rect.width  == doctest::Approx(320.0f));
    CHECK(rects[0].rect.height == doctest::Approx(32.0f));

    // A 16x16 dynamic body starts above the floor, moving down.
    auto body = std::make_unique<Indium::Entity>();
    body->id = 2; body->position = {64.0f, 150.0f}; body->scale = {16.0f, 16.0f};
    body->velocity = {0.0f, 400.0f};
    body->addComponent<Indium::RigidbodyComponent>();
    scene.entities.push_back(std::move(body));
    Indium::Entity* b = scene.entities[1].get();

    // Integrate + resolve ~3 s of fixed steps so it lands and settles.
    for (int i = 0; i < 180; ++i)
    {
        b->fixedUpdate(Indium::Scene::FIXED_TIMESTEP, scene.worldSize, &scene);
        Indium::RigidbodyComponent::ResolveScene(&scene, Indium::Scene::FIXED_TIMESTEP);
    }

    // It fell from y=150 and came to rest on the tile top (surface y=200, body
    // half-height 8 → centre ~192). A free fall would be far below 200 after 3 s.
    CHECK(b->position.y > 150.0f);   // it fell onto the floor
    CHECK(b->position.y < 200.0f);   // resting on top, not sunk through
    CHECK(b->velocity.y < 5.0f);     // came to rest, not still accelerating

    // The solid tiles must be visible to OverlapBox so the platformer ground check
    // works: a probe just below the surface should report the tilemap entity (id 1).
    auto hits = scene.OverlapBox({64.0f, 201.0f}, {12.0f, 6.0f});
    bool foundGround = false;
    for (Indium::Entity* h : hits) if (h->id == 1) { foundGround = true; break; }
    CHECK(foundGround);
}

// ---------------------------------------------------------------------------
// Test 12: Per-tile classification — pass-through tiles drop out of the solid set
//         (and split a merged run), while the rest stay solid.
// ---------------------------------------------------------------------------
TEST_CASE("Per-tile classification keeps solid tiles and drops pass-through ones")
{
    Indium::Entity e;
    auto* tm = e.addComponent<Indium::TilemapComponent>();
    tm->tileW = 32; tm->tileH = 32; tm->tileScale = 1.0f;
    tm->Resize(3, 1);
    tm->collisionEnabled = true;
    tm->SetTile(0, 0, 0);
    tm->SetTile(1, 0, 1);   // a different tileset index in the middle
    tm->SetTile(2, 0, 0);

    // Default: every non-empty tile is solid → one merged 3-wide rect.
    {
        const auto& r = tm->GetSolidWorldRects();
        REQUIRE(r.size() == 1);
        CHECK(r[0].rect.width == doctest::Approx(96.0f));
        CHECK(r[0].oneWay == false);
    }

    // Mark tileset index 1 pass-through → the middle cell is no longer solid,
    // leaving two separate single-tile rects.
    tm->SetIndexPassable(1, true);
    {
        const auto& r = tm->GetSolidWorldRects();
        REQUIRE(r.size() == 2);
        CHECK(r[0].rect.width == doctest::Approx(32.0f));
        CHECK(r[1].rect.width == doctest::Approx(32.0f));
    }
}

// ---------------------------------------------------------------------------
// Test 13: One-way platforms catch a body landing from above but let a body
//         moving up pass straight through.
// ---------------------------------------------------------------------------
TEST_CASE("One-way tiles catch a falling body and let a rising body through")
{
    auto buildPlatform = [](Indium::Scene& scene)
    {
        auto plat = std::make_unique<Indium::Entity>();
        plat->id = 1; plat->position = {0.0f, 200.0f};
        auto* tm = plat->addComponent<Indium::TilemapComponent>();
        tm->tileW = 32; tm->tileH = 32; tm->tileScale = 1.0f;
        tm->Resize(6, 1);
        tm->Fill(0);
        tm->collisionEnabled = true;
        tm->SetIndexOneWay(0, true);
        scene.entities.push_back(std::move(plat));
    };

    // Falling body lands on top of the one-way platform.
    {
        Indium::Scene scene;
        buildPlatform(scene);
        REQUIRE(scene.entities[0]->getComponent<Indium::TilemapComponent>()->GetSolidWorldRects()[0].oneWay);

        auto fall = std::make_unique<Indium::Entity>();
        fall->id = 2; fall->position = {64.0f, 150.0f}; fall->scale = {16.0f, 16.0f};
        fall->velocity = {0.0f, 300.0f};
        fall->addComponent<Indium::RigidbodyComponent>();
        scene.entities.push_back(std::move(fall));
        Indium::Entity* b = scene.entities[1].get();

        for (int i = 0; i < 180; ++i)
        {
            b->fixedUpdate(Indium::Scene::FIXED_TIMESTEP, scene.worldSize, &scene);
            Indium::RigidbodyComponent::ResolveScene(&scene, Indium::Scene::FIXED_TIMESTEP);
        }
        CHECK(b->position.y > 150.0f);   // it fell
        CHECK(b->position.y < 200.0f);   // and landed on top of the one-way platform
    }

    // Body below the platform moving up passes straight through it.
    {
        Indium::Scene scene;
        buildPlatform(scene);

        auto rise = std::make_unique<Indium::Entity>();
        rise->id = 2; rise->position = {120.0f, 280.0f}; rise->scale = {16.0f, 16.0f};
        rise->velocity = {0.0f, -600.0f};
        rise->addComponent<Indium::RigidbodyComponent>();
        scene.entities.push_back(std::move(rise));
        Indium::Entity* b = scene.entities[1].get();

        // Short sim so we observe it mid-rise (gravity would later pull it back down).
        for (int i = 0; i < 20; ++i)
        {
            b->fixedUpdate(Indium::Scene::FIXED_TIMESTEP, scene.worldSize, &scene);
            Indium::RigidbodyComponent::ResolveScene(&scene, Indium::Scene::FIXED_TIMESTEP);
        }
        CHECK(b->position.y < 200.0f);   // rose above the platform top (passed through)
        CHECK(b->velocity.y < 0.0f);     // still moving up — was never blocked from below
    }
}

// A NativeScript that records tile/body collision-enter events for Test 9.
struct TileTouchProbe : Indium::NativeScript
{
    int enters      = 0;
    int lastOtherId = -1;
    void OnCollisionEnter2D(Indium::Entity* other) override { ++enters; if (other) lastOtherId = other->id; }
};

// ---------------------------------------------------------------------------
// Test 14: A circle collider rests on tiles (closest-point resolution) and the
//         landing fires OnCollisionEnter2D against the tilemap entity.
// ---------------------------------------------------------------------------
TEST_CASE("Circle body rests on tiles and fires OnCollisionEnter2D with the tilemap")
{
    Indium::Scene scene;

    auto ground = std::make_unique<Indium::Entity>();
    ground->id = 1; ground->position = {0.0f, 200.0f};
    auto* tm = ground->addComponent<Indium::TilemapComponent>();
    tm->tileW = 32; tm->tileH = 32; tm->tileScale = 1.0f;
    tm->Resize(6, 1); tm->Fill(0); tm->collisionEnabled = true;
    scene.entities.push_back(std::move(ground));

    auto body = std::make_unique<Indium::Entity>();
    body->id = 2; body->position = {64.0f, 150.0f};
    auto* cc = body->addComponent<Indium::CircleCollider2D>();
    cc->radius = 12.0f;
    body->velocity = {0.0f, 300.0f};
    body->addComponent<Indium::RigidbodyComponent>();
    auto* probe = body->addComponent<TileTouchProbe>();
    scene.entities.push_back(std::move(body));

    Indium::Entity* b = scene.entities[1].get();
    for (int i = 0; i < 180; ++i)
    {
        b->fixedUpdate(Indium::Scene::FIXED_TIMESTEP, scene.worldSize, &scene);
        Indium::RigidbodyComponent::ResolveScene(&scene, Indium::Scene::FIXED_TIMESTEP);
    }

    // Circle bottom (centre.y + r) rests on the tile top (200) → centre ~188.
    CHECK(b->position.y > 150.0f);
    CHECK(b->position.y < 200.0f);
    CHECK(b->velocity.y < 5.0f);
    CHECK(probe->enters >= 1);        // landing fired a collision-enter…
    CHECK(probe->lastOtherId == 1);   // …against the tilemap entity (id 1)
}

// Count TilemapComponents on an entity — the key invariant for the create/load path.
static int countTilemapComponents(const Indium::Entity& e)
{
    int n = 0;
    for (const auto& c : e.components) if (dynamic_cast<const Indium::TilemapComponent*>(c.get())) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// Test 15: The Tilemap entity created by the factory owns exactly one
//          TilemapComponent and survives a serialize -> LoadEntity round-trip
//          WITHOUT the auto-added component being duplicated.
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap entity round-trips through the factory without duplicating its component")
{
    Indium::Scene scene;
    Indium::EntityFactory factory;

    auto tilemap = factory.CreateTilemap(scene);
    CHECK(tilemap->getType() == "Tilemap");
    REQUIRE(countTilemapComponents(*tilemap) == 1);   // ctor attached exactly one

    // Author some state so the round-trip has something to preserve.
    auto* tm = tilemap->getComponent<Indium::TilemapComponent>();
    tm->tileW = 32; tm->tileH = 32; tm->tileScale = 1.0f;
    tm->Resize(4, 3);
    tm->Fill(0);
    tm->collisionEnabled = true;
    tm->SetIndexOneWay(0, true);

    // Serialize the entity and rebuild it the way scene-load does.
    nlohmann::json j = tilemap->serialize();
    auto loaded = factory.LoadEntity(j);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->getType() == "Tilemap");

    // The auto-added component must be deserialized INTO, not appended next to.
    REQUIRE(countTilemapComponents(*loaded) == 1);
    auto* lt = loaded->getComponent<Indium::TilemapComponent>();
    REQUIRE(lt != nullptr);
    CHECK(lt->collisionEnabled);
    CHECK(lt->cols == 4);
    CHECK(lt->rows == 3);
    CHECK(lt->IsIndexOneWay(0));
}

// ---------------------------------------------------------------------------
// Test 16: The Tilemap entity's bounds AND its merged collision rects track the
//          grid extent scaled by tileScale × the entity Transform scale (top-left
//          anchored, non-uniform allowed); clone() deep-copies the component.
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap entity bounds and collision honor tileScale and entity scale")
{
    Indium::Tilemap t;
    t.position = { 100.0f, 50.0f };
    t.scale    = { 2.0f, 3.0f };       // entity Transform scale must apply (non-uniform)
    auto* tm = t.getComponent<Indium::TilemapComponent>();
    tm->tileW = 16; tm->tileH = 16; tm->tileScale = 2.0f;
    tm->Resize(5, 4);
    tm->Fill(0);                       // every cell solid → one merged rect
    tm->collisionEnabled = true;

    // Per-tile world size = tile px × tileScale × entity scale:
    //   x: 16 * 2.0 * 2.0 = 64    y: 16 * 2.0 * 3.0 = 96
    // Grid extent: 5*64 = 320 wide, 4*96 = 384 tall, anchored at the entity position.
    ::Rectangle b = t.getBounds();
    CHECK(b.x == doctest::Approx(100.0f));   // top-left anchored at the entity position
    CHECK(b.y == doctest::Approx(50.0f));
    CHECK(b.width  == doctest::Approx(320.0f));
    CHECK(b.height == doctest::Approx(384.0f));

    // The merged collision rect must scale the same way as the visual.
    const auto& rects = tm->GetSolidWorldRects();
    REQUIRE(rects.size() == 1);
    CHECK(rects[0].rect.x == doctest::Approx(100.0f));
    CHECK(rects[0].rect.y == doctest::Approx(50.0f));
    CHECK(rects[0].rect.width  == doctest::Approx(320.0f));
    CHECK(rects[0].rect.height == doctest::Approx(384.0f));

    auto copy = t.clone();
    REQUIRE(countTilemapComponents(*copy) == 1);
    auto* ct = copy->getComponent<Indium::TilemapComponent>();
    REQUIRE(ct != nullptr);
    CHECK(ct->cols == 5);
    CHECK(ct->collisionEnabled);
    CHECK(ct != tm);   // a distinct component instance, not an aliased pointer
}

// ---------------------------------------------------------------------------
// Test 17: A CircleCollider2D's effective radius (hence its bounds and collision)
//          scales with the entity Transform scale, so render and physics track the
//          visual; a non-uniform scale collapses to the average.
// ---------------------------------------------------------------------------
TEST_CASE("Circle collider radius scales with entity Transform scale")
{
    Indium::Entity e;
    e.position = { 0.0f, 0.0f };
    auto* cc = e.addComponent<Indium::CircleCollider2D>();
    cc->radius = 10.0f;

    // Default scale 1 → effective radius is the authored radius.
    CHECK(cc->getCircleRadius() == doctest::Approx(10.0f));

    // Uniform scale 2 → radius doubles; bounds is a 40x40 box centred on the entity.
    e.scale = { 2.0f, 2.0f };
    CHECK(cc->getCircleRadius() == doctest::Approx(20.0f));
    ::Rectangle b = cc->getBounds();
    CHECK(b.width  == doctest::Approx(40.0f));
    CHECK(b.height == doctest::Approx(40.0f));

    // Non-uniform scale collapses to the average (a true circle can't be an ellipse).
    e.scale = { 2.0f, 4.0f };
    CHECK(cc->getCircleRadius() == doctest::Approx(30.0f));   // 10 * (2 + 4) / 2
}
