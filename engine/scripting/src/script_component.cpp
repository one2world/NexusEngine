#include "nexus/scripting/script_component.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/lua_backend.h"
#include "nexus/scene/registry.h"
#include "nexus/core/log.h"

namespace nexus::scripting {

// ── ScriptSystem ────────────────────────────────────────────────────────────
//
// Dispatch model after the Lua 5.4 integration:
//   - Each ScriptComponent.script_name maps to a Lua module that the host
//     loaded with lua_backend.execute*.  The module is expected to define
//     `<script_name>.on_create(entity)`, `on_update(entity, dt)`,
//     `on_destroy(entity)`, `on_collision(self, other)` (any subset).
//   - Dispatch goes through ScriptEngine::call_function which falls
//     through to the lua_State automatically — that keeps the path
//     identical for C++-native NativeFunction modules and Lua modules.
//   - We don't cache FunctionType callbacks anymore: scripts can be
//     hot-reloaded without re-binding components, and there's only one
//     source of truth for whether the function exists (the lua_State).

namespace {

// Probe whether `<module>.<callback>` is callable (either via the C++
// NativeFunction registry or the live Lua VM).  We call it speculatively
// and check engine.errors() for "not found"; that lets us short-circuit
// dispatch for components whose script doesn't define a given lifecycle
// hook without paying the call cost every frame.
bool callback_exists(ScriptEngine& engine,
                     const std::string& module,
                     const std::string& callback) {
    if (engine.find_function(module, callback) != nullptr) return true;
    auto& backend = engine.lua_backend();
    if (!backend.is_initialized()) return false;
    // Look up `<module>.<callback>` directly in Lua: rawly poll the
    // global table so we don't have to construct an args vector or
    // invoke pcall.  The lua_backend.get_global("module.callback")
    // helper walks dotted paths.
    return !backend.get_global(module + "." + callback).is_nil();
}

void invoke(ScriptEngine& engine,
            const std::string& module,
            const std::string& callback,
            std::vector<ScriptValue> args) {
    engine.call_function(module + "." + callback, args);
}

} // namespace

void ScriptSystem::bind_script(ScriptComponent& component) {
    if (!engine_ || component.script_name.empty()) return;
    // Per-entity context preserved for legacy callers reading
    // `component.context`; bookkeeping only — actual state lives in
    // the Lua-side script module.
    component.context = std::make_shared<ScriptContext>(&engine_->globals());
}

void ScriptSystem::initialize_scripts(Registry& registry) {
    if (!engine_) return;
    auto entities = registry.view<ScriptComponent>();
    for (auto entity : entities) {
        auto& sc = registry.get_component<ScriptComponent>(entity);
        if (!sc.enabled || sc.initialized) continue;
        if (sc.script_name.empty()) { sc.initialized = true; continue; }

        if (!sc.context) bind_script(sc);

        if (callback_exists(*engine_, sc.script_name, "on_create")) {
            invoke(*engine_, sc.script_name, "on_create",
                    { ScriptValue::entity(entity) });
        }
        sc.initialized = true;
    }
}

void ScriptSystem::update_scripts(Registry& registry, float dt) {
    if (!engine_) return;
    auto entities = registry.view<ScriptComponent>();
    for (auto entity : entities) {
        auto& sc = registry.get_component<ScriptComponent>(entity);
        if (!sc.enabled || !sc.initialized || sc.script_name.empty()) continue;
        if (!callback_exists(*engine_, sc.script_name, "on_update")) continue;
        invoke(*engine_, sc.script_name, "on_update",
                { ScriptValue::entity(entity), ScriptValue(dt) });
    }
}

void ScriptSystem::destroy_scripts(Registry& registry) {
    if (!engine_) return;
    auto entities = registry.view<ScriptComponent>();
    for (auto entity : entities) {
        auto& sc = registry.get_component<ScriptComponent>(entity);
        if (!sc.initialized || sc.script_name.empty()) continue;
        if (callback_exists(*engine_, sc.script_name, "on_destroy")) {
            invoke(*engine_, sc.script_name, "on_destroy",
                    { ScriptValue::entity(entity) });
        }
        sc.initialized = false;
    }
}

void ScriptSystem::on_collision(Registry& registry, u32 entity_a, u32 entity_b) {
    if (!engine_) return;
    auto fire = [&](u32 self, u32 other) {
        if (!registry.has_component<ScriptComponent>(self)) return;
        auto& sc = registry.get_component<ScriptComponent>(self);
        if (!sc.enabled || !sc.initialized || sc.script_name.empty()) return;
        if (!callback_exists(*engine_, sc.script_name, "on_collision")) return;
        invoke(*engine_, sc.script_name, "on_collision",
                { ScriptValue::entity(self), ScriptValue::entity(other) });
    };
    fire(entity_a, entity_b);
    fire(entity_b, entity_a);
}

} // namespace nexus::scripting
