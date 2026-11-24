/**
 * @file event_router.cpp
 * @brief Event router implementation
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include <atomic>

namespace phone_projection {
namespace core {

using EventHandler = IEventRouter::EventHandler;
using SubscriptionToken = IEventRouter::SubscriptionToken;

struct SubscriptionEntry {
    EventType type;
    EventHandler handler;
};

class EventRouterImpl : public IEventRouter {
public:
    EventRouterImpl() = default;
    ~EventRouterImpl() override = default;

    Result<SubscriptionToken> subscribe(EventType type, EventHandler handler) override {
        std::lock_guard<std::mutex> lock(mutex_);

        SubscriptionToken token = next_token_.fetch_add(1, std::memory_order_relaxed);
        if (token == 0) {
            // Skip 0 as invalid token
            token = next_token_.fetch_add(1, std::memory_order_relaxed);
        }

        subscriptions_[token] = SubscriptionEntry{type, std::move(handler)};
        subscribers_[type].push_back(token);

        PP_LOG_DEBUG("Subscribed to event type {} with token {}", static_cast<int>(type), token);
        return Result<SubscriptionToken>(token);
    }

    Result<void> unsubscribe(SubscriptionToken token) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = subscriptions_.find(token);
        if (it == subscriptions_.end()) {
            return Result<void>(make_error_code(ErrorCode::InvalidArgument));
        }

        EventType type = it->second.type;
        subscriptions_.erase(it);

        // Remove token from type subscribers list
        auto& tokens = subscribers_[type];
        tokens.erase(std::remove(tokens.begin(), tokens.end(), token), tokens.end());

        PP_LOG_DEBUG("Unsubscribed token {}", token);
        return Result<void>();
    }

    Result<void> publish(const Event& event) override {
        std::vector<EventHandler> handlers;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(event.type);
            if (it != subscribers_.end()) {
                for (auto token : it->second) {
                    auto sub_it = subscriptions_.find(token);
                    if (sub_it != subscriptions_.end()) {
                        handlers.push_back(sub_it->second.handler);
                    }
                }
            }
        }

        for (auto& handler : handlers) {
            try {
                handler(event);
            } catch (const std::exception& e) {
                PP_LOG_ERROR("Event handler exception: {}", e.what());
            } catch (...) {
                PP_LOG_ERROR("Event handler unknown exception");
            }
        }

        return Result<void>();
    }

    Result<void> publish_async(std::unique_ptr<Event> event) override {
        // In a full implementation, this would use a thread pool
        // For now, just call publish directly
        return publish(*event);
    }

private:
    mutable std::mutex mutex_;
    std::atomic<SubscriptionToken> next_token_{1};
    std::unordered_map<SubscriptionToken, SubscriptionEntry> subscriptions_;
    std::unordered_map<EventType, std::vector<SubscriptionToken>> subscribers_;
};

std::unique_ptr<IEventRouter> create_event_router() {
    return std::make_unique<EventRouterImpl>();
}

} // namespace core
} // namespace phone_projection