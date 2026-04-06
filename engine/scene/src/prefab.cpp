#include "nexus/scene/prefab.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/core/log.h"
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace nexus {

// Helper: copy all known components from src entity to dst entity
static void copy_entity_components(const Registry& src_reg, Entity src,
                                    Registry& dst_reg, Entity dst) {
    if (src_reg.has_component<Transform2DComponent>(src))
        dst_reg.add_component<Transform2DComponent>(dst, src_reg.get_component<Transform2DComponent>(src));
    if (src_reg.has_component<Transform3DComponent>(src))
        dst_reg.add_component<Transform3DComponent>(dst, src_reg.get_component<Transform3DComponent>(src));
    if (src_reg.has_component<SpriteRendererComponent>(src))
        dst_reg.add_component<SpriteRendererComponent>(dst, src_reg.get_component<SpriteRendererComponent>(src));
    if (src_reg.has_component<MeshRendererComponent>(src))
        dst_reg.add_component<MeshRendererComponent>(dst, src_reg.get_component<MeshRendererComponent>(src));
    if (src_reg.has_component<CameraComponent>(src))
        dst_reg.add_component<CameraComponent>(dst, src_reg.get_component<CameraComponent>(src));
    if (src_reg.has_component<RigidBody2DComponent>(src))
        dst_reg.add_component<RigidBody2DComponent>(dst, src_reg.get_component<RigidBody2DComponent>(src));
    if (src_reg.has_component<Collider2DComponent>(src))
        dst_reg.add_component<Collider2DComponent>(dst, src_reg.get_component<Collider2DComponent>(src));
    if (src_reg.has_component<RigidBody3DComponent>(src))
        dst_reg.add_component<RigidBody3DComponent>(dst, src_reg.get_component<RigidBody3DComponent>(src));
    if (src_reg.has_component<Collider3DComponent>(src))
        dst_reg.add_component<Collider3DComponent>(dst, src_reg.get_component<Collider3DComponent>(src));
    if (src_reg.has_component<DirectionalLightComponent>(src))
        dst_reg.add_component<DirectionalLightComponent>(dst, src_reg.get_component<DirectionalLightComponent>(src));
    if (src_reg.has_component<PointLightComponent>(src))
        dst_reg.add_component<PointLightComponent>(dst, src_reg.get_component<PointLightComponent>(src));
    if (src_reg.has_component<SpotLightComponent>(src))
        dst_reg.add_component<SpotLightComponent>(dst, src_reg.get_component<SpotLightComponent>(src));
}

// Recursive helper: copy entity and all descendants into temp scene, preserving hierarchy
static void copy_entity_recursive(const Scene& scene, Entity entity,
                                   Scene& temp, Entity parent_in_temp,
                                   bool is_root) {
    auto& src_reg = scene.registry();
    auto& dst_reg = temp.registry();

    std::string tag_name = "Entity";
    if (src_reg.has_component<TagComponent>(entity)) {
        tag_name = src_reg.get_component<TagComponent>(entity).name;
    }

    Entity new_e = temp.create_entity(tag_name);
    copy_entity_components(src_reg, entity, dst_reg, new_e);

    // Set parent in temp scene (skip for root)
    if (!is_root && parent_in_temp != INVALID_ENTITY) {
        Hierarchy::set_parent(dst_reg, new_e, parent_in_temp);
    }

    // Recurse into children
    auto children = Hierarchy::get_children(src_reg, entity);
    for (Entity child : children) {
        copy_entity_recursive(scene, child, temp, new_e, false);
    }
}

Prefab Prefab::from_entity(const Scene& scene, Entity entity, const std::string& name) {
    auto& src_reg = scene.registry();

    if (!src_reg.alive(entity)) {
        NX_WARN("Prefab::from_entity: entity is not alive");
        return Prefab{};
    }

    // Create a temporary scene containing this entity and all descendants
    Scene temp;
    copy_entity_recursive(scene, entity, temp, INVALID_ENTITY, true);

    SceneSerializer serializer(temp);
    std::string json = serializer.to_json();

    return Prefab{name, json};
}

Entity Prefab::instantiate(Scene& scene) const {
    if (!valid()) return INVALID_ENTITY;

    // Load into a temp scene
    Scene temp;
    SceneSerializer serializer(temp);
    if (!serializer.from_json(json_data_)) {
        NX_WARN("Prefab::instantiate: failed to deserialize prefab '{}'", name_);
        return INVALID_ENTITY;
    }

    // Phase 1: Copy all entities from temp to target scene, building an ID mapping
    std::unordered_map<Entity, Entity> entity_map; // temp entity -> scene entity
    Entity first_entity = INVALID_ENTITY;

    temp.registry().each<TagComponent>([&](Entity src, TagComponent& tag) {
        auto& src_reg = temp.registry();
        Entity dst = scene.create_entity(tag.name);

        if (first_entity == INVALID_ENTITY) first_entity = dst;

        copy_entity_components(src_reg, src, scene.registry(), dst);
        entity_map[src] = dst;
    });

    // Phase 2: Restore hierarchy relationships using the entity mapping
    for (auto& [src, dst] : entity_map) {
        auto children = Hierarchy::get_children(temp.registry(), src);
        for (Entity src_child : children) {
            auto it = entity_map.find(src_child);
            if (it != entity_map.end()) {
                Hierarchy::set_parent(scene.registry(), it->second, dst);
            }
        }
    }

    return first_entity;
}

// ── PrefabLibrary ────────────────────────────────────────────────────────────

void PrefabLibrary::add(const Prefab& prefab) {
    prefabs_[prefab.name()] = prefab;
}

void PrefabLibrary::remove(const std::string& name) {
    prefabs_.erase(name);
}

const Prefab* PrefabLibrary::get(const std::string& name) const {
    auto it = prefabs_.find(name);
    return it != prefabs_.end() ? &it->second : nullptr;
}

bool PrefabLibrary::has(const std::string& name) const {
    return prefabs_.find(name) != prefabs_.end();
}

Entity PrefabLibrary::instantiate(const std::string& name, Scene& scene) const {
    const Prefab* p = get(name);
    if (!p) {
        NX_WARN("PrefabLibrary: prefab '{}' not found", name);
        return Entity{};
    }
    return p->instantiate(scene);
}

bool PrefabLibrary::save_to_directory(const std::string& dir_path) const {
    namespace fs = std::filesystem;
    fs::create_directories(dir_path);

    for (const auto& [name, prefab] : prefabs_) {
        std::string file = dir_path + "/" + name + ".prefab.json";
        std::ofstream out(file);
        if (!out) {
            NX_WARN("PrefabLibrary: failed to write '{}'", file);
            return false;
        }
        out << prefab.data();
    }
    return true;
}

bool PrefabLibrary::load_from_directory(const std::string& dir_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(dir_path)) return false;

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        std::string path = entry.path().string();
        if (path.size() >= 13 && path.substr(path.size() - 13) == ".prefab.json") {
            std::ifstream in(path);
            if (!in) continue;
            std::string json((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
            std::string name = entry.path().stem().stem().string(); // remove .prefab.json
            prefabs_[name] = Prefab{name, json};
        }
    }
    return true;
}

} // namespace nexus
