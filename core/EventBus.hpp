#pragma once
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <utility>

namespace Indium
{
    // RAII handle returned by Subscribe. Unsubscribes automatically when destroyed.
    struct SubscriptionHandle
    {
        SubscriptionHandle() = default;
        explicit SubscriptionHandle(std::shared_ptr<bool> alive) : alive_(std::move(alive)) {}

        ~SubscriptionHandle() { Unsubscribe(); }

        SubscriptionHandle(const SubscriptionHandle&)            = delete;
        SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;
        SubscriptionHandle(SubscriptionHandle&&) = default;
        SubscriptionHandle& operator=(SubscriptionHandle&& o) noexcept
        {
            if (this != &o) { Unsubscribe(); alive_ = std::move(o.alive_); }
            return *this;
        }

        void Unsubscribe() { if (alive_) { *alive_ = false; } }
        [[nodiscard]] bool IsAlive() const { return alive_ && *alive_; }

    private:
        std::shared_ptr<bool> alive_;
    };

    class EventBus
    {
    public:
        EventBus(const EventBus&)            = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&)                 = delete;
        EventBus& operator=(EventBus&&)      = delete;

        static EventBus& Get()
        {
            static EventBus instance;
            return instance;
        }

        // Subscribe to event type T. Returns a handle — store it; destruction = unsubscribe.
        template<typename T>
        SubscriptionHandle Subscribe(std::function<void(const T&)> handler)
        {
            auto alive = std::make_shared<bool>(true);
            HandlerEntry entry{
                alive,
                [callback = std::move(handler)](const void* data) {
                    callback(*static_cast<const T*>(data));
                }
            };

            const auto type = std::type_index(typeid(T));
            // Pushing into a channel mid-dispatch can reallocate the very vector an
            // outer Publish is iterating, freeing the handler currently running.
            // Defer the add until the outermost Publish unwinds.
            if (dispatchDepth_ > 0) { pendingAdds_.push_back({ type, std::move(entry) }); }
            else                    { channels_[type].push_back(std::move(entry)); }

            return SubscriptionHandle(alive);
        }

        // Dispatch event to all live subscribers of type T synchronously.
        template<typename T>
        void Publish(const T& event)
        {
            auto channel = channels_.find(std::type_index(typeid(T)));
            if (channel == channels_.end()) { return; }

            auto& handlers = channel->second;
            auto count = handlers.size(); // freeze size — new subs are deferred and fire next dispatch

            ++dispatchDepth_;
            // A throwing handler must not leave dispatchDepth_ stuck above zero — that
            // would defer every future add/purge/Clear forever. Unwind on exception too.
            try
            {
                for (decltype(count) idx = 0; idx < count; ++idx)
                {
                    if (*handlers[idx].alive)
                    {
                        handlers[idx].fn(&event);
                    }
                }
            }
            catch (...)
            {
                if (--dispatchDepth_ == 0) { FlushDeferred(); }
                throw;
            }

            // Adds, purges and Clear are deferred while any dispatch is on the stack
            // so a re-entrant handler can't shift or free what an outer Publish is
            // still iterating. Apply them once the stack has fully unwound.
            if (--dispatchDepth_ == 0) { FlushDeferred(); }
        }

        // Remove all handlers for all event types (useful on scene reset).
        void Clear()
        {
            // Wiping channels mid-dispatch would destroy the vector being iterated.
            if (dispatchDepth_ > 0) { clearRequested_ = true; return; }
            channels_.clear();
        }

    private:
        struct HandlerEntry
        {
            std::shared_ptr<bool>            alive;
            std::function<void(const void*)> fn;
        };

        // Applied when the outermost Publish finishes (dispatchDepth_ hits 0).
        void FlushDeferred()
        {
            if (clearRequested_)
            {
                channels_.clear();
                pendingAdds_.clear();
                clearRequested_ = false;
                return;
            }

            for (auto& [type, entry] : pendingAdds_) { channels_[type].push_back(std::move(entry)); }
            pendingAdds_.clear();

            for (auto& [type, handlers] : channels_)
            {
                handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                    [](const HandlerEntry& entry) { return !*entry.alive; }), handlers.end());
            }
        }

        std::unordered_map<std::type_index, std::vector<HandlerEntry>> channels_;
        std::vector<std::pair<std::type_index, HandlerEntry>>          pendingAdds_;
        int  dispatchDepth_  = 0;
        bool clearRequested_ = false;

        EventBus()  = default;
        ~EventBus() = default;
    };

    // Convenience free functions — prefer these over calling EventBus::Get() directly.
    namespace Events
    {
        template<typename T>
        SubscriptionHandle Subscribe(std::function<void(const T&)> handler){ return EventBus::Get().Subscribe<T>(std::move(handler)); }

        template<typename T>
        void Publish(const T& event) {EventBus::Get().Publish(event);}

    }
}
