#pragma once
#include <coroutine>
#include <functional>
#include <vector>
#include <algorithm>

namespace Indium
{
    // ----------------------------------------------------------------
    // Awaitables — use with co_await inside a CoroutineTask function
    // ----------------------------------------------------------------

    /** @brief Pauses the coroutine for `duration` seconds. */
    struct WaitForSeconds { float duration; };

    /** @brief Pauses the coroutine for `frames` update frames. */
    struct WaitForFrames  { int frames; };

    /** @brief Pauses the coroutine until `condition()` returns true. */
    struct WaitUntil      { std::function<bool()> condition; };

    // ----------------------------------------------------------------
    // CoroutineTask — return type for coroutine functions
    // ----------------------------------------------------------------

    struct CoroutineTask
    {
        struct promise_type
        {
            float                 waitSeconds   = 0.0f;
            int                   waitFrames    = 0;
            std::function<bool()> waitCondition;

            CoroutineTask get_return_object() noexcept
            {
                return CoroutineTask{ std::coroutine_handle<promise_type>::from_promise(*this) };
            }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend()   noexcept { return {}; }
            void return_void()        noexcept {}
            void unhandled_exception() noexcept {}

            struct Yielder
            {
                bool await_ready()  const noexcept { return false; }
                void await_suspend(std::coroutine_handle<>) noexcept {}
                void await_resume() noexcept {}
            };

            Yielder await_transform(WaitForSeconds w) noexcept
            {
                waitSeconds   = w.duration;
                waitFrames    = 0;
                waitCondition = nullptr;
                return {};
            }
            Yielder await_transform(WaitForFrames w) noexcept
            {
                waitFrames    = w.frames;
                waitSeconds   = 0.0f;
                waitCondition = nullptr;
                return {};
            }
            Yielder await_transform(WaitUntil w) noexcept
            {
                waitCondition = std::move(w.condition);
                waitSeconds   = 0.0f;
                waitFrames    = 0;
                return {};
            }
        };

        std::coroutine_handle<promise_type> handle;

        CoroutineTask() = default;
        explicit CoroutineTask(std::coroutine_handle<promise_type> h) : handle(h) {}

        CoroutineTask(CoroutineTask&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
        CoroutineTask& operator=(CoroutineTask&& o) noexcept
        {
            if (handle) handle.destroy();
            handle   = o.handle;
            o.handle = nullptr;
            return *this;
        }

        CoroutineTask(const CoroutineTask&)            = delete;
        CoroutineTask& operator=(const CoroutineTask&) = delete;

        ~CoroutineTask() { if (handle) handle.destroy(); }
    };

    // ----------------------------------------------------------------
    // CoroutineManager — owns and ticks active coroutines
    // ----------------------------------------------------------------

    class CoroutineManager
    {
        struct Entry
        {
            std::coroutine_handle<CoroutineTask::promise_type> handle;
            float timer      = 0.0f;
            int   frameTimer = 0;
        };
        std::vector<Entry> entries_;

    public:
        /** @brief Start a coroutine. Takes ownership of the task handle. */
        void Start(CoroutineTask&& task)
        {
            if (!task.handle) return;
            auto h    = task.handle;
            task.handle = nullptr;
            entries_.push_back({h, 0.0f, 0});
            if (!h.done()) h.resume();
        }

        /** @brief Advance all active coroutines by one frame. Call from update(). */
        void Update(float dt)
        {
            for (auto& e : entries_)
            {
                if (e.handle.done()) continue;
                auto& p = e.handle.promise();

                if (p.waitCondition)
                {
                    if (p.waitCondition()) { p.waitCondition = nullptr; if (!e.handle.done()) e.handle.resume(); }
                }
                else if (p.waitFrames > 0)
                {
                    if (++e.frameTimer >= p.waitFrames) { e.frameTimer = 0; p.waitFrames = 0; if (!e.handle.done()) e.handle.resume(); }
                }
                else
                {
                    e.timer += dt;
                    if (e.timer >= p.waitSeconds) { e.timer = 0.0f; p.waitSeconds = 0.0f; if (!e.handle.done()) e.handle.resume(); }
                }
            }

            for (auto it = entries_.begin(); it != entries_.end(); )
            {
                if (it->handle.done()) { it->handle.destroy(); it = entries_.erase(it); }
                else ++it;
            }
        }

        /** @brief Stop and destroy all running coroutines. */
        void StopAll()
        {
            for (auto& e : entries_) if (e.handle) e.handle.destroy();
            entries_.clear();
        }

        int Count() const { return (int)entries_.size(); }

        ~CoroutineManager() { StopAll(); }
    };

} // namespace Indium
