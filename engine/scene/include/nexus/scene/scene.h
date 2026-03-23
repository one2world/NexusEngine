#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/entity.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"
#include "nexus/scene/system_scheduler.h"

#include <string>

namespace nexus {

// ---------------------------------------------------------------------------
// Scene - high-level wrapper around the ECS Registry
// ---------------------------------------------------------------------------
class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    /// Access the underlying ECS registry.
    Registry& registry() { return registry_; }
    const Registry& registry() const { return registry_; }

    /// Access the system scheduler.
    SystemScheduler& scheduler() { return scheduler_; }
    const SystemScheduler& scheduler() const { return scheduler_; }

    /// Create a bare entity with a TagComponent.
    Entity create_entity(const std::string& name = "Entity");

    /// Create a 2-D entity (Tag + Transform2D).
    Entity create_entity_2d(const std::string& name);

    /// Create a 3-D entity (Tag + Transform3D).
    Entity create_entity_3d(const std::string& name);

    /// Destroy an entity and all its components.
    void destroy_entity(Entity e);

    /// Tick the scene: propagate transforms and execute all registered systems.
    void update(float dt);

    /// Remove all entities from the scene.
    void clear();

    // -- Play mode snapshot/restore ------------------------------------------

    /// Take a serialized snapshot of the current scene state (for editor play mode).
    /// Returns true if snapshot was successfully captured.
    bool take_snapshot();

    /// Restore the scene to the last snapshot state.
    /// Returns true if restore was successful.
    bool restore_snapshot();

    /// Check if a snapshot exists.
    bool has_snapshot() const { return !snapshot_json_.empty(); }

    /// Clear the stored snapshot.
    void clear_snapshot() { snapshot_json_.clear(); }

private:
    Registry registry_;
    SystemScheduler scheduler_;
    std::string snapshot_json_;  // serialized scene state for play mode restore
};

} // namespace nexus
