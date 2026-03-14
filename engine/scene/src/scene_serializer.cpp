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
        entity_json["mesh_renderer"] = {
            {"mesh_id", m.mesh_id},
            {"material_id", m.material_id}
        };
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
        reg.add_component<TagComponent>(e, TagComponent{j["tag"].get<std::string>()});
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

    return e;
}

// ── Public API ──────────────────────────────────────────────────────────────

std::string SceneSerializer::to_json() const {
    json root;
    root["version"] = 1;
    root["entities"] = json::array();

    auto& reg = scene_.registry();
    auto entities = reg.view<TagComponent>();

    for (Entity e : entities) {
        root["entities"].push_back(serialize_entity(reg, e));
    }

    return root.dump(2);
}

bool SceneSerializer::from_json(const std::string& json_str) {
    try {
        json root = json::parse(json_str);

        if (!root.contains("entities") || !root["entities"].is_array()) {
            NX_ERROR("SceneSerializer: invalid JSON format");
            return false;
        }

        scene_.clear();
        auto& reg = scene_.registry();

        // First pass: create all entities
        std::unordered_map<u32, Entity> id_map;
        for (auto& entity_json : root["entities"]) {
            deserialize_entity(reg, entity_json, id_map);
        }

        // Second pass: restore hierarchy
        for (auto& entity_json : root["entities"]) {
            if (entity_json.contains("parent")) {
                u32 original_id = entity_json["id"].get<u32>();
                u32 parent_id = entity_json["parent"].get<u32>();

                auto child_it = id_map.find(original_id);
                auto parent_it = id_map.find(parent_id);
                if (child_it != id_map.end() && parent_it != id_map.end()) {
                    Hierarchy::set_parent(reg, child_it->second, parent_it->second);
                }
            }
        }

        NX_INFO("Scene loaded: {} entities", root["entities"].size());
        return true;
    } catch (const json::exception& e) {
        NX_ERROR("SceneSerializer JSON error: {}", e.what());
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

} // namespace nexus
