// End-to-end LuaBackend tests — drive scripts through the real Lua 5.4
// VM and verify that arithmetic, control flow, closures, metatables, and
// the C++ <-> Lua marshalling all work as expected.  These exist
// specifically to keep the engine honest about embedding the canonical
// Lua reference implementation rather than a hand-rolled approximation.

#include <gtest/gtest.h>
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/lua_backend.h"
#include "nexus/scripting/engine_bindings.h"
#include "nexus/scene/registry.h"

using namespace nexus;
using namespace nexus::scripting;

namespace {

class LuaBackendIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        bind_all(engine, registry);
        backend = &engine.lua_backend();
    }
    ScriptEngine engine;
    Registry     registry;
    LuaBackend*  backend{nullptr};
};

} // namespace

// ── Arithmetic / expressions (the original "i + j" failure) ────────────────

TEST_F(LuaBackendIntegration, ArithmeticInsideFunctionCall) {
    // The exact reproduction the user pasted:
    //   i = 9
    //   j = 10
    //   s = tostring(i + j)
    //   print(s)
    // Should yield "19", NOT nil (which is what the old hand-rolled
    // interpreter produced because it had no operator-precedence
    // expression evaluator).
    std::string captured;
    engine.set_print_sink([&](const std::string& m) { captured = m; });

    EXPECT_TRUE(backend->execute(
        "i = 9\n"
        "j = 10\n"
        "s = tostring(i + j)\n"
        "print(s)\n"));
    EXPECT_EQ(captured, "19");
}

TEST_F(LuaBackendIntegration, OperatorPrecedence) {
    // 2 + 3 * 4 = 14, not 20 — straight precedence climb.
    auto r = backend->evaluate("2 + 3 * 4");
    EXPECT_EQ(r.as_int(), 14);
    // Parens override.
    r = backend->evaluate("(2 + 3) * 4");
    EXPECT_EQ(r.as_int(), 20);
    // Unary minus.
    r = backend->evaluate("-3 + 5");
    EXPECT_EQ(r.as_int(), 2);
    // Power is right-associative: 2^3^2 = 2^(3^2) = 2^9 = 512.
    r = backend->evaluate("2^3^2");
    EXPECT_NEAR(r.as_float(), 512.0f, 1e-3f);
}

TEST_F(LuaBackendIntegration, ComparisonAndBoolean) {
    EXPECT_TRUE (backend->evaluate("1 < 2").as_bool());
    EXPECT_FALSE(backend->evaluate("2 < 1").as_bool());
    EXPECT_TRUE (backend->evaluate("1 == 1 and 2 ~= 3").as_bool());
    EXPECT_TRUE (backend->evaluate("nil or 'fallback' == 'fallback'").as_bool());
    // Lua's `not nil` is true.
    EXPECT_TRUE (backend->evaluate("not nil").as_bool());
}

TEST_F(LuaBackendIntegration, StringConcatenation) {
    EXPECT_EQ(backend->evaluate("'foo' .. 'bar'").as_string(), "foobar");
    EXPECT_EQ(backend->evaluate("'n=' .. tostring(42)").as_string(), "n=42");
}

// ── Control flow ───────────────────────────────────────────────────────────

TEST_F(LuaBackendIntegration, IfElse) {
    EXPECT_TRUE(backend->execute(
        "result = nil\n"
        "if 1 < 2 then result = 'less' else result = 'more' end\n"));
    EXPECT_EQ(backend->get_global("result").as_string(), "less");
}

TEST_F(LuaBackendIntegration, NumericForLoop) {
    EXPECT_TRUE(backend->execute(
        "sum = 0\n"
        "for i = 1, 10 do sum = sum + i end\n"));
    EXPECT_EQ(backend->get_global("sum").as_int(), 55);
}

TEST_F(LuaBackendIntegration, WhileLoop) {
    EXPECT_TRUE(backend->execute(
        "n, p = 1, 1\n"
        "while n < 6 do p = p * n; n = n + 1 end\n"));  // 5! = 120
    EXPECT_EQ(backend->get_global("p").as_int(), 120);
}

// ── Functions / closures ───────────────────────────────────────────────────

TEST_F(LuaBackendIntegration, FunctionDefinitionAndCall) {
    EXPECT_TRUE(backend->execute(
        "function add(a, b) return a + b end\n"
        "x = add(7, 8)\n"));
    EXPECT_EQ(backend->get_global("x").as_int(), 15);
}

