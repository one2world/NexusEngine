#pragma once

#include <functional>
#include <vector>
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <algorithm>

namespace nexus {

// Type-safe event bus
class EventBus {
public:
    using HandlerId = uint64_t;

    template <typename EventType>
    HandlerId subscribe(std::function<void(const EventType&)> handler) {
        auto id = next_id_++;
        auto& handlers = get_handlers<EventType>();
        handlers.push_back({id, std::move(handler)});
        return id;
    }

    template <typename EventType>
    void unsubscribe(HandlerId id) {
        auto& handlers = get_handlers<EventType>();
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [id](const auto& entry) { return entry.id == id; }),
            handlers.end()
        );
    }

    template <typename EventType>
    void publish(const EventType& event) {
        auto& handlers = get_handlers<EventType>();
        for (auto& entry : handlers) {
            entry.handler(event);
        }
    }

    void clear() { handlers_.clear(); }

private:
    template <typename EventType>
    struct HandlerEntry {
        HandlerId id;
        std::function<void(const EventType&)> handler;
    };

    template <typename EventType>
    struct HandlerList {
        std::vector<HandlerEntry<EventType>> entries;
    };

    struct IHandlerList {
        virtual ~IHandlerList() = default;
    };

    template <typename EventType>
    struct TypedHandlerList : IHandlerList {
        std::vector<HandlerEntry<EventType>> entries;
    };

    template <typename EventType>
    std::vector<HandlerEntry<EventType>>& get_handlers() {
        auto key = std::type_index(typeid(EventType));
        auto it = handlers_.find(key);
        if (it == handlers_.end()) {
            auto list = std::make_unique<TypedHandlerList<EventType>>();
            auto* ptr = list.get();
            handlers_[key] = std::move(list);
            return ptr->entries;
        }
        return static_cast<TypedHandlerList<EventType>*>(it->second.get())->entries;
    }

    std::unordered_map<std::type_index, std::unique_ptr<IHandlerList>> handlers_;
    HandlerId next_id_ = 1;
};

// Common engine events
struct WindowResizeEvent {
    int width;
    int height;
};

struct WindowCloseEvent {};

struct KeyEvent {
    int key;
    int scancode;
    int action;
    int mods;
};

struct MouseMoveEvent {
    double x;
    double y;
};

struct MouseButtonEvent {
    int button;
    int action;
    int mods;
};

struct MouseScrollEvent {
    double x_offset;
    double y_offset;
};

} // namespace nexus
