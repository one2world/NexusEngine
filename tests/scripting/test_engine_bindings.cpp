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

TEST(InputBindings, IsKeyReleased) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.is_key_released", {ScriptValue(32)});
    EXPECT_FALSE(r.as_bool());
}

TEST(InputBindings, GetMousePosition) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.get_mouse_position");
    EXPECT_TRUE(r.is_vec2());
}

TEST(InputBindings, GetMouseDelta) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.get_mouse_delta");
    EXPECT_TRUE(r.is_vec2());
}

TEST(InputBindings, IsMouseButtonPressed) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.is_mouse_button_pressed", {ScriptValue(0)});
    EXPECT_FALSE(r.as_bool());
}

TEST(InputBindings, GetScrollDelta) {
    ScriptEngine engine;
    bind_input_api(engine);

    auto r = engine.call_function("Input.get_scroll_delta");
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

TEST(AudioBindings, StubAPIParity) {
    ScriptEngine engine;
    bind_audio_api(engine);

    // All functions that exist in the live API must also exist in stubs
    EXPECT_NE(engine.find_function("Audio", "play"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "stop"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "pause"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "resume"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "set_volume"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "set_pitch"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "is_playing"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "play_event"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "set_master_volume"), nullptr);
    EXPECT_NE(engine.find_function("Audio", "stop_all"), nullptr);
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

TEST(PhysicsBindings, StubAPIParity) {
    ScriptEngine engine;
    bind_physics_api(engine);

    // All functions that exist in the live API must also exist in stubs
    EXPECT_NE(engine.find_function("Physics", "raycast"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "raycast_2d"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "overlap_sphere"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "set_velocity"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "apply_force"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "set_gravity"), nullptr);
    EXPECT_NE(engine.find_function("Physics", "set_gravity_2d"), nullptr);
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

// ── M32: print() routing through ScriptEngine::set_print_sink ─────────────

TEST(BindAll, PrintSinkReceivesJoinedMessage) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    std::string captured;
    engine.set_print_sink([&captured](const std::string& msg) {
        captured = msg;
    });

    engine.call_function("print",
        {ScriptValue("hello"), ScriptValue(42), ScriptValue(true)});

    EXPECT_EQ(captured, "hello 42 true");
}

TEST(BindAll, PrintSinkInvokedOncePerCall) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    int call_count = 0;
    engine.set_print_sink([&call_count](const std::string&) { ++call_count; });

    engine.call_function("print", {ScriptValue("a")});
    engine.call_function("print", {ScriptValue("b")});
    engine.call_function("print", {});  // empty args still fires

    EXPECT_EQ(call_count, 3);
}

TEST(BindAll, PrintSinkAccumulatesEachCall) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    std::vector<std::string> log;
    engine.set_print_sink([&log](const std::string& m) { log.push_back(m); });

    engine.call_function("print", {ScriptValue("first")});
    engine.call_function("print", {ScriptValue("second"), ScriptValue("line")});

    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0], "first");
    EXPECT_EQ(log[1], "second line");
}

TEST(BindAll, PrintSinkUnsetFallsBackSilently) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    EXPECT_FALSE(engine.has_print_sink());
    // Without a sink the binding still resolves (NX_INFO fallback).
    auto result = engine.call_function("print", {ScriptValue("ignored")});
    EXPECT_TRUE(result.is_nil());
}

TEST(BindAll, PrintSinkReplaceableLatestWins) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    std::string first;
    engine.set_print_sink([&first](const std::string& m) { first = m; });

    std::string second;
    engine.set_print_sink([&second](const std::string& m) { second = m; });

    engine.call_function("print", {ScriptValue("hello")});

    EXPECT_TRUE(first.empty());
    EXPECT_EQ(second, "hello");
}

TEST(BindAll, PrintSinkClearedByEmptyAssignment) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    int count = 0;
    engine.set_print_sink([&count](const std::string&) { ++count; });
    engine.call_function("print", {ScriptValue("x")});
    EXPECT_EQ(count, 1);

    engine.set_print_sink({});
    EXPECT_FALSE(engine.has_print_sink());
    engine.call_function("print", {ScriptValue("y")});
    EXPECT_EQ(count, 1);  // sink no longer called
}

TEST(BindAll, PrintSinkSeparatesArgsWithSingleSpace) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    std::string captured;
    engine.set_print_sink([&captured](const std::string& m) { captured = m; });

    engine.call_function("print",
        {ScriptValue(1), ScriptValue(2), ScriptValue(3)});

    EXPECT_EQ(captured, "1 2 3");
}

