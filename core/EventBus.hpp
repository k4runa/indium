#pragma once
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <algorithm>

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
            channels_[std::type_index(typeid(T))].push_back({
                alive,
                [callback = std::move(handler)](const void* data) {
                    callback(*static_cast<const T*>(data));
                }
            });
            return SubscriptionHandle(alive);
        }

        // Dispatch event to all live subscribers of type T synchronously.
        template<typename T>
        void Publish(const T& event)
        {
            auto channel = channels_.find(std::type_index(typeid(T)));
            if (channel == channels_.end()) { return; }

            auto& handlers = channel->second;
            auto count = handlers.size(); // freeze size — new subs during dispatch fire next call

            for (decltype(count) idx = 0; idx < count; ++idx)
            {
                if (*handlers[idx].alive)
                {
                    handlers[idx].fn(&event);
                }
            }

            // Lazy purge dead entries after full dispatch
            handlers.erase(std::remove_if(handlers.begin(), handlers.end(),[](const HandlerEntry& entry) { return !*entry.alive; }), handlers.end());
        }

        // Remove all handlers for all event types (useful on scene reset).
        void Clear() { channels_.clear(); }

    private:
        struct HandlerEntry
        {
            std::shared_ptr<bool>            alive;
            std::function<void(const void*)> fn;
        };

        std::unordered_map<std::type_index, std::vector<HandlerEntry>> channels_;

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
