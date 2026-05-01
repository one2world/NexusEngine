#include "nexus/scene/scene_serializer.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/core/log.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace nexus {

// ── JSON helpers ────────────────────────────────────────────────────────────

static json vec2_to_json(Vec2 v) { return {v.x, v.y}; }
static json vec3_to_json(Vec3 v) { return {v.x, v.y, v.z}; }
static json vec4_to_json(Vec4 v) { return {v.x, v.y, v.z, v.w}; }
static json quat_to_json(Quat q) { return {q.w, q.x, q.y, q.z}; }

static Vec2 json_to_vec2(const json& j) {
    return {j[0].get<float>(), j[1].get<float>()};
}
static Vec3 json_to_vec3(const json& j) {
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}
static Vec4 json_to_vec4(const json& j) {
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}
static Quat json_to_quat(const json& j) {
    return Quat{j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

// ── Serialize entity to JSON ────────────────────────────────────────────────

static json serialize_entity(const Registry& reg, Entity e) {
    json entity_json;
    entity_json["id"] = e;

    if (reg.has_component<TagComponent>(e)) {
        auto& tag = reg.get_component<TagComponent>(e);
        entity_json["tag"] = tag.name;
        // Optional Unity-style category and layer — only emitted when
        // non-default so legacy scenes round-trip without bloat.
        if (tag.category != "Untagged") {
            entity_json["tag_category"] = tag.category;
        }
        if (tag.layer != 0) {
            entity_json["tag_layer"] = tag.layer;
        }
    }

    // Editor-authored presentation markers — active/visibility and lock.
    // Active defaults to true; only emit when false so clean scenes stay tidy.
    if (reg.has_component<ActiveComponent>(e)) {
        const auto& ac = reg.get_component<ActiveComponent>(e);
        if (!ac.active) entity_json["active"] = false;
    }
    if (reg.has_component<LockedComponent>(e)) {
        entity_json["locked"] = true;
    }

    if (reg.has_component<Transform2DComponent>(e)) {
        auto& t = reg.get_component<Transform2DComponent>(e);
        entity_json["transform2d"] = {
            {"position", vec2_to_json(t.position)},
            {"rotation", t.rotation},
            {"scale", vec2_to_json(t.scale)}
        };
    }

    if (reg.has_component<Transform3DComponent>(e)) {
        auto& t = reg.get_component<Transform3DComponent>(e);
        entity_json["transform3d"] = {
            {"position", vec3_to_json(t.position)},
            {"rotation", quat_to_json(t.rotation)},
            {"scale", vec3_to_json(t.scale)}
        };
    }

    if (reg.has_component<SpriteRendererComponent>(e)) {
        auto& s = reg.get_component<SpriteRendererComponent>(e);
        entity_json["sprite"] = {
            {"texture_id", s.texture_id},
            {"color", vec4_to_json(s.color)},
            {"uv_min", vec2_to_json(s.uv_min)},
            {"uv_max", vec2_to_json(s.uv_max)},
            {"sort_order", s.sort_order}
        };
    }

    if (reg.has_component<MeshRendererComponent>(e)) {
        auto& m = reg.get_component<MeshRendererComponent>(e);
        json mr = {
            {"mesh_id", m.mesh_id},
            {"material_id", m.material_id}
        };
        // Lighting / probes / additional settings — emit only when they
        // diverge from defaults so common scenes stay tidy on disk.
        if (!m.cast_shadows)        mr["cast_shadows"] = false;
        if (!m.receive_shadows)     mr["receive_shadows"] = false;
        if (!m.dynamic_occlusion)   mr["dynamic_occlusion"] = false;
        if (m.light_probes != LightProbesMode::BlendProbes) {
            mr["light_probes"] = static_cast<int>(m.light_probes);
        }
        if (m.reflection_probes != ReflectionProbesMode::BlendProbes) {
            mr["reflection_probes"] = static_cast<int>(m.reflection_probes);
        }
        // Tint is editable from the Inspector and stored per-entity, so it
        // must persist.  Default white is the common case and is elided.
        if (m.tint.x != 1.0f || m.tint.y != 1.0f ||
            m.tint.z != 1.0f || m.tint.w != 1.0f) {
            mr["tint"] = {m.tint.x, m.tint.y, m.tint.z, m.tint.w};
        }
        entity_json["mesh_renderer"] = std::move(mr);
    }

    if (reg.has_component<CameraComponent>(e)) {
        auto& c = reg.get_component<CameraComponent>(e);
        entity_json["camera"] = {
            {"is_primary", c.is_primary},
            {"is_orthographic", c.is_orthographic},
            {"fov", c.fov},
            {"ortho_size", c.ortho_size},
            {"near_clip", c.near_clip},
            {"far_clip", c.far_clip}
        };
    }

    if (reg.has_component<DirectionalLightComponent>(e)) {
        auto& l = reg.get_component<DirectionalLightComponent>(e);
        entity_json["directional_light"] = {
            {"color", vec3_to_json(l.color)},
            {"intensity", l.intensity}
        };
    }

    if (reg.has_component<PointLightComponent>(e)) {
        auto& l = reg.get_component<PointLightComponent>(e);
        entity_json["point_light"] = {
            {"color", vec3_to_json(l.color)},
            {"intensity", l.intensity},
            {"radius", l.radius}
        };
    }

    if (reg.has_component<RigidBody2DComponent>(e)) {
        auto& rb = reg.get_component<RigidBody2DComponent>(e);
        entity_json["rigidbody2d"] = {
            {"type", static_cast<u8>(rb.type)},
            {"density", rb.density},
            {"friction", rb.friction},
            {"restitution", rb.restitution},
            {"linear_damping", rb.linear_damping},
            {"angular_damping", rb.angular_damping},
            {"gravity_scale", rb.gravity_scale},
            {"fixed_rotation", rb.fixed_rotation},
            {"velocity", vec2_to_json(rb.velocity)},
            {"angular_velocity", rb.angular_velocity}
        };
    }

    if (reg.has_component<Collider2DComponent>(e)) {
        auto& c = reg.get_component<Collider2DComponent>(e);
        entity_json["collider2d"] = {
            {"shape", static_cast<u8>(c.shape)},
            {"offset", vec2_to_json(c.offset)},
            {"half_size", vec2_to_json(c.half_size)},
            {"radius", c.radius},
            {"is_trigger", c.is_trigger},
            {"layer", c.layer},
            {"mask", c.mask}
        };
    }

    if (reg.has_component<RigidBody3DComponent>(e)) {
        auto& rb = reg.get_component<RigidBody3DComponent>(e);
        entity_json["rigidbody3d"] = {
            {"type", static_cast<u8>(rb.type)},
            {"mass", rb.mass},
            {"friction", rb.friction},
            {"restitution", rb.restitution},
            {"linear_damping", rb.linear_damping},
            {"angular_damping", rb.angular_damping},
            {"gravity_scale", rb.gravity_scale},
            {"velocity", vec3_to_json(rb.velocity)},
            {"angular_velocity", vec3_to_json(rb.angular_velocity)}
        };
    }

    if (reg.has_component<Collider3DComponent>(e)) {
        auto& c = reg.get_component<Collider3DComponent>(e);
        entity_json["collider3d"] = {
            {"shape", static_cast<u8>(c.shape)},
            {"offset", vec3_to_json(c.offset)},
            {"half_extents", vec3_to_json(c.half_extents)},
            {"radius", c.radius},
            {"height", c.height},
            {"is_trigger", c.is_trigger},
            {"layer", c.layer},
            {"mask", c.mask}
        };
    }

    if (reg.has_component<AudioSourceComponent>(e)) {
        auto& a = reg.get_component<AudioSourceComponent>(e);
        entity_json["audio_source"] = {
            {"clip_id", a.clip_id},
            {"volume", a.volume},
            {"pitch", a.pitch},
            {"min_distance", a.min_distance},
            {"max_distance", a.max_distance},
            {"looping", a.looping},
            {"spatial", a.spatial},
            {"play_on_start", a.play_on_start},
            {"bus", a.bus}
        };
    }

    if (reg.has_component<AudioListenerComponent>(e)) {
        auto& a = reg.get_component<AudioListenerComponent>(e);
        entity_json["audio_listener"] = {
            {"active", a.active}
        };
    }

    if (reg.has_component<TilemapComponent>(e)) {
        auto& tm = reg.get_component<TilemapComponent>(e);
        entity_json["tilemap"] = {
            {"width", tm.width},
            {"height", tm.height},
            {"tile_size", tm.tile_size},
            {"texture_id", tm.texture_id},
            {"tiles_per_row", tm.tiles_per_row},
            {"tiles_per_col", tm.tiles_per_col},
            {"tiles", tm.tiles}
        };
    }

    if (reg.has_component<HierarchyComponent>(e)) {
        auto& h = reg.get_component<HierarchyComponent>(e);
        if (h.parent != INVALID_ENTITY) {
            entity_json["parent"] = h.parent;
        }
    }

    return entity_json;
}

// ── Deserialize entity from JSON ────────────────────────────────────────────

static Entity deserialize_entity(Registry& reg, const json& j,
                                 std::unordered_map<u32, Entity>& id_map) {
    Entity e = reg.create();
    u32 original_id = j.value("id", 0u);
    id_map[original_id] = e;

    if (j.contains("tag")) {
        TagComponent tag;
        tag.name = j["tag"].get<std::string>();
        if (j.contains("tag_category")) {
            tag.category = j["tag_category"].get<std::string>();
        }
        if (j.contains("tag_layer")) {
            tag.layer = j["tag_layer"].get<i32>();
        }
        reg.add_component<TagComponent>(e, std::move(tag));
    }

    // Editor presentation markers — symmetric with serialize_entity.
    if (j.contains("active") && !j["active"].get<bool>()) {
        reg.add_component<ActiveComponent>(e, ActiveComponent{false});
    }
    if (j.contains("locked") && j["locked"].get<bool>()) {
        reg.add_component<LockedComponent>(e, LockedComponent{});
    }

    if (j.contains("transform2d")) {
        auto& t = j["transform2d"];
        Transform2DComponent comp;
        comp.position = json_to_vec2(t["position"]);
        comp.rotation = t["rotation"].get<float>();
        comp.scale = json_to_vec2(t["scale"]);
        reg.add_component<Transform2DComponent>(e, comp);
    }

    if (j.contains("transform3d")) {
        auto& t = j["transform3d"];
        Transform3DComponent comp;
        comp.position = json_to_vec3(t["position"]);
        comp.rotation = json_to_quat(t["rotation"]);
        comp.scale = json_to_vec3(t["scale"]);
        reg.add_component<Transform3DComponent>(e, comp);
    }

    if (j.contains("sprite")) {
        auto& s = j["sprite"];
        SpriteRendererComponent comp;
        comp.texture_id = s["texture_id"].get<u32>();
        comp.color = json_to_vec4(s["color"]);
        comp.uv_min = json_to_vec2(s["uv_min"]);
        comp.uv_max = json_to_vec2(s["uv_max"]);
        comp.sort_order = s["sort_order"].get<i32>();
        reg.add_component<SpriteRendererComponent>(e, comp);
    }

    if (j.contains("mesh_renderer")) {
        auto& m = j["mesh_renderer"];
        MeshRendererComponent comp;
        comp.mesh_id = m["mesh_id"].get<u32>();
        comp.material_id = m["material_id"].get<u32>();
        if (m.contains("cast_shadows"))      comp.cast_shadows      = m["cast_shadows"].get<bool>();
        if (m.contains("receive_shadows"))   comp.receive_shadows   = m["receive_shadows"].get<bool>();
        if (m.contains("dynamic_occlusion")) comp.dynamic_occlusion = m["dynamic_occlusion"].get<bool>();
        if (m.contains("light_probes")) {
            comp.light_probes = static_cast<LightProbesMode>(m["light_probes"].get<int>());
        }
        if (m.contains("reflection_probes")) {
            comp.reflection_probes =
                static_cast<ReflectionProbesMode>(m["reflection_probes"].get<int>());
        }
        if (m.contains("tint") && m["tint"].is_array() && m["tint"].size() == 4) {
            comp.tint = Vec4(m["tint"][0].get<f32>(), m["tint"][1].get<f32>(),
                             m["tint"][2].get<f32>(), m["tint"][3].get<f32>());
        }
        reg.add_component<MeshRendererComponent>(e, comp);
    }

    if (j.contains("camera")) {
        auto& c = j["camera"];
        CameraComponent comp;
        comp.is_primary = c["is_primary"].get<bool>();
        comp.is_orthographic = c["is_orthographic"].get<bool>();
        comp.fov = c["fov"].get<float>();
        comp.ortho_size = c["ortho_size"].get<float>();
        comp.near_clip = c["near_clip"].get<float>();
        comp.far_clip = c["far_clip"].get<float>();
        reg.add_component<CameraComponent>(e, comp);
    }

    if (j.contains("directional_light")) {
        auto& l = j["directional_light"];
        DirectionalLightComponent comp;
        comp.color = json_to_vec3(l["color"]);
        comp.intensity = l["intensity"].get<float>();
        reg.add_component<DirectionalLightComponent>(e, comp);
    }

    if (j.contains("point_light")) {
        auto& l = j["point_light"];
        PointLightComponent comp;
        comp.color = json_to_vec3(l["color"]);
        comp.intensity = l["intensity"].get<float>();
        comp.radius = l["radius"].get<float>();
        reg.add_component<PointLightComponent>(e, comp);
    }

    if (j.contains("rigidbody2d")) {
        auto& r = j["rigidbody2d"];
        RigidBody2DComponent comp;
        comp.type = static_cast<RigidBody2DComponent::Type>(r["type"].get<u8>());
        comp.density = r["density"].get<float>();
        comp.friction = r["friction"].get<float>();
        comp.restitution = r["restitution"].get<float>();
        comp.linear_damping = r["linear_damping"].get<float>();
        comp.angular_damping = r["angular_damping"].get<float>();
        comp.gravity_scale = r["gravity_scale"].get<float>();
        comp.fixed_rotation = r["fixed_rotation"].get<bool>();
        comp.velocity = json_to_vec2(r["velocity"]);
        comp.angular_velocity = r["angular_velocity"].get<float>();
        reg.add_component<RigidBody2DComponent>(e, comp);
    }

    if (j.contains("collider2d")) {
        auto& c = j["collider2d"];
        Collider2DComponent comp;
        comp.shape = static_cast<Collider2DComponent::Shape>(c["shape"].get<u8>());
        comp.offset = json_to_vec2(c["offset"]);
        comp.half_size = json_to_vec2(c["half_size"]);
        comp.radius = c["radius"].get<float>();
        comp.is_trigger = c["is_trigger"].get<bool>();
        comp.layer = c["layer"].get<u16>();
        comp.mask = c["mask"].get<u16>();
        reg.add_component<Collider2DComponent>(e, comp);
    }

    if (j.contains("rigidbody3d")) {
        auto& r = j["rigidbody3d"];
        RigidBody3DComponent comp;
        comp.type = static_cast<RigidBody3DComponent::Type>(r["type"].get<u8>());
        comp.mass = r["mass"].get<float>();
        comp.friction = r["friction"].get<float>();
        comp.restitution = r["restitution"].get<float>();
        comp.linear_damping = r["linear_damping"].get<float>();
        comp.angular_damping = r["angular_damping"].get<float>();
        comp.gravity_scale = r["gravity_scale"].get<float>();
        comp.velocity = json_to_vec3(r["velocity"]);
        comp.angular_velocity = json_to_vec3(r["angular_velocity"]);
        reg.add_component<RigidBody3DComponent>(e, comp);
    }

    if (j.contains("collider3d")) {
        auto& c = j["collider3d"];
        Collider3DComponent comp;
        comp.shape = static_cast<Collider3DComponent::Shape>(c["shape"].get<u8>());
        comp.offset = json_to_vec3(c["offset"]);
        comp.half_extents = json_to_vec3(c["half_extents"]);
        comp.radius = c["radius"].get<float>();
        comp.height = c["height"].get<float>();
        comp.is_trigger = c["is_trigger"].get<bool>();
        comp.layer = c["layer"].get<u16>();
        comp.mask = c["mask"].get<u16>();
        reg.add_component<Collider3DComponent>(e, comp);
    }

    if (j.contains("audio_source")) {
        auto& a = j["audio_source"];
        AudioSourceComponent comp;
        comp.clip_id = a["clip_id"].get<u32>();
        comp.volume = a["volume"].get<float>();
        comp.pitch = a["pitch"].get<float>();
        comp.min_distance = a["min_distance"].get<float>();
        comp.max_distance = a["max_distance"].get<float>();
        comp.looping = a["looping"].get<bool>();
        comp.spatial = a["spatial"].get<bool>();
        comp.play_on_start = a["play_on_start"].get<bool>();
        comp.bus = a["bus"].get<u32>();
        reg.add_component<AudioSourceComponent>(e, comp);
    }

    if (j.contains("tilemap")) {
        auto& tm = j["tilemap"];
        TilemapComponent comp;
        comp.width = tm["width"].get<u32>();
        comp.height = tm["height"].get<u32>();
        comp.tile_size = tm["tile_size"].get<float>();
        comp.texture_id = tm["texture_id"].get<u32>();
        comp.tiles_per_row = tm["tiles_per_row"].get<u32>();
        comp.tiles_per_col = tm["tiles_per_col"].get<u32>();
        comp.tiles = tm["tiles"].get<std::vector<i32>>();
        reg.add_component<TilemapComponent>(e, std::move(comp));
    }

    if (j.contains("audio_listener")) {
        auto& a = j["audio_listener"];
        AudioListenerComponent comp;
        comp.active = a["active"].get<bool>();
        reg.add_component<AudioListenerComponent>(e, comp);
    }

    return e;
}

// ── Public API ──────────────────────────────────────────────────────────────

static constexpr int SCENE_FORMAT_VERSION = 2;

std::string SceneSerializer::to_json() const {
    json root;
    root["version"] = SCENE_FORMAT_VERSION;
    root["entities"] = json::array();

    auto& reg = scene_.registry();
    auto entities = reg.view<TagComponent>();

    for (Entity e : entities) {
        root["entities"].push_back(serialize_entity(reg, e));
    }

    return root.dump(2);
}

bool SceneSerializer::from_json(const std::string& json_str) {
    // Pre-validate the JSON before modifying any state
    json root;
    try {
        root = json::parse(json_str);
    } catch (const json::exception& e) {
        NX_ERROR("SceneSerializer JSON parse error: {}", e.what());
        return false;
    }

    if (!root.contains("entities") || !root["entities"].is_array()) {
        NX_ERROR("SceneSerializer: invalid JSON format");
        return false;
    }

    // Version check
    int version = root.value("version", 1);
    if (version > SCENE_FORMAT_VERSION) {
        NX_ERROR("SceneSerializer: file version {} is newer than supported version {}",
                 version, SCENE_FORMAT_VERSION);
        return false;
    }

    // Transactional safety: take a backup before clearing.
    std::string backup_json = to_json();

    try {
        scene_.clear();
        auto& reg = scene_.registry();

        // First pass: create all entities
        std::unordered_map<u32, Entity> id_map;
        for (auto& entity_json : root["entities"]) {
            deserialize_entity(reg, entity_json, id_map);
        }

        // Second pass: restore hierarchy with orphan detection
        u32 orphaned_count = 0;
        for (auto& entity_json : root["entities"]) {
            if (entity_json.contains("parent")) {
                u32 original_id = entity_json["id"].get<u32>();
                u32 parent_id = entity_json["parent"].get<u32>();

                auto child_it = id_map.find(original_id);
                auto parent_it = id_map.find(parent_id);
                if (child_it != id_map.end() && parent_it != id_map.end()) {
                    Hierarchy::set_parent(reg, child_it->second, parent_it->second);
                } else {
                    ++orphaned_count;
                    NX_WARN("SceneSerializer: entity {} has parent {} which was not found — orphaned",
                            original_id, parent_id);
                }
            }
        }

        if (orphaned_count > 0) {
            NX_WARN("SceneSerializer: {} entities were orphaned during deserialization",
                    orphaned_count);
        }

        NX_INFO("Scene loaded: {} entities (format v{})", root["entities"].size(), version);
        return true;
    } catch (const std::exception& e) {
        NX_ERROR("SceneSerializer error during deserialization: {} — restoring backup", e.what());
        // Attempt rollback from backup
        try {
            scene_.clear();
            json backup_root = json::parse(backup_json);
            auto& reg = scene_.registry();
            std::unordered_map<u32, Entity> id_map;
            for (auto& entity_json : backup_root["entities"]) {
                deserialize_entity(reg, entity_json, id_map);
            }
            for (auto& entity_json : backup_root["entities"]) {
                if (entity_json.contains("parent")) {
                    auto child_it = id_map.find(entity_json["id"].get<u32>());
                    auto parent_it = id_map.find(entity_json["parent"].get<u32>());
                    if (child_it != id_map.end() && parent_it != id_map.end()) {
                        Hierarchy::set_parent(reg, child_it->second, parent_it->second);
                    }
                }
            }
            NX_WARN("SceneSerializer: backup restored successfully");
        } catch (...) {
            NX_ERROR("SceneSerializer: CRITICAL — backup restoration also failed");
        }
        return false;
    }
}

bool SceneSerializer::save(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        NX_ERROR("SceneSerializer: cannot open file for writing: {}", filepath);
        return false;
    }
    file << to_json();
    NX_INFO("Scene saved to: {}", filepath);
    return true;
}

bool SceneSerializer::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        NX_ERROR("SceneSerializer: cannot open file for reading: {}", filepath);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return from_json(content);
}

