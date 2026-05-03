#include "nexus/scene/prefab.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/core/log.h"
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace nexus {

// Helper: copy all known components from src entity to dst entity.
//
// Must mirror SceneSerializer's component coverage — instantiate() loads a
// prefab into a temp scene via the serializer (which restores everything),
// then copies into the target via this helper.  Missing entries here cause
// silent data loss when a prefab is instantiated.  The TagComponent itself
// is omitted because callers always set it via `create_entity(name)` first.
template <typename C>
static inline void copy_one(const Registry& src_reg, Entity src,
                             Registry& dst_reg, Entity dst) {
    if (src_reg.has_component<C>(src)) {
        dst_reg.add_component<C>(dst, src_reg.get_component<C>(src));
    }
}

static void copy_entity_components(const Registry& src_reg, Entity src,
                                    Registry& dst_reg, Entity dst) {
    // Lifecycle / metadata flags.  ActiveComponent and LockedComponent are
    // marker components that don't exist on every entity by default.
    if (src_reg.has_component<ActiveComponent>(src)) {
        dst_reg.set_active(dst, src_reg.is_active(src));
    }
    copy_one<LockedComponent>(src_reg, src, dst_reg, dst);

    // Transforms.
    copy_one<Transform2DComponent>(src_reg, src, dst_reg, dst);
    copy_one<Transform3DComponent>(src_reg, src, dst_reg, dst);

    // Renderers.
    copy_one<SpriteRendererComponent>(src_reg, src, dst_reg, dst);
    copy_one<MeshRendererComponent>(src_reg, src, dst_reg, dst);
    copy_one<CameraComponent>(src_reg, src, dst_reg, dst);
    copy_one<TilemapComponent>(src_reg, src, dst_reg, dst);

    // Lights.
    copy_one<DirectionalLightComponent>(src_reg, src, dst_reg, dst);
    copy_one<PointLightComponent>(src_reg, src, dst_reg, dst);
    copy_one<SpotLightComponent>(src_reg, src, dst_reg, dst);

    // Physics.
    copy_one<RigidBody2DComponent>(src_reg, src, dst_reg, dst);
    copy_one<Collider2DComponent>(src_reg, src, dst_reg, dst);
    copy_one<RigidBody3DComponent>(src_reg, src, dst_reg, dst);
    copy_one<Collider3DComponent>(src_reg, src, dst_reg, dst);

    // Audio.
    copy_one<AudioSourceComponent>(src_reg, src, dst_reg, dst);
    copy_one<AudioListenerComponent>(src_reg, src, dst_reg, dst);

    // Animator — playhead state is not authored data, but the bindings
    // (clip id, speed, looping, autoplay) are part of the prefab.
    copy_one<AnimatorComponent>(src_reg, src, dst_reg, dst);

    // Skeleton — bone_entities ids reference the prefab's own subtree, so
    // the value carries through unchanged.  When a prefab is instantiated
    // into a new scene the subtree's entity ids change, so callers that
    // rely on bone_entities through Prefab::instantiate need a post-pass
    // remap (M21+).  Round-tripping for snapshots / scene-local copies
    // works as-is.
    copy_one<SkeletonComponent>(src_reg, src, dst_reg, dst);
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
