#include "doctest.h"
#include "ItemManager.hpp"
#include "StoryState.hpp"

using namespace Indium;

// The ItemManager + StoryState singletons are process-wide and shared across cases, so
// clear StoryState (the inventory store) and (re)load this suite's definitions at the
// start of every case. Definitions are keyed by id, so reloading just overwrites.
static void freshItems()
{
    StoryState::Get().Clear();
    auto& im = ItemManager::Get();
    im.LoadFromJson(nlohmann::json::parse(R"({ "id": "gold",   "name": "Gold",   "stackable": true })"));
    im.LoadFromJson(nlohmann::json::parse(R"({ "id": "potion", "name": "Potion", "stackable": true, "maxStack": 3 })"));
    im.LoadFromJson(nlohmann::json::parse(R"({ "id": "sword",  "name": "Sword",  "stackable": false })"));
}

TEST_CASE("give and take adjust the StoryState-backed count")
{
    freshItems();
    auto& im = ItemManager::Get();
    CHECK(im.Count("gold") == 0);
    CHECK_FALSE(im.Has("gold"));

    im.Give("gold", 5);
    CHECK(im.Count("gold") == 5);
    CHECK(im.Has("gold"));
    CHECK(im.Has("gold", 5));
    CHECK_FALSE(im.Has("gold", 6));

    CHECK(im.Take("gold", 2));
    CHECK(im.Count("gold") == 3);
}

TEST_CASE("take fails (no change) when the player has too few")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("gold", 2);
    CHECK_FALSE(im.Take("gold", 5));
    CHECK(im.Count("gold") == 2);
}

TEST_CASE("taking the last of an item clears the key")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("gold", 3);
    CHECK(im.Take("gold", 3));
    CHECK(im.Count("gold") == 0);
    CHECK_FALSE(StoryState::Get().Has(ItemManager::CountKey("gold")));
}

TEST_CASE("stackable maxStack clamps the count")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("potion", 10);          // maxStack 3
    CHECK(im.Count("potion") == 3);
    im.Give("potion", 1);           // already at cap -> no change
    CHECK(im.Count("potion") == 3);
}

TEST_CASE("non-stackable items cap at one")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("sword", 5);
    CHECK(im.Count("sword") == 1);
}

TEST_CASE("an unauthored item behaves as an unlimited counter")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("score", 1000000);      // no items/score.json authored
    CHECK(im.Count("score") == 1000000);
    CHECK(im.Definition("score") == nullptr);
}

TEST_CASE("contents lists owned items paired with their definitions")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("gold", 5);
    im.Give("sword", 1);

    bool foundGold = false, foundSword = false;
    for (const auto& e : im.Contents())
    {
        if (e.id == "gold")  { foundGold  = true; CHECK(e.count == 5); CHECK(e.def != nullptr); }
        if (e.id == "sword") { foundSword = true; CHECK(e.count == 1); CHECK(e.def != nullptr); }
    }
    CHECK(foundGold);
    CHECK(foundSword);
}

TEST_CASE("inventory rides StoryState serialization (save/load)")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("gold", 7);

    const nlohmann::json snap = StoryState::Get().serialize();
    StoryState::Get().Clear();
    CHECK(im.Count("gold") == 0);

    StoryState::Get().deserialize(snap);
    CHECK(im.Count("gold") == 7);
}

TEST_CASE("counts plug into dialogue/quest story expressions for free")
{
    freshItems();
    auto& im = ItemManager::Get();
    im.Give("gold", 12);

    // The "has item" gate used by dialogue requireFlag and (via completeWhen) quest objectives:
    CHECK(StoryEval("item.gold >= 10"));
    CHECK_FALSE(StoryEval("item.gold >= 20"));
    // Count interpolation used by dialogue text:
    CHECK(StoryInterpolate("You have {item.gold} gold.") == "You have 12 gold.");
}

TEST_CASE("give and take publish ItemEvents")
{
    freshItems();
    auto& im = ItemManager::Get();
    int added = 0, removed = 0, lastCount = -1;
    SubscriptionHandle sub = Events::Subscribe<GameEvents::ItemEvent>(
        [&](const GameEvents::ItemEvent& e)
        {
            if (e.type == GameEvents::ItemEvent::Type::Added)   added   += e.delta;
            if (e.type == GameEvents::ItemEvent::Type::Removed) removed += -e.delta;
            lastCount = e.newCount;
        });

    im.Give("gold", 4);
    CHECK(added == 4);
    CHECK(lastCount == 4);

    im.Take("gold", 1);
    CHECK(removed == 1);
    CHECK(lastCount == 3);
}