Entity SceneSerializer::duplicate_entity(Entity src) {
    auto& reg = scene_.registry();
    if (!reg.alive(src)) return INVALID_ENTITY;

    // Snapshot the source subtree (root + descendants) through the same
    // serialize_entity path used by save()/to_json(), guaranteeing feature
    // parity with scene files: every component the serializer knows round-trips
    // cleanly into the duplicate.
    std::vector<Entity> subtree;
    subtree.reserve(8);
    subtree.push_back(src);
    auto descendants = Hierarchy::get_descendants(reg, src);
    subtree.insert(subtree.end(), descendants.begin(), descendants.end());

    std::vector<json> snapshot;
    snapshot.reserve(subtree.size());
    for (Entity e : subtree) {
        snapshot.push_back(serialize_entity(reg, e));
    }

    // Recreate each entity; id_map is keyed on the original id so the second
    // pass can restore child → parent wiring inside the duplicated subtree.
    std::unordered_map<u32, Entity> id_map;
    Entity duplicated_root = INVALID_ENTITY;
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        Entity new_e = deserialize_entity(reg, snapshot[i], id_map);
        if (i == 0) duplicated_root = new_e;
    }
    if (duplicated_root == INVALID_ENTITY) return INVALID_ENTITY;

    // Restore internal hierarchy (only for non-root nodes — the root gets its
    // parent rewritten below so the duplicate becomes a sibling of `src`).
    for (std::size_t i = 1; i < snapshot.size(); ++i) {
        const json& ej = snapshot[i];
        if (!ej.contains("parent")) continue;
        auto child_it  = id_map.find(ej["id"].get<u32>());
        auto parent_it = id_map.find(ej["parent"].get<u32>());
        if (child_it != id_map.end() && parent_it != id_map.end()) {
            Hierarchy::set_parent(reg, child_it->second, parent_it->second);
        }
    }

    // Parent the duplicated root where the source lives (same sibling level).
    if (reg.has_component<HierarchyComponent>(src)) {
        Entity src_parent = reg.get_component<HierarchyComponent>(src).parent;
        if (src_parent != INVALID_ENTITY && reg.alive(src_parent)) {
            Hierarchy::set_parent(reg, duplicated_root, src_parent);
        }
    }

    // Append "(N)" to the root tag — find first unused suffix so repeated
    // duplications stay distinguishable instead of colliding.
    if (reg.has_component<TagComponent>(duplicated_root)) {
        auto& tag = reg.get_component<TagComponent>(duplicated_root);
        const std::string base = tag.name;
        for (int n = 1; n < 10000; ++n) {
            std::string candidate = base + " (" + std::to_string(n) + ")";
            bool collision = false;
            reg.each<TagComponent>([&](Entity e, TagComponent& other) {
                if (e != duplicated_root && other.name == candidate) collision = true;
            });
            if (!collision) { tag.name = candidate; break; }
        }
    }

    return duplicated_root;
}

} // namespace nexus
