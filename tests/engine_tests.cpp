#include "doctest.h"
#include "../core/Entity.hpp"
#include "scene/Scene.hpp"
#include "2D/component/RigidbodyComponent.hpp"
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
}
