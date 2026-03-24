#include "nexus/scene/scene.h"
#include "nexus/scene/scene_serializer.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/core/log.h"

namespace nexus {

Entity Scene::create_entity(const std::string& name) {
    Entity e = registry_.create();
    registry_.add_component<ActiveComponent>(e, ActiveComponent{true});
    registry_.add_component<TagComponent>(e, TagComponent{name});
    return e;
}

Entity Scene::create_entity_2d(const std::string& name) {
    Entity e = registry_.create();
    registry_.add_component<ActiveComponent>(e, ActiveComponent{true});
    registry_.add_component<TagComponent>(e, TagComponent{name});
    registry_.add_component<Transform2DComponent>(e, Transform2DComponent{});
    return e;
}

Entity Scene::create_entity_3d(const std::string& name) {
    Entity e = registry_.create();
    registry_.add_component<ActiveComponent>(e, ActiveComponent{true});
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

// ── Play mode snapshot/restore ─────────────────────────────────────────────

bool Scene::take_snapshot() {
    SceneSerializer serializer(*this);
    snapshot_json_ = serializer.to_json();
    if (snapshot_json_.empty()) {
        NX_ERROR("Scene::take_snapshot: failed to serialize scene");
        return false;
    }
    NX_INFO("Scene snapshot captured ({} bytes)", snapshot_json_.size());
    return true;
}

bool Scene::restore_snapshot() {
    if (snapshot_json_.empty()) {
        NX_WARN("Scene::restore_snapshot: no snapshot to restore");
        return false;
    }
    SceneSerializer serializer(*this);
    if (!serializer.from_json(snapshot_json_)) {
        NX_ERROR("Scene::restore_snapshot: failed to deserialize snapshot");
        return false;
    }
    NX_INFO("Scene snapshot restored");
    return true;
}

} // namespace nexus
