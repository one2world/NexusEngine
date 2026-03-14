#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/entity.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"

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

    /// Create a bare entity with a TagComponent.
    Entity create_entity(const std::string& name = "Entity");

    /// Create a 2-D entity (Tag + Transform2D).
    Entity create_entity_2d(const std::string& name);

    /// Create a 3-D entity (Tag + Transform3D).
    Entity create_entity_3d(const std::string& name);

    /// Destroy an entity and all its components.
    void destroy_entity(Entity e);

    /// Tick the scene (placeholder for system execution).
    void update(float dt);

    /// Remove all entities from the scene.
    void clear();

private:
    Registry registry_;
};

} // namespace nexus
