#include <gtest/gtest.h>
#include "nexus/scripting/engine_bindings.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"

using namespace nexus;
using namespace nexus::scripting;

// =============================================================================
// Entity API Tests
// =============================================================================

TEST(EntityBindings, CreateAndDestroy) {
    ScriptEngine engine;
    Registry registry;
    bind_entity_api(engine, registry);

    auto result = engine.call_function("Entity.create");
    EXPECT_TRUE(result.is_entity());
    u32 e = result.as_entity();
    EXPECT_TRUE(registry.alive(e));

    engine.call_function("Entity.destroy", {result});
    EXPECT_FALSE(registry.alive(e));
}

TEST(EntityBindings, Alive) {
    ScriptEngine engine;
    Registry registry;
    bind_entity_api(engine, registry);

    auto e = engine.call_function("Entity.create");
    auto alive = engine.call_function("Entity.alive", {e});
    EXPECT_TRUE(alive.as_bool());

    engine.call_function("Entity.destroy", {e});
    alive = engine.call_function("Entity.alive", {e});
    EXPECT_FALSE(alive.as_bool());
}

TEST(EntityBindings, SetGetName) {
    ScriptEngine engine;
    Registry registry;
    bind_entity_api(engine, registry);

    auto e = engine.call_function("Entity.create");
    engine.call_function("Entity.set_name", {e, ScriptValue("Player")});

    auto name = engine.call_function("Entity.get_name", {e});
    EXPECT_EQ(name.as_string(), "Player");
}

TEST(EntityBindings, SetGetPosition) {
    ScriptEngine engine;
    Registry registry;
    bind_entity_api(engine, registry);

    auto e_val = engine.call_function("Entity.create");
    u32 e = e_val.as_entity();

    // Add transform component so position works
    registry.add_component<Transform3DComponent>(e, {});

    engine.call_function("Entity.set_position",
        {e_val, ScriptValue(1.0f), ScriptValue(2.0f), ScriptValue(3.0f)});

    auto pos = engine.call_function("Entity.get_position", {e_val});
    EXPECT_TRUE(pos.is_vec3());
    auto v = pos.as_vec3();
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(EntityBindings, Count) {
    ScriptEngine engine;
    Registry registry;
    bind_entity_api(engine, registry);

    engine.call_function("Entity.create");
    engine.call_function("Entity.create");

    auto count = engine.call_function("Entity.count");
    EXPECT_EQ(count.as_int(), 2);
}

// =============================================================================
// Math API Tests
// =============================================================================

TEST(MathBindings, Sin) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.sin", {ScriptValue(0.0f)});
    EXPECT_NEAR(r.as_float(), 0.0f, 0.001f);
}

TEST(MathBindings, Cos) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.cos", {ScriptValue(0.0f)});
    EXPECT_NEAR(r.as_float(), 1.0f, 0.001f);
}

TEST(MathBindings, Sqrt) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.sqrt", {ScriptValue(9.0f)});
    EXPECT_NEAR(r.as_float(), 3.0f, 0.001f);
}

TEST(MathBindings, Abs) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.abs", {ScriptValue(-5.0f)});
    EXPECT_NEAR(r.as_float(), 5.0f, 0.001f);
}

TEST(MathBindings, MinMax) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto mn = engine.call_function("Math.min", {ScriptValue(3.0f), ScriptValue(7.0f)});
    EXPECT_NEAR(mn.as_float(), 3.0f, 0.001f);

    auto mx = engine.call_function("Math.max", {ScriptValue(3.0f), ScriptValue(7.0f)});
    EXPECT_NEAR(mx.as_float(), 7.0f, 0.001f);
}

TEST(MathBindings, Clamp) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.clamp",
        {ScriptValue(15.0f), ScriptValue(0.0f), ScriptValue(10.0f)});
    EXPECT_NEAR(r.as_float(), 10.0f, 0.001f);

    r = engine.call_function("Math.clamp",
        {ScriptValue(-5.0f), ScriptValue(0.0f), ScriptValue(10.0f)});
    EXPECT_NEAR(r.as_float(), 0.0f, 0.001f);
}

TEST(MathBindings, Lerp) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.lerp",
        {ScriptValue(0.0f), ScriptValue(10.0f), ScriptValue(0.5f)});
    EXPECT_NEAR(r.as_float(), 5.0f, 0.001f);
}

