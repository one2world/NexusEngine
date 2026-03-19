#include "nexus/scripting/engine_bindings.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"
#include "nexus/core/log.h"
#include <cmath>

namespace nexus::scripting {

// ── Entity / ECS Bindings ───────────────────────────────────────────────────

void bind_entity_api(ScriptEngine& engine, Registry& registry) {
    // Entity.create() -> entity
    engine.register_function("Entity", "create",
        [&registry](const std::vector<ScriptValue>&) -> ScriptValue {
            auto e = registry.create();
            return ScriptValue::entity(e);
        }, 0, 0, "Create a new entity");

    // Entity.destroy(entity)
    engine.register_function("Entity", "destroy",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args.empty() && args[0].is_entity()) {
                registry.destroy(args[0].as_entity());
            }
            return ScriptValue::nil();
        }, 1, 1, "Destroy an entity");

    // Entity.alive(entity) -> bool
    engine.register_function("Entity", "alive",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args.empty() && args[0].is_entity()) {
                return ScriptValue(registry.alive(args[0].as_entity()));
            }
            return ScriptValue(false);
        }, 1, 1, "Check if entity is alive");

    // Entity.set_name(entity, name)
    engine.register_function("Entity", "set_name",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2 && args[0].is_entity() && args[1].is_string()) {
                u32 e = args[0].as_entity();
                if (registry.alive(e)) {
                    if (!registry.has_component<TagComponent>(e)) {
                        registry.add_component<TagComponent>(e, {args[1].as_string()});
                    } else {
                        registry.get_component<TagComponent>(e).name = args[1].as_string();
                    }
                }
            }
            return ScriptValue::nil();
        }, 2, 2, "Set entity name");

    // Entity.get_name(entity) -> string
    engine.register_function("Entity", "get_name",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args.empty() && args[0].is_entity()) {
                u32 e = args[0].as_entity();
                if (registry.alive(e) && registry.has_component<TagComponent>(e)) {
                    return ScriptValue(registry.get_component<TagComponent>(e).name);
                }
            }
            return ScriptValue("");
        }, 1, 1, "Get entity name");

    // Entity.set_position(entity, x, y, z)
    engine.register_function("Entity", "set_position",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 4 && args[0].is_entity()) {
                u32 e = args[0].as_entity();
                if (registry.alive(e) && registry.has_component<Transform3DComponent>(e)) {
                    auto& t = registry.get_component<Transform3DComponent>(e);
                    t.position = Vec3(args[1].as_float(), args[2].as_float(), args[3].as_float());
                }
            }
            return ScriptValue::nil();
        }, 4, 4, "Set entity 3D position");

    // Entity.get_position(entity) -> vec3
    engine.register_function("Entity", "get_position",
        [&registry](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args.empty() && args[0].is_entity()) {
                u32 e = args[0].as_entity();
                if (registry.alive(e) && registry.has_component<Transform3DComponent>(e)) {
                    return ScriptValue(registry.get_component<Transform3DComponent>(e).position);
                }
            }
            return ScriptValue(Vec3(0.0f));
        }, 1, 1, "Get entity 3D position");

    // Entity.count() -> int
    engine.register_function("Entity", "count",
        [&registry](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(static_cast<i32>(registry.size()));
        }, 0, 0, "Get total entity count");
}

// ── Math Bindings ───────────────────────────────────────────────────────────

void bind_math_api(ScriptEngine& engine) {
    engine.register_function("Math", "sin",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::sin(args[0].as_float()));
        }, 1, 1, "Sine function");

    engine.register_function("Math", "cos",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::cos(args[0].as_float()));
        }, 1, 1, "Cosine function");

    engine.register_function("Math", "sqrt",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::sqrt(args[0].as_float()));
        }, 1, 1, "Square root");

    engine.register_function("Math", "abs",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::abs(args[0].as_float()));
        }, 1, 1, "Absolute value");

    engine.register_function("Math", "min",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::min(args[0].as_float(), args[1].as_float()));
        }, 2, 2, "Minimum of two values");

    engine.register_function("Math", "max",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(std::max(args[0].as_float(), args[1].as_float()));
        }, 2, 2, "Maximum of two values");

    engine.register_function("Math", "clamp",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            float v = args[0].as_float();
            float lo = args[1].as_float();
            float hi = args[2].as_float();
            return ScriptValue(std::max(lo, std::min(v, hi)));
        }, 3, 3, "Clamp value between min and max");

    engine.register_function("Math", "lerp",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            float a = args[0].as_float();
            float b = args[1].as_float();
            float t = args[2].as_float();
            return ScriptValue(a + (b - a) * t);
        }, 3, 3, "Linear interpolation");

    engine.register_function("Math", "distance",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args[0].is_vec3() && args[1].is_vec3()) {
                return ScriptValue(glm::distance(args[0].as_vec3(), args[1].as_vec3()));
            }
            if (args[0].is_vec2() && args[1].is_vec2()) {
                return ScriptValue(glm::distance(args[0].as_vec2(), args[1].as_vec2()));
            }
            return ScriptValue(0.0f);
        }, 2, 2, "Distance between two vectors");

    engine.register_function("Math", "normalize",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args[0].is_vec3()) {
                return ScriptValue(glm::normalize(args[0].as_vec3()));
            }
            if (args[0].is_vec2()) {
                return ScriptValue(glm::normalize(args[0].as_vec2()));
            }
            return ScriptValue::nil();
        }, 1, 1, "Normalize a vector");

    engine.register_function("Math", "vec2",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(Vec2(args[0].as_float(), args[1].as_float()));
        }, 2, 2, "Create a Vec2");

    engine.register_function("Math", "vec3",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(Vec3(args[0].as_float(), args[1].as_float(), args[2].as_float()));
        }, 3, 3, "Create a Vec3");

    // Constants
    engine.set_global("Math.PI", ScriptValue(math::PI));
    engine.set_global("Math.TWO_PI", ScriptValue(math::TWO_PI));
    engine.set_global("Math.DEG2RAD", ScriptValue(math::DEG2RAD));
    engine.set_global("Math.RAD2DEG", ScriptValue(math::RAD2DEG));
}

