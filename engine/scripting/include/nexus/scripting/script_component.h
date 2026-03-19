#pragma once

#include "nexus/scripting/script_value.h"
#include "nexus/core/types.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

namespace nexus {
class Registry;
}

namespace nexus::scripting {

// ─────────────────────────────────────────────────────────────────────────────
// ScriptComponent — attached to entities for gameplay scripting
// ─────────────────────────────────────────────────────────────────────────────

struct ScriptComponent {
    /// Script identifier (for hot-reload and lookup).
    std::string script_name;

    /// Per-entity variables.
    std::shared_ptr<ScriptContext> context;

    /// Lifecycle callbacks.
    ScriptValue::FunctionType on_create;
    ScriptValue::FunctionType on_update;
    ScriptValue::FunctionType on_destroy;
    ScriptValue::FunctionType on_collision;

    /// Whether on_create has been called.
    bool initialized{false};

    /// Whether this script is enabled.
    bool enabled{true};
};

// ─────────────────────────────────────────────────────────────────────────────
// ScriptSystem — processes ScriptComponents each frame
// ─────────────────────────────────────────────────────────────────────────────

class ScriptEngine;

class ScriptSystem {
public:
    explicit ScriptSystem(ScriptEngine* engine) : engine_(engine) {}

    /// Call on_create for newly added scripts.
    void initialize_scripts(nexus::Registry& registry);

    /// Call on_update for all active scripts.
    void update_scripts(nexus::Registry& registry, float dt);

    /// Call on_destroy for scripts being removed.
    void destroy_scripts(nexus::Registry& registry);

    /// Notify collision between two entities.
    void on_collision(nexus::Registry& registry, u32 entity_a, u32 entity_b);

    /// Set up a ScriptComponent with callbacks from the registered script.
    void bind_script(ScriptComponent& component);

private:
    ScriptEngine* engine_{nullptr};
};

} // namespace nexus::scripting
