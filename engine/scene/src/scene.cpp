#include "nexus/scene/scene.h"

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

void Scene::update(float /*dt*/) {
    // Placeholder: systems will be executed here in future iterations.
}

void Scene::clear() {
    // Collect all alive entities and destroy them.
    // We cannot iterate alive_ directly while modifying it,
    // so gather IDs first via a view over TagComponent (every entity has one).
    auto entities = registry_.view<TagComponent>();
    for (Entity e : entities) {
        registry_.destroy(e);
    }
}

} // namespace nexus