// ── Input Bindings (stubs — require platform integration) ───────────────────

void bind_input_api(ScriptEngine& engine) {
    // These are stubs that return default values.
    // In a real integration, they'd query the platform input system.

    engine.register_function("Input", "is_key_down",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(false);
        }, 1, 1, "Check if a key is currently held down");

    engine.register_function("Input", "is_key_pressed",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(false);
        }, 1, 1, "Check if a key was just pressed this frame");

    engine.register_function("Input", "get_mouse_position",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(Vec2(0.0f));
        }, 0, 0, "Get current mouse position");

    engine.register_function("Input", "is_mouse_button_down",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(false);
        }, 1, 1, "Check if a mouse button is held down");

    engine.register_function("Input", "get_axis",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(0.0f);
        }, 1, 1, "Get a named input axis value (-1 to 1)");
}

// ── Audio Bindings (stubs) ──────────────────────────────────────────────────

void bind_audio_api(ScriptEngine& engine) {
    engine.register_function("Audio", "play",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(static_cast<i32>(-1)); // voice handle
        }, 1, 3, "Play a sound (name, [volume], [loop])");

    engine.register_function("Audio", "stop",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil();
        }, 1, 1, "Stop a playing sound by handle");

    engine.register_function("Audio", "set_volume",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil();
        }, 2, 2, "Set volume of a voice (handle, volume)");

    engine.register_function("Audio", "play_event",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue(static_cast<i32>(-1));
        }, 1, 1, "Trigger a named audio event");
}

// ── Physics Bindings (stubs) ────────────────────────────────────────────────

void bind_physics_api(ScriptEngine& engine) {
    engine.register_function("Physics", "raycast",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil(); // would return hit info table
        }, 2, 3, "Cast a ray (origin_vec3, direction_vec3, [max_distance])");

    engine.register_function("Physics", "overlap_sphere",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil(); // would return table of entities
        }, 2, 2, "Find entities overlapping a sphere (center_vec3, radius)");

    engine.register_function("Physics", "set_velocity",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil();
        }, 2, 2, "Set velocity of a physics body (entity, velocity_vec3)");

    engine.register_function("Physics", "apply_force",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil();
        }, 2, 2, "Apply force to a physics body (entity, force_vec3)");
}

// ── Bind All ────────────────────────────────────────────────────────────────

void bind_all(ScriptEngine& engine, Registry& registry) {
    bind_entity_api(engine, registry);
    bind_math_api(engine);
    bind_input_api(engine);
    bind_audio_api(engine);
    bind_physics_api(engine);

    // Utility: print
    engine.register_function("", "print",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            std::string msg;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) msg += " ";
                msg += args[i].to_string();
            }
            NX_INFO("[Script] {}", msg);
            return ScriptValue::nil();
        }, 0, 255, "Print values to the console");

    // Utility: type(value) -> string
    engine.register_function("", "type",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.empty()) return ScriptValue("nil");
            switch (args[0].type()) {
                case ScriptValue::Type::Nil:      return ScriptValue("nil");
                case ScriptValue::Type::Bool:     return ScriptValue("bool");
                case ScriptValue::Type::Int:      return ScriptValue("int");
                case ScriptValue::Type::Float:    return ScriptValue("float");
                case ScriptValue::Type::String:   return ScriptValue("string");
                case ScriptValue::Type::Vec2:     return ScriptValue("vec2");
                case ScriptValue::Type::Vec3:     return ScriptValue("vec3");
                case ScriptValue::Type::Vec4:     return ScriptValue("vec4");
                case ScriptValue::Type::Entity:   return ScriptValue("entity");
                case ScriptValue::Type::Function: return ScriptValue("function");
                case ScriptValue::Type::Table:    return ScriptValue("table");
            }
            return ScriptValue("unknown");
        }, 1, 1, "Get the type name of a value");
}

} // namespace nexus::scripting