TEST(MathBindings, Distance) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.distance",
        {ScriptValue(Vec3(0, 0, 0)), ScriptValue(Vec3(3, 4, 0))});
    EXPECT_NEAR(r.as_float(), 5.0f, 0.001f);
}

TEST(MathBindings, Normalize) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.normalize",
        {ScriptValue(Vec3(3, 0, 0))});
    EXPECT_TRUE(r.is_vec3());
    EXPECT_NEAR(r.as_vec3().x, 1.0f, 0.001f);
}

TEST(MathBindings, Vec2Create) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.vec2",
        {ScriptValue(1.0f), ScriptValue(2.0f)});
    EXPECT_TRUE(r.is_vec2());
    EXPECT_FLOAT_EQ(r.as_vec2().x, 1.0f);
    EXPECT_FLOAT_EQ(r.as_vec2().y, 2.0f);
}

TEST(MathBindings, Vec3Create) {
    ScriptEngine engine;
    bind_math_api(engine);

    auto r = engine.call_function("Math.vec3",
        {ScriptValue(1.0f), ScriptValue(2.0f), ScriptValue(3.0f)});
    EXPECT_TRUE(r.is_vec3());
    EXPECT_FLOAT_EQ(r.as_vec3().z, 3.0f);
}

TEST(MathBindings, Constants) {
    ScriptEngine engine;
    bind_math_api(engine);

    EXPECT_NEAR(engine.get_global("Math.PI").as_float(), 3.14159f, 0.001f);
    EXPECT_NEAR(engine.get_global("Math.DEG2RAD").as_float(), 0.01745f, 0.001f);
}

// =============================================================================
// Input API Tests (stubs)
// =============================================================================

TEST(InputBindings, IsKeyDown) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.is_key_down", {ScriptValue(32)});
    EXPECT_FALSE(r.as_bool());
}

TEST(InputBindings, GetMousePosition) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.get_mouse_position");
    EXPECT_TRUE(r.is_vec2());
}

// =============================================================================
// Audio API Tests (stubs)
// =============================================================================

TEST(AudioBindings, Play) {
    ScriptEngine engine;
    bind_audio_api(engine);

    auto r = engine.call_function("Audio.play", {ScriptValue("sound.wav")});
    EXPECT_TRUE(r.is_int());
}

// =============================================================================
// Physics API Tests (stubs)
// =============================================================================

TEST(PhysicsBindings, Raycast) {
    ScriptEngine engine;
    bind_physics_api(engine);

    auto r = engine.call_function("Physics.raycast",
        {ScriptValue(Vec3(0, 0, 0)), ScriptValue(Vec3(0, -1, 0))});
    // Stub returns nil
    EXPECT_TRUE(r.is_nil());
}

// =============================================================================
// bind_all Tests
// =============================================================================

TEST(BindAll, RegistersAllModules) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    // Check that functions from all modules exist
    EXPECT_NE(engine.find_function("Entity", "create"), nullptr);
    EXPECT_NE(engine.find_function("Math", "sin"), nullptr);
    EXPECT_NE(engine.find_function("Input", "is_key_down"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "play"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "raycast"), nullptr);
    EXPECT_NE(engine.find_function("", "print"), nullptr);
    EXPECT_NE(engine.find_function("", "type"), nullptr);
}

TEST(BindAll, PrintFunction) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    // Should not crash
    engine.call_function("print", {ScriptValue("hello"), ScriptValue(42)});
}

TEST(BindAll, TypeFunction) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    EXPECT_EQ(engine.call_function("type", {ScriptValue(42)}).as_string(), "int");
    EXPECT_EQ(engine.call_function("type", {ScriptValue("hi")}).as_string(), "string");
    EXPECT_EQ(engine.call_function("type", {ScriptValue(true)}).as_string(), "bool");
    EXPECT_EQ(engine.call_function("type", {ScriptValue(1.0f)}).as_string(), "float");
    EXPECT_EQ(engine.call_function("type", {ScriptValue::nil()}).as_string(), "nil");
    EXPECT_EQ(engine.call_function("type", {ScriptValue(Vec3(0))}).as_string(), "vec3");
    EXPECT_EQ(engine.call_function("type", {ScriptValue::entity(1)}).as_string(), "entity");
    EXPECT_EQ(engine.call_function("type", {ScriptValue::table()}).as_string(), "table");
}
