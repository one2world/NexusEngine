#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/entity.h"
#include <vector>
#include <unordered_map>

namespace nexus {

// ---------------------------------------------------------------------------
// HierarchyComponent - parent-child relationship data stored per entity
// ---------------------------------------------------------------------------
struct HierarchyComponent {
    Entity parent{INVALID_ENTITY};
    Entity first_child{INVALID_ENTITY};
    Entity next_sibling{INVALID_ENTITY};
    Entity prev_sibling{INVALID_ENTITY};
};

class Registry; // forward

// ---------------------------------------------------------------------------
// Hierarchy - manages parent-child relationships and transform propagation
// ---------------------------------------------------------------------------
class Hierarchy {
public:
    /// Attach child to parent. Both entities must exist in the registry.
    static void set_parent(Registry& reg, Entity child, Entity parent);

    /// Detach entity from its parent (becomes root-level).
    static void remove_parent(Registry& reg, Entity child);

    /// Get the parent of an entity (INVALID_ENTITY if root).
    static Entity get_parent(const Registry& reg, Entity entity);

    /// Collect all direct children of an entity.
    static std::vector<Entity> get_children(const Registry& reg, Entity parent);

    /// Collect all descendants (recursive children).
    static std::vector<Entity> get_descendants(const Registry& reg, Entity root);

    /// Check if ancestor is an ancestor of entity (prevents cycles).
    static bool is_ancestor(const Registry& reg, Entity entity, Entity ancestor);

    /// Propagate Transform3D from parent to children (world = parent * local).
    static void propagate_transforms_3d(Registry& reg);

    /// Propagate Transform2D from parent to children.
    static void propagate_transforms_2d(Registry& reg);
};

} // namespace nexus