TEST(BindAll, TypeFunctionMatchesLuaSpec) {
    // `type()` returns canonical Lua type names: int and float collapse
    // to "number"; Vec/Entity/Function are exposed as "userdata" /
    // "function" so user scripts can be written against the standard
    // Lua reference.  Engine-specific subtype information lives on
    // dedicated helpers (Math.is_vec3, Entity.alive, ...).
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    EXPECT_EQ(engine.call_function("type", {ScriptValue(42)}).as_string(),       "number");
    EXPECT_EQ(engine.call_function("type", {ScriptValue("hi")}).as_string(),     "string");
    EXPECT_EQ(engine.call_function("type", {ScriptValue(true)}).as_string(),     "boolean");
    EXPECT_EQ(engine.call_function("type", {ScriptValue(1.0f)}).as_string(),     "number");
    EXPECT_EQ(engine.call_function("type", {ScriptValue::nil()}).as_string(),    "nil");
    EXPECT_EQ(engine.call_function("type", {ScriptValue(Vec3(0))}).as_string(),  "userdata");
    EXPECT_EQ(engine.call_function("type", {ScriptValue::entity(1)}).as_string(),"userdata");
    EXPECT_EQ(engine.call_function("type", {ScriptValue::table()}).as_string(),  "table");
}

TEST(BindAll, ToStringHandlesAllTypes) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    EXPECT_EQ(engine.call_function("tostring",
        {ScriptValue::nil()}).as_string(), "nil");
    EXPECT_EQ(engine.call_function("tostring",
        {ScriptValue(true)}).as_string(), "true");
    EXPECT_EQ(engine.call_function("tostring",
        {ScriptValue(false)}).as_string(), "false");
    EXPECT_EQ(engine.call_function("tostring",
        {ScriptValue(42)}).as_string(), "42");
    EXPECT_EQ(engine.call_function("tostring",
        {ScriptValue("text")}).as_string(), "text");
}

TEST(BindAll, ToNumberRoundTripsIntegers) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("tonumber", {ScriptValue("42")});
    EXPECT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 42);

    // Negative integers
    r = engine.call_function("tonumber", {ScriptValue("-7")});
    EXPECT_EQ(r.as_int(), -7);
}

TEST(BindAll, ToNumberRoundTripsFloats) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("tonumber", {ScriptValue("3.14")});
    EXPECT_TRUE(r.is_float());
    EXPECT_NEAR(r.as_float(), 3.14f, 1e-5f);
}

TEST(BindAll, ToNumberHonoursBase) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("tonumber",
        {ScriptValue("ff"), ScriptValue(16)});
    EXPECT_EQ(r.as_int(), 255);

    r = engine.call_function("tonumber",
        {ScriptValue("1010"), ScriptValue(2)});
    EXPECT_EQ(r.as_int(), 10);
}

TEST(BindAll, ToNumberReturnsNilOnGarbage) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("tonumber", {ScriptValue("not a number")});
    EXPECT_TRUE(r.is_nil());

    r = engine.call_function("tonumber", {ScriptValue("3.14abc")});
    EXPECT_TRUE(r.is_nil());

    r = engine.call_function("tonumber", {ScriptValue::nil()});
    EXPECT_TRUE(r.is_nil());
}

TEST(BindAll, AssertReturnsArgWhenTruthy) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("assert", {ScriptValue(42)});
    EXPECT_EQ(r.as_int(), 42);
}

TEST(BindAll, AssertThrowsWhenFalsy) {
    // The runtime catches the throw and reports it as a script error
    // (see ScriptEngine::call_function).  Result value is nil and the
    // engine's error log gains the message.
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);
    engine.clear_errors();

    auto r = engine.call_function("assert",
        {ScriptValue(false), ScriptValue("custom msg")});
    EXPECT_TRUE(r.is_nil());
    ASSERT_GE(engine.errors().size(), 1u);
    EXPECT_NE(engine.errors().back().message.find("custom msg"),
              std::string::npos);
}

TEST(BindAll, SelectCountReturnsArgN) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("select",
        {ScriptValue("#"), ScriptValue(1), ScriptValue(2), ScriptValue(3)});
    EXPECT_EQ(r.as_int(), 3);
}

TEST(BindAll, SelectIndexReturnsArgAtPosition) {
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    auto r = engine.call_function("select",
        {ScriptValue(2), ScriptValue("a"), ScriptValue("b"), ScriptValue("c")});
    EXPECT_EQ(r.as_string(), "b");
}

TEST(BindAll, RegistersFullStdlib) {
    // Spot-check that bind_all wires the basic library AND the math/
    // string/table/os modules.  If any of these is missing the editor's
    // Lua Console reports "Function not found" the moment a user types
    // a routine line.
    ScriptEngine engine;
    Registry registry;
    bind_all(engine, registry);

    const char* basic_globals[] = {
        "print", "tostring", "tonumber", "type", "assert", "error",
        "select", "ipairs", "pairs", "rawequal", "rawget", "rawset", "unpack",
    };
    for (const char* name : basic_globals) {
        EXPECT_NE(engine.find_function("", name), nullptr)
            << "missing basic global: " << name;
    }
    EXPECT_NE(engine.find_function("math",   "abs"),     nullptr);
    EXPECT_NE(engine.find_function("string", "len"),     nullptr);
    EXPECT_NE(engine.find_function("table",  "insert"),  nullptr);
    EXPECT_NE(engine.find_function("table",  "unpack"),  nullptr);
    EXPECT_NE(engine.find_function("os",     "clock"),   nullptr);
}
