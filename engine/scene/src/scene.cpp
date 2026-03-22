#include "nexus/scene/scene.h"
#include "nexus/scene/hierarchy.h"

namespace nexus {

Entity Scene::create_entity(const std::string& name) {
    Entity e = registry_.create();
    registry_.add_component<TagComponent>(e, TagComponent{name});
    return e;
}

Entity Scene::create_entity_2d(const std::string& name) {
    Entity e = registry_.create();
    registry_.add_component<TagComponent>(e, TagComponent{name});
    registry_.add_component<Transform2DComponent>(e, Transform2DComponent{});
    return e;
}

Entity Scene::create_entity_3d(const std::string& name) {
    Entity e = registry_.create();
    registry_.add_component<TagComponent>(e, TagComponent{name});
    registry_.add_component<Transform3DComponent>(e, Transform3DComponent{});
    return e;
}

void Scene::destroy_entity(Entity e) {
    registry_.destroy(e);
}

void Scene::update(float dt) {
    // Propagate parent-child transforms before systems run.
    Hierarchy::propagate_transforms_3d(registry_);
    Hierarchy::propagate_transforms_2d(registry_);

    // Execute all registered systems in dependency order.
    scheduler_.execute(registry_, dt);
}

void Scene::clear() {
    // Destroy all alive entities. Use registry's clear_all() which
    // safely handles iteration during destruction.
    registry_.clear_all();
}

} // namespace nexus
