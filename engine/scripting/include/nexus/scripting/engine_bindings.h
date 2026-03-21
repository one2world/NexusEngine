#pragma once

#include "nexus/scripting/script_engine.h"

namespace nexus {
    class Registry;
    class Input;
}
namespace nexus::audio { class AudioEngine; }
namespace nexus::physics { class PhysicsSystem; }

namespace nexus::scripting {

// ─────────────────────────────────────────────────────────────────────────────
// Engine API bindings — register all engine subsystems into the ScriptEngine
// ─────────────────────────────────────────────────────────────────────────────

/// Register Entity/ECS bindings: create, destroy, add/get/has component, etc.
void bind_entity_api(ScriptEngine& engine, nexus::Registry& registry);

/// Register math utility functions.
void bind_math_api(ScriptEngine& engine);

/// Register input query functions (keyboard, mouse, gamepad).
/// The no-arg overload registers stubs; the Input* overload queries real state.
void bind_input_api(ScriptEngine& engine);
void bind_input_api(ScriptEngine& engine, nexus::Input& input);

/// Register audio functions (play, stop, volume, etc.).
/// The no-arg overload registers stubs; the AudioEngine* overload delegates.
void bind_audio_api(ScriptEngine& engine);
void bind_audio_api(ScriptEngine& engine, nexus::audio::AudioEngine& audio);

/// Register physics query functions (raycast, overlap).
/// The no-arg overload registers stubs; the PhysicsSystem* overload delegates.
void bind_physics_api(ScriptEngine& engine);
void bind_physics_api(ScriptEngine& engine, nexus::physics::PhysicsSystem& physics);

/// Register all engine bindings at once (stub variants for Input/Audio/Physics).
void bind_all(ScriptEngine& engine, nexus::Registry& registry);

/// Register all engine bindings with live subsystem pointers.
void bind_all(ScriptEngine& engine, nexus::Registry& registry,
              nexus::Input& input,
              nexus::audio::AudioEngine& audio,
              nexus::physics::PhysicsSystem& physics);

} // namespace nexus::scripting
