#include "doctest.h"
#include "QuestManager.hpp"
#include "StoryState.hpp"

using namespace Indium;

// Each case shares the process-wide singletons, so reset StoryState and (re)arm the
// QuestManager subscription before loading the case's definition. StoryState's own
// NarrativeEvent subscription is created at construction and never torn down here
// (no EventBus::Clear in the test process), so flag-setting keeps working.
static void freshStart(const char* defJson)
{
    StoryState::Get().Clear();
    QuestManager::Get().SubscribeToEvents();
    QuestManager::Get().LoadFromJson(nlohmann::json::parse(defJson));
}

static const char* kSeq = R"({
  "id": "seq", "title": "Seq", "mode": "sequential", "completeFlag": "seq_done",
  "objectives": [
    { "id": "a", "desc": "A", "completeFlag": "flag_a" },
    { "id": "b", "desc": "B", "completeFlag": "flag_b" }
  ]
})";

static const char* kPar = R"({
  "id": "par", "title": "Par", "mode": "parallel", "completeFlag": "par_done",
  "objectives": [
    { "id": "a", "desc": "A", "completeFlag": "p_a" },
    { "id": "b", "desc": "B", "completeFlag": "p_b" }
  ]
})";

TEST_CASE("sequential quest advances in order and completes")
{
    freshStart(kSeq);
    auto& q = QuestManager::Get();

    q.Start("seq");
    CHECK(q.IsActive("seq"));
    CHECK(q.CurrentStep("seq") == 0);

    StoryState::Get().SetFlag("flag_a");
    CHECK(q.IsActive("seq"));
    CHECK(q.CurrentStep("seq") == 1);

    StoryState::Get().SetFlag("flag_b");
    CHECK(q.IsComplete("seq"));
    CHECK(StoryState::Get().HasFlag("seq_done"));
}

TEST_CASE("sequential quest ignores out-of-order flags until the current objective")
{
    freshStart(kSeq);
    auto& q = QuestManager::Get();
    q.Start("seq");

    StoryState::Get().SetFlag("flag_b"); // a later objective satisfied first
    CHECK(q.IsActive("seq"));
    CHECK(q.CurrentStep("seq") == 0);    // must not skip ahead

    StoryState::Get().SetFlag("flag_a"); // current objective done; b already done -> full complete
    CHECK(q.IsComplete("seq"));
}

TEST_CASE("parallel quest completes only when every objective is done")
{
    freshStart(kPar);
    auto& q = QuestManager::Get();
    q.Start("par");

    StoryState::Get().SetFlag("p_b");
    CHECK(q.IsActive("par"));

    StoryState::Get().SetFlag("p_a");
    CHECK(q.IsComplete("par"));
    CHECK(StoryState::Get().HasFlag("par_done"));
}

TEST_CASE("script can drive a quest directly")
{
    freshStart(kSeq);
    auto& q = QuestManager::Get();
    q.Start("seq");

    q.CompleteObjective("seq", "a"); // sets flag_a -> advances to step 1
    CHECK(q.CurrentStep("seq") == 1);

    q.Complete("seq");               // force-complete regardless of flag_b
    CHECK(q.IsComplete("seq"));
}

TEST_CASE("quest progress round-trips through StoryState serialization (save/load)")
{
    freshStart(kSeq);
    auto& q = QuestManager::Get();
    q.Start("seq");
    StoryState::Get().SetFlag("flag_a");
    CHECK(q.CurrentStep("seq") == 1);

    const nlohmann::json snapshot = StoryState::Get().serialize();
    StoryState::Get().Clear();
    CHECK(q.StateOf("seq") == QuestState::NotStarted);

    StoryState::Get().deserialize(snapshot);
    CHECK(q.IsActive("seq"));
    CHECK(q.CurrentStep("seq") == 1);
}

TEST_CASE("completing one quest can satisfy another's objective without looping")
{
    StoryState::Get().Clear();
    QuestManager::Get().SubscribeToEvents();
    // A completes -> sets "a_done"; B's only objective is gated on "a_done".
    QuestManager::Get().LoadFromJson(nlohmann::json::parse(R"({
      "id": "A", "mode": "sequential", "completeFlag": "a_done",
      "objectives": [ { "id": "x", "completeFlag": "x_flag" } ]
    })"));
    QuestManager::Get().LoadFromJson(nlohmann::json::parse(R"({
      "id": "B", "mode": "sequential",
      "objectives": [ { "id": "y", "completeFlag": "a_done" } ]
    })"));

    auto& q = QuestManager::Get();
    q.Start("A");
    q.Start("B");

    StoryState::Get().SetFlag("x_flag"); // completes A, which chains into B
    CHECK(q.IsComplete("A"));
    CHECK(q.IsComplete("B"));
}
