#include "nexus/editor/component_registry.h"

#include "nexus/scene/components.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/scene/registry.h"

#include <algorithm>
#include <cctype>

namespace nexus::editor {

namespace {

bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    auto to_lower = [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    };
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (to_lower(static_cast<unsigned char>(haystack[i + j])) !=
                to_lower(static_cast<unsigned char>(needle[j]))) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

}  // namespace

const ComponentDescriptor* ComponentRegistry::find(
    const std::string& type_id) const {
    for (const auto& d : descriptors_) {
        if (d.type_id == type_id) return &d;
    }
    return nullptr;
}

std::vector<const ComponentDescriptor*> ComponentRegistry::matching(
    const std::string& query) const {
    std::vector<const ComponentDescriptor*> out;
    out.reserve(descriptors_.size());
    for (const auto& d : descriptors_) {
        if (icontains(d.name, query) || icontains(d.category, query)) {
            out.push_back(&d);
        }
    }
    return out;
}

std::vector<const ComponentDescriptor*> ComponentRegistry::available_to(
    Registry& reg, u64 entity, const std::string& query) const {
    std::vector<const ComponentDescriptor*> out;
    out.reserve(descriptors_.size());
    for (const auto& d : descriptors_) {
        if (!d.has || !d.add) continue;
        if (d.has(reg, entity)) continue;  // already attached, hide it
        if (icontains(d.name, query) || icontains(d.category, query)) {
            out.push_back(&d);
        }
    }
    return out;
}

std::vector<std::string> ComponentRegistry::categories() const {
    std::vector<std::string> out;
    for (const auto& d : descriptors_) {
        if (std::find(out.begin(), out.end(), d.category) == out.end()) {
            out.push_back(d.category);
        }
    }
    return out;
}

// ── Built-in registration ───────────────────────────────────────────────────
//
// Order here drives the menu's category ordering and intra-category list
// ordering.  Mirrors Unity's "Component" menu layout for screenshot parity.
void register_builtin_components(ComponentRegistry& reg) {
    // Layout / Transform — already added to every entity at create time, but
    // listed here so users can re-add a missing transform after a manual
    // remove via the Inspector's component header gear menu.
    reg.register_type<Transform3DComponent>("Transform 3D",  "Layout");
    reg.register_type<Transform2DComponent>("Transform 2D",  "Layout");
    reg.register_type<HierarchyComponent>  ("Hierarchy",     "Layout");

    // Mesh — visual surface for 3D objects.
    reg.register_type<MeshRendererComponent> ("Mesh Renderer",  "Mesh");
    reg.register_type<SpriteRendererComponent>("Sprite Renderer","Mesh");
    reg.register_type<TilemapComponent>      ("Tilemap",        "Mesh");

    // Effects / Rendering / Camera.
    reg.register_type<CameraComponent>             ("Camera",            "Rendering");
    reg.register_type<DirectionalLightComponent>   ("Directional Light", "Rendering");
    reg.register_type<PointLightComponent>         ("Point Light",       "Rendering");
    reg.register_type<SpotLightComponent>          ("Spot Light",        "Rendering");

    // Physics — 2D and 3D bodies/colliders.
    reg.register_type<RigidBody3DComponent>("Rigid Body 3D", "Physics");
    reg.register_type<Collider3DComponent> ("Collider 3D",   "Physics");
    reg.register_type<RigidBody2DComponent>("Rigid Body 2D", "Physics");
    reg.register_type<Collider2DComponent> ("Collider 2D",   "Physics");

    // Audio.
    reg.register_type<AudioSourceComponent>  ("Audio Source",   "Audio");
    reg.register_type<AudioListenerComponent>("Audio Listener", "Audio");
}

}  // namespace nexus::editor
