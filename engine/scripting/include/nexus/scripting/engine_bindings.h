#pragma once

#include "nexus/scripting/script_engine.h"

namespace nexus {
    class Registry;
}

namespace nexus::scripting {

// ─────────────────────────────────────────────────────────────────────────────
// Engine API bindings — register all engine subsystems into the ScriptEngine
// ─────────────────────────────────────────────────────────────────────────────

/// Register Entity/ECS bindings: create, destroy, add/get/has component, etc.
void bind_entity_api(ScriptEngine& engine, nexus::Registry& registry);

/// Register math utility functions.
void bind_math_api(ScriptEngine& engine);

/// Register input query functions (keyboard, mouse, gamepad).
void bind_input_api(ScriptEngine& engine);

/// Register audio functions (play, stop, volume, etc.).
void bind_audio_api(ScriptEngine& engine);

/// Register physics query functions (raycast, overlap).
void bind_physics_api(ScriptEngine& engine);

/// Register all engine bindings at once.
void bind_all(ScriptEngine& engine, nexus::Registry& registry);

} // namespace nexus::scripting
