#include "nexus/scripting/script_component.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/scene/registry.h"
#include "nexus/core/log.h"

namespace nexus::scripting {

// ── ScriptSystem ────────────────────────────────────────────────────────────

void ScriptSystem::bind_script(ScriptComponent& component) {
    if (!engine_ || component.script_name.empty()) return;

    // Create per-entity context with engine globals as parent
    component.context = std::make_shared<ScriptContext>(&engine_->globals());

    // Look up lifecycle functions from registered scripts
    std::string prefix = component.script_name;

    auto find_callback = [&](const std::string& callback_name)
        -> ScriptValue::FunctionType {
        auto* nf = engine_->find_function(prefix, callback_name);
        if (nf) return nf->func;
        return nullptr;
    };

    component.on_create    = find_callback("on_create");
    component.on_update    = find_callback("on_update");
    component.on_destroy   = find_callback("on_destroy");
    component.on_collision = find_callback("on_collision");
}

void ScriptSystem::initialize_scripts(Registry& registry) {
    auto entities = registry.view<ScriptComponent>();
    for (auto entity : entities) {
        auto& sc = registry.get_component<ScriptComponent>(entity);
        if (!sc.enabled || sc.initialized) continue;

        if (!sc.context) {
            bind_script(sc);
        }

        if (sc.on_create) {
            std::vector<ScriptValue> args;
            args.push_back(ScriptValue::entity(entity));
            sc.on_create(args);
        }
        sc.initialized = true;
    }
}

void ScriptSystem::update_scripts(Registry& registry, float dt) {
    auto entities = registry.view<ScriptComponent>();
    for (auto entity : entities) {
        auto& sc = registry.get_component<ScriptComponent>(entity);
        if (!sc.enabled || !sc.initialized) continue;

        if (sc.on_update) {
            std::vector<ScriptValue> args;
            args.push_back(ScriptValue::entity(entity));
            args.push_back(ScriptValue(dt));
            sc.on_update(args);
        }
    }
}

void ScriptSystem::destroy_scripts(Registry& registry) {
    auto entities = registry.view<ScriptComponent>();
    for (auto entity : entities) {
        auto& sc = registry.get_component<ScriptComponent>(entity);
        if (!sc.initialized) continue;

        if (sc.on_destroy) {
            std::vector<ScriptValue> args;
            args.push_back(ScriptValue::entity(entity));
            sc.on_destroy(args);
        }
        sc.initialized = false;
    }
}

void ScriptSystem::on_collision(Registry& registry, u32 entity_a, u32 entity_b) {
    if (registry.has_component<ScriptComponent>(entity_a)) {
        auto& sc = registry.get_component<ScriptComponent>(entity_a);
        if (sc.enabled && sc.initialized && sc.on_collision) {
            std::vector<ScriptValue> args;
            args.push_back(ScriptValue::entity(entity_a));
            args.push_back(ScriptValue::entity(entity_b));
            sc.on_collision(args);
        }
    }

    if (registry.has_component<ScriptComponent>(entity_b)) {
        auto& sc = registry.get_component<ScriptComponent>(entity_b);
        if (sc.enabled && sc.initialized && sc.on_collision) {
            std::vector<ScriptValue> args;
            args.push_back(ScriptValue::entity(entity_b));
            args.push_back(ScriptValue::entity(entity_a));
            sc.on_collision(args);
        }
    }
}

} // namespace nexus::scripting
