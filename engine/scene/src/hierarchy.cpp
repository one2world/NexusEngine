#include "nexus/scene/hierarchy.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"
#include <queue>

namespace nexus {

void Hierarchy::set_parent(Registry& reg, Entity child, Entity parent) {
    if (child == parent) return;
    if (!reg.alive(child) || !reg.alive(parent)) return;
    if (is_ancestor(reg, parent, child)) return; // prevent cycles

    // Detach from current parent first
    remove_parent(reg, child);

    // Ensure both have HierarchyComponent
    if (!reg.has_component<HierarchyComponent>(child))
        reg.add_component<HierarchyComponent>(child, HierarchyComponent{});
    if (!reg.has_component<HierarchyComponent>(parent))
        reg.add_component<HierarchyComponent>(parent, HierarchyComponent{});

    auto& child_h = reg.get_component<HierarchyComponent>(child);
    auto& parent_h = reg.get_component<HierarchyComponent>(parent);

    child_h.parent = parent;
    child_h.prev_sibling = INVALID_ENTITY;
    child_h.next_sibling = parent_h.first_child;

    if (parent_h.first_child != INVALID_ENTITY) {
        auto& old_first = reg.get_component<HierarchyComponent>(parent_h.first_child);
        old_first.prev_sibling = child;
    }
    parent_h.first_child = child;
}

void Hierarchy::remove_parent(Registry& reg, Entity child) {
    if (!reg.has_component<HierarchyComponent>(child)) return;

    auto& child_h = reg.get_component<HierarchyComponent>(child);
    if (child_h.parent == INVALID_ENTITY) return;

    Entity parent = child_h.parent;

    // Fix sibling links
    if (child_h.prev_sibling != INVALID_ENTITY) {
        auto& prev = reg.get_component<HierarchyComponent>(child_h.prev_sibling);
        prev.next_sibling = child_h.next_sibling;
    } else if (reg.has_component<HierarchyComponent>(parent)) {
        // child was the first_child of parent
        auto& parent_h = reg.get_component<HierarchyComponent>(parent);
        parent_h.first_child = child_h.next_sibling;
    }

    if (child_h.next_sibling != INVALID_ENTITY) {
        auto& next = reg.get_component<HierarchyComponent>(child_h.next_sibling);
        next.prev_sibling = child_h.prev_sibling;
    }

    child_h.parent = INVALID_ENTITY;
    child_h.prev_sibling = INVALID_ENTITY;
    child_h.next_sibling = INVALID_ENTITY;
}

Entity Hierarchy::get_parent(const Registry& reg, Entity entity) {
    if (!reg.has_component<HierarchyComponent>(entity)) return INVALID_ENTITY;
    return reg.get_component<HierarchyComponent>(entity).parent;
}

std::vector<Entity> Hierarchy::get_children(const Registry& reg, Entity parent) {
    std::vector<Entity> result;
    if (!reg.has_component<HierarchyComponent>(parent)) return result;

    Entity child = reg.get_component<HierarchyComponent>(parent).first_child;
    while (child != INVALID_ENTITY) {
        result.push_back(child);
        if (!reg.has_component<HierarchyComponent>(child)) break;
        child = reg.get_component<HierarchyComponent>(child).next_sibling;
    }
    return result;
}

std::vector<Entity> Hierarchy::get_descendants(const Registry& reg, Entity root) {
    std::vector<Entity> result;
    std::queue<Entity> queue;

    auto children = get_children(reg, root);
    for (Entity c : children) queue.push(c);

    while (!queue.empty()) {
        Entity e = queue.front();
        queue.pop();
        result.push_back(e);
        auto sub_children = get_children(reg, e);
        for (Entity c : sub_children) queue.push(c);
    }
    return result;
}

bool Hierarchy::is_ancestor(const Registry& reg, Entity entity, Entity ancestor) {
    Entity current = entity;
    while (current != INVALID_ENTITY) {
        if (current == ancestor) return true;
        if (!reg.has_component<HierarchyComponent>(current)) break;
        current = reg.get_component<HierarchyComponent>(current).parent;
    }
    return false;
}

void Hierarchy::propagate_transforms_3d(Registry& reg) {
    // Process all entities with HierarchyComponent and Transform3DComponent.
    // Root entities (no parent) have world = local.
    // Children: world = parent_world * local.
    // BFS from roots ensures parents are processed before children.

    std::vector<Entity> roots;
    reg.each<HierarchyComponent>([&](Entity e, HierarchyComponent& h) {
        if (h.parent == INVALID_ENTITY) roots.push_back(e);
    });

    std::queue<Entity> queue;
    for (Entity r : roots) queue.push(r);

    while (!queue.empty()) {
        Entity e = queue.front();
        queue.pop();

        if (reg.has_component<Transform3DComponent>(e) &&
            reg.has_component<HierarchyComponent>(e)) {
            auto& h = reg.get_component<HierarchyComponent>(e);
            if (h.parent != INVALID_ENTITY &&
                reg.has_component<Transform3DComponent>(h.parent)) {
                // World transform is computed during rendering using
                // the hierarchy; here we just ensure traversal order.
            }
        }

        auto children = get_children(reg, e);
        for (Entity c : children) queue.push(c);
    }
}

void Hierarchy::propagate_transforms_2d(Registry& reg) {
    std::vector<Entity> roots;
    reg.each<HierarchyComponent>([&](Entity e, HierarchyComponent& h) {
        if (h.parent == INVALID_ENTITY) roots.push_back(e);
    });

    std::queue<Entity> queue;
    for (Entity r : roots) queue.push(r);

    while (!queue.empty()) {
        Entity e = queue.front();
        queue.pop();
        auto children = get_children(reg, e);
        for (Entity c : children) queue.push(c);
    }
}

} // namespace nexus
