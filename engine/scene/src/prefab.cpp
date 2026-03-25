#include "nexus/scene/prefab.h"
#include "nexus/core/log.h"
#include <fstream>
#include <filesystem>

namespace nexus {

Prefab Prefab::from_entity(const Scene& scene, Entity entity, const std::string& name) {
    // Create a temporary scene containing just this entity and serialize it
    Scene temp;
    auto& src_reg = scene.registry();
    auto& dst_reg = temp.registry();

    if (!src_reg.alive(entity)) {
        NX_WARN("Prefab::from_entity: entity is not alive");
        return Prefab{};
    }

    // Create entity in temp scene with same tag
    Entity new_e = temp.create_entity(name);

    // Copy Transform2D if present
    if (src_reg.has_component<Transform2DComponent>(entity)) {
        dst_reg.add_component<Transform2DComponent>(
            new_e, src_reg.get_component<Transform2DComponent>(entity));
    }
    // Copy Transform3D if present
    if (src_reg.has_component<Transform3DComponent>(entity)) {
        dst_reg.add_component<Transform3DComponent>(
            new_e, src_reg.get_component<Transform3DComponent>(entity));
    }
    // Copy SpriteRenderer if present
    if (src_reg.has_component<SpriteRendererComponent>(entity)) {
        dst_reg.add_component<SpriteRendererComponent>(
            new_e, src_reg.get_component<SpriteRendererComponent>(entity));
    }
    // Copy MeshRenderer if present
    if (src_reg.has_component<MeshRendererComponent>(entity)) {
        dst_reg.add_component<MeshRendererComponent>(
            new_e, src_reg.get_component<MeshRendererComponent>(entity));
    }
    // Copy Camera if present
    if (src_reg.has_component<CameraComponent>(entity)) {
        dst_reg.add_component<CameraComponent>(
            new_e, src_reg.get_component<CameraComponent>(entity));
    }
    // Copy RigidBody2D if present
    if (src_reg.has_component<RigidBody2DComponent>(entity)) {
        dst_reg.add_component<RigidBody2DComponent>(
            new_e, src_reg.get_component<RigidBody2DComponent>(entity));
    }
    // Copy Collider2D if present
    if (src_reg.has_component<Collider2DComponent>(entity)) {
        dst_reg.add_component<Collider2DComponent>(
            new_e, src_reg.get_component<Collider2DComponent>(entity));
    }
    // Copy RigidBody3D if present
    if (src_reg.has_component<RigidBody3DComponent>(entity)) {
        dst_reg.add_component<RigidBody3DComponent>(
            new_e, src_reg.get_component<RigidBody3DComponent>(entity));
    }
    // Copy Collider3D if present
    if (src_reg.has_component<Collider3DComponent>(entity)) {
        dst_reg.add_component<Collider3DComponent>(
            new_e, src_reg.get_component<Collider3DComponent>(entity));
    }
    // Copy lights
    if (src_reg.has_component<DirectionalLightComponent>(entity)) {
        dst_reg.add_component<DirectionalLightComponent>(
            new_e, src_reg.get_component<DirectionalLightComponent>(entity));
    }
    if (src_reg.has_component<PointLightComponent>(entity)) {
        dst_reg.add_component<PointLightComponent>(
            new_e, src_reg.get_component<PointLightComponent>(entity));
    }

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

    // Copy entities from temp to target scene via TagComponent iteration
    Entity first_entity = INVALID_ENTITY;
    temp.registry().each<TagComponent>([&](Entity src, TagComponent& tag) {
        auto& src_reg = temp.registry();
        Entity dst = scene.create_entity(tag.name);

        if (first_entity == INVALID_ENTITY) first_entity = dst;

        auto& dst_reg = scene.registry();
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
    });

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
