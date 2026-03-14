#pragma once

#include "nexus/core/types.h"
#include "nexus/core/containers.h"
#include "nexus/scene/entity.h"

#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// IComponentPool - type-erased interface for component storage
// ---------------------------------------------------------------------------
class IComponentPool {
public:
    virtual ~IComponentPool() = default;

    /// Remove the component associated with the given entity (if any).
    virtual void remove(Entity e) = 0;

    /// Return true if the pool contains a component for the given entity.
    virtual bool has(Entity e) const = 0;
};

// ---------------------------------------------------------------------------
// ComponentPool<T> - concrete pool backed by SparseSet<T>
// ---------------------------------------------------------------------------
template <typename T>
class ComponentPool : public IComponentPool {
public:
    ComponentPool() = default;

    /// Add (or replace) a component for the given entity.
    T& add(Entity e, T component) {
        set_.add(e, std::move(component));
        return set_.get(e);
    }

    /// Retrieve a mutable reference to the component.
    T& get(Entity e) {
        return set_.get(e);
    }

    /// Retrieve a const reference to the component.
    const T& get(Entity e) const {
        return set_.get(e);
    }

    /// Check whether the entity has a component in this pool.
    bool has(Entity e) const override {
        return set_.has(e);
    }

    /// Remove the component for the given entity.
    void remove(Entity e) override {
        set_.remove(e);
    }

    /// Number of components stored.
    std::size_t size() const { return set_.size(); }

    /// Direct access to the packed entity array.
    const std::vector<Entity>& entities() const { return set_.entities(); }

    /// Direct access to the packed component array.
    std::vector<T>& components() { return set_.components(); }
    const std::vector<T>& components() const { return set_.components(); }

private:
    SparseSet<T> set_;
};

} // namespace nexus
