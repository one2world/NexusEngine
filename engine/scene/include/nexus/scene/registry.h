#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/entity.h"
#include "nexus/scene/component_pool.h"

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// Registry - the central ECS database
// ---------------------------------------------------------------------------
class Registry {
public:
    Registry() = default;

    // -- Entity management ---------------------------------------------------

    /// Create a new entity (reuses IDs from the free list when available).
    Entity create() {
        Entity e;
        if (!free_list_.empty()) {
            e = free_list_.back();
            free_list_.pop_back();
        } else {
            e = next_entity_++;
        }
        alive_.insert(e);
        return e;
    }

    /// Destroy an entity, removing all of its components.
    void destroy(Entity e) {
        if (!alive(e)) return;

        // Remove from every pool
        for (auto& [type, pool] : pools_) {
            pool->remove(e);
        }

        alive_.erase(e);
        free_list_.push_back(e);
    }

    /// Check whether the entity is currently alive.
    bool alive(Entity e) const {
        return alive_.count(e) > 0;
    }

    /// Total number of alive entities.
    std::size_t size() const { return alive_.size(); }

    // -- Component access ----------------------------------------------------

    /// Add a component to an entity. Returns a reference to the stored component.
    template <typename T>
    T& add_component(Entity e, T component) {
        return get_pool<T>().add(e, std::move(component));
    }

    /// Retrieve a mutable reference to the entity's component of type T.
    template <typename T>
    T& get_component(Entity e) {
        return get_pool<T>().get(e);
    }

    /// Retrieve a const reference to the entity's component of type T.
    template <typename T>
    const T& get_component(Entity e) const {
        return const_cast<Registry*>(this)->get_pool<T>().get(e);
    }

    /// Check whether the entity has a component of type T.
    template <typename T>
    bool has_component(Entity e) const {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) return false;
        return it->second->has(e);
    }

    /// Remove the component of type T from the entity.
    template <typename T>
    void remove_component(Entity e) {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it != pools_.end()) {
            it->second->remove(e);
        }
    }

    /// Return (or create) the typed ComponentPool for T.
    template <typename T>
    ComponentPool<T>& get_pool() {
        auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            auto* raw = pool.get();
            pools_.emplace(key, std::move(pool));
            return *raw;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    // -- Views ---------------------------------------------------------------

    /// Return all entities that have component T.
    template <typename T>
    std::vector<Entity> view() {
        auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) return {};

        auto& pool = *static_cast<ComponentPool<T>*>(it->second.get());
        return {pool.entities().begin(), pool.entities().end()};
    }

    /// Return all entities that have both T1 and T2.
    template <typename T1, typename T2>
    std::vector<Entity> view() {
        auto key1 = std::type_index(typeid(T1));
        auto key2 = std::type_index(typeid(T2));
        auto it1 = pools_.find(key1);
        auto it2 = pools_.find(key2);
        if (it1 == pools_.end() || it2 == pools_.end()) return {};

        auto& pool1 = *static_cast<ComponentPool<T1>*>(it1->second.get());
        auto& pool2 = *static_cast<ComponentPool<T2>*>(it2->second.get());

        // Iterate over the smaller pool for efficiency
        const auto& src = (pool1.size() <= pool2.size())
                          ? pool1.entities() : pool2.entities();
        std::vector<Entity> result;
        result.reserve(src.size());
        for (Entity e : src) {
            if (pool1.has(e) && pool2.has(e)) {
                result.push_back(e);
            }
        }
        return result;
    }

    // -- Each ----------------------------------------------------------------

    /// Call callback(Entity, T&) for every entity that has component T.
    template <typename T, typename Func>
    void each(Func&& callback) {
        auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) return;

        auto& pool = *static_cast<ComponentPool<T>*>(it->second.get());
        // Iterate by index so the callback may safely modify the pool.
        const auto& ents = pool.entities();
        auto& comps = pool.components();
        for (std::size_t i = 0; i < ents.size(); ++i) {
            callback(ents[i], comps[i]);
        }
    }

    /// Call callback(Entity, T1&, T2&) for every entity that has both components.
    template <typename T1, typename T2, typename Func>
    void each(Func&& callback) {
        auto key1 = std::type_index(typeid(T1));
        auto key2 = std::type_index(typeid(T2));
        auto it1 = pools_.find(key1);
        auto it2 = pools_.find(key2);
        if (it1 == pools_.end() || it2 == pools_.end()) return;

        auto& pool1 = *static_cast<ComponentPool<T1>*>(it1->second.get());
        auto& pool2 = *static_cast<ComponentPool<T2>*>(it2->second.get());

        // Walk the smaller pool
        if (pool1.size() <= pool2.size()) {
            const auto& ents = pool1.entities();
            auto& comps1 = pool1.components();
            for (std::size_t i = 0; i < ents.size(); ++i) {
                Entity e = ents[i];
                if (pool2.has(e)) {
                    callback(e, comps1[i], pool2.get(e));
                }
            }
        } else {
            const auto& ents = pool2.entities();
            auto& comps2 = pool2.components();
            for (std::size_t i = 0; i < ents.size(); ++i) {
                Entity e = ents[i];
                if (pool1.has(e)) {
                    callback(e, pool1.get(e), comps2[i]);
                }
            }
        }
    }

private:
    Entity next_entity_ = 0;
    std::vector<Entity> free_list_;
    std::unordered_set<Entity> alive_;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
};

} // namespace nexus