TEST_F(LuaBackendIntegration, ClosureCapturesUpvalue) {
    EXPECT_TRUE(backend->execute(
        "function make_counter()\n"
        "  local n = 0\n"
        "  return function() n = n + 1; return n end\n"
        "end\n"
        "c = make_counter()\n"
        "a = c(); b = c(); d = c()\n"));
    EXPECT_EQ(backend->get_global("a").as_int(), 1);
    EXPECT_EQ(backend->get_global("b").as_int(), 2);
    EXPECT_EQ(backend->get_global("d").as_int(), 3);
}

// ── Tables ─────────────────────────────────────────────────────────────────

TEST_F(LuaBackendIntegration, TableCreateAndIndex) {
    EXPECT_TRUE(backend->execute(
        "t = {a = 1, b = 'two', c = {nested = true}}\n"
        "x = t.a\n"
        "y = t.b\n"
        "z = t.c.nested\n"));
    EXPECT_EQ(backend->get_global("x").as_int(),     1);
    EXPECT_EQ(backend->get_global("y").as_string(),  "two");
    EXPECT_TRUE(backend->get_global("z").as_bool());
}

TEST_F(LuaBackendIntegration, IpairsIteratesArrayKeys) {
    EXPECT_TRUE(backend->execute(
        "t = {10, 20, 30}\n"
        "sum = 0\n"
        "for _, v in ipairs(t) do sum = sum + v end\n"));
    EXPECT_EQ(backend->get_global("sum").as_int(), 60);
}

// ── Vec / Entity marshalling ───────────────────────────────────────────────

TEST_F(LuaBackendIntegration, Vec3RoundTrip) {
    backend->set_global("v", ScriptValue(Vec3(1.0f, 2.0f, 3.0f)));
    EXPECT_TRUE(backend->execute(
        "x = v.x\n"
        "y = v.y\n"
        "z = v.z\n"
        "v2 = {x = v.x * 2, y = v.y * 2, z = v.z * 2}\n"));
    EXPECT_FLOAT_EQ(backend->get_global("x").as_float(), 1.0f);
    EXPECT_FLOAT_EQ(backend->get_global("y").as_float(), 2.0f);
    EXPECT_FLOAT_EQ(backend->get_global("z").as_float(), 3.0f);
    auto v2 = backend->get_global("v2");
    ASSERT_TRUE(v2.is_vec3());
    EXPECT_FLOAT_EQ(v2.as_vec3().x, 2.0f);
    EXPECT_FLOAT_EQ(v2.as_vec3().y, 4.0f);
    EXPECT_FLOAT_EQ(v2.as_vec3().z, 6.0f);
}

TEST_F(LuaBackendIntegration, EntityRoundTrip) {
    backend->set_global("e", ScriptValue::entity(42));
    EXPECT_TRUE(backend->execute("id = e.id\n"));
    EXPECT_EQ(backend->get_global("id").as_int(), 42);
}

// ── Native function bridge ─────────────────────────────────────────────────

TEST_F(LuaBackendIntegration, NativeFunctionCallableFromLua) {
    // Register a host C++ lambda; verify Lua scripts can invoke it,
    // pass arguments, and consume its return value.
    engine.register_function("Game", "double",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.empty() || !args[0].is_int()) return ScriptValue::nil();
            return ScriptValue(args[0].as_int() * 2);
        }, 1, 1, "double an integer");

    EXPECT_TRUE(backend->execute("y = Game.double(21)\n"));
    EXPECT_EQ(backend->get_global("y").as_int(), 42);
}

TEST_F(LuaBackendIntegration, LuaErrorReportedThroughErrorHandler) {
    // A runtime error inside Lua should surface via
    // ScriptEngine::errors() so the editor's Console panel sees it
    // alongside engine logs.  The specific test: divide a string,
    // which Lua refuses with a typed runtime error.
    EXPECT_FALSE(backend->execute("x = 'a' / 2"));
    EXPECT_FALSE(backend->last_error().empty());
}

// ── Original symptomatic regression ────────────────────────────────────────

TEST_F(LuaBackendIntegration, UserReproducedScriptYields19AndHelloWorld) {
    // Exact transcript of the failure the user reported after round 2:
    //   i = 9
    //   j = 10
    //   s = tostring(i + j)
    //   print(s)
    //   print("hello world")
    // Expected: print fires twice with "19" and "hello world".
    std::vector<std::string> log;
    engine.set_print_sink([&](const std::string& m) { log.push_back(m); });

    EXPECT_TRUE(backend->execute(
        "i = 9\n"
        "j = 10\n"
        "s = tostring(i + j)\n"
        "print(s)\n"
        "print('hello world')\n"));

    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0], "19");
    EXPECT_EQ(log[1], "hello world");
}
