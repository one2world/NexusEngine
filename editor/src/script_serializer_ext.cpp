// ─────────────────────────────────────────────────────────────────────────────
// script_serializer_ext.cpp — register ScriptComponent JSON support
// ─────────────────────────────────────────────────────────────────────────────
//
// Lives in the editor library because engine/scene cannot depend on
// engine/scripting (scripting depends on scene).  By plugging in via
// SceneSerializer's extension hook (M19) we round-trip ScriptComponent
// alongside every other component without breaking the layer boundary.
//
// JSON shape:
//   "script": { "name": "...", "enabled": true }
//
// `context` and the lifecycle callbacks deliberately are NOT serialised
// — they're rebuilt by ScriptSystem on the next tick (initialized=false
// causes on_create to fire).

#include "nexus/scene/scene_serializer.h"
#include "nexus/scene/registry.h"
#include "nexus/scripting/script_component.h"

#include <nlohmann/json.hpp>

namespace nexus::editor {

void install_script_serializer_extension() {
    // Idempotent — scene_serializer keeps a vector and clear_extensions()
    // is the only way to reset.  Calling install twice would double-fire
    // the writer/reader.  Caller (editor_main) guarantees one-shot.
    using nlohmann::json;

    SceneSerializer::register_extension(
        "ScriptComponent",
        // Writer
        [](void* entity_json_ptr, const Registry& reg, Entity e) {
            auto* j = static_cast<json*>(entity_json_ptr);
            if (!reg.has_component<scripting::ScriptComponent>(e)) return;
            const auto& sc =
                reg.get_component<scripting::ScriptComponent>(e);
            (*j)["script"] = {
                {"name",    sc.script_name},
                {"enabled", sc.enabled},
            };
        },
        // Reader
        [](const void* entity_json_ptr, Registry& reg, Entity e) {
            const auto* j = static_cast<const json*>(entity_json_ptr);
            if (!j->contains("script")) return;
            const auto& sj = (*j)["script"];
            scripting::ScriptComponent sc;
            sc.script_name = sj.value("name",    std::string{});
            sc.enabled     = sj.value("enabled", true);
            // initialized stays false so ScriptSystem rebinds + fires
            // on_create on the next tick.
            sc.initialized = false;
            reg.add_component<scripting::ScriptComponent>(e, std::move(sc));
        });
}

}  // namespace nexus::editor
