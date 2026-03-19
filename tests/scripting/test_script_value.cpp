#include <gtest/gtest.h>
#include "nexus/scripting/script_value.h"

using namespace nexus;
using namespace nexus::scripting;

// =============================================================================
// ScriptValue Type Tests
// =============================================================================

TEST(ScriptValue, DefaultIsNil) {
    ScriptValue v;
    EXPECT_TRUE(v.is_nil());
    EXPECT_EQ(v.type(), ScriptValue::Type::Nil);
}

TEST(ScriptValue, NilFactory) {
    auto v = ScriptValue::nil();
    EXPECT_TRUE(v.is_nil());
}

TEST(ScriptValue, BoolValue) {
    ScriptValue t(true);
    EXPECT_TRUE(t.is_bool());
    EXPECT_TRUE(t.as_bool());

    ScriptValue f(false);
    EXPECT_FALSE(f.as_bool());
}

TEST(ScriptValue, IntValue) {
    ScriptValue v(42);
    EXPECT_TRUE(v.is_int());
    EXPECT_TRUE(v.is_number());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(ScriptValue, FloatValue) {
    ScriptValue v(3.14f);
    EXPECT_TRUE(v.is_float());
    EXPECT_TRUE(v.is_number());
    EXPECT_NEAR(v.as_float(), 3.14f, 0.001f);
}

TEST(ScriptValue, IntAsFloat) {
    ScriptValue v(10);
    EXPECT_NEAR(v.as_float(), 10.0f, 0.001f);
}

TEST(ScriptValue, StringValue) {
    ScriptValue v("hello");
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string(), "hello");
}

TEST(ScriptValue, StdString) {
    std::string s = "world";
    ScriptValue v(s);
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string(), "world");
}

TEST(ScriptValue, Vec2Value) {
    ScriptValue v(Vec2(1.0f, 2.0f));
    EXPECT_TRUE(v.is_vec2());
    auto vec = v.as_vec2();
    EXPECT_FLOAT_EQ(vec.x, 1.0f);
    EXPECT_FLOAT_EQ(vec.y, 2.0f);
}

TEST(ScriptValue, Vec3Value) {
    ScriptValue v(Vec3(1.0f, 2.0f, 3.0f));
    EXPECT_TRUE(v.is_vec3());
    auto vec = v.as_vec3();
    EXPECT_FLOAT_EQ(vec.x, 1.0f);
    EXPECT_FLOAT_EQ(vec.z, 3.0f);
}

TEST(ScriptValue, Vec4Value) {
    ScriptValue v(Vec4(1.0f, 2.0f, 3.0f, 4.0f));
    EXPECT_TRUE(v.is_vec4());
    EXPECT_FLOAT_EQ(v.as_vec4().w, 4.0f);
}

TEST(ScriptValue, EntityValue) {
    auto v = ScriptValue::entity(42);
    EXPECT_TRUE(v.is_entity());
    EXPECT_EQ(v.as_entity(), 42u);
}

TEST(ScriptValue, FunctionValue) {
    auto fn = [](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue(99);
    };
    ScriptValue v{ScriptValue::FunctionType{fn}};
    EXPECT_TRUE(v.is_function());
}

// =============================================================================
// Truthiness
// =============================================================================

TEST(ScriptValue, TruthyNilIsFalse) {
    EXPECT_FALSE(ScriptValue::nil().truthy());
}

TEST(ScriptValue, TruthyFalseIsFalse) {
    EXPECT_FALSE(ScriptValue(false).truthy());
}

TEST(ScriptValue, TruthyTrueIsTrue) {
    EXPECT_TRUE(ScriptValue(true).truthy());
}

TEST(ScriptValue, TruthyNumberIsTrue) {
    EXPECT_TRUE(ScriptValue(0).truthy());  // unlike Lua, 0 is truthy
    EXPECT_TRUE(ScriptValue(42).truthy());
}

TEST(ScriptValue, TruthyStringIsTrue) {
    EXPECT_TRUE(ScriptValue("").truthy());
    EXPECT_TRUE(ScriptValue("hello").truthy());
}

// =============================================================================
// Function Calls
// =============================================================================

TEST(ScriptValue, CallFunction) {
    ScriptValue fn(ScriptValue::FunctionType(
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.empty()) return ScriptValue(0);
            return ScriptValue(args[0].as_int() * 2);
        }
    ));

    auto result = fn.call({ScriptValue(21)});
    EXPECT_EQ(result.as_int(), 42);
}

TEST(ScriptValue, CallNonFunctionReturnsNil) {
    ScriptValue v(42);
    auto result = v.call();
    EXPECT_TRUE(result.is_nil());
}

// =============================================================================
// Table
// =============================================================================

TEST(ScriptValue, TableCreate) {
    auto t = ScriptValue::table();
    EXPECT_TRUE(t.is_table());
}

TEST(ScriptValue, TableGetSet) {
    auto t = ScriptValue::table();
    t.set_field("name", ScriptValue("Alice"));
    t.set_field("age", ScriptValue(30));

    EXPECT_EQ(t.get_field("name").as_string(), "Alice");
    EXPECT_EQ(t.get_field("age").as_int(), 30);
}

TEST(ScriptValue, TableMissingFieldIsNil) {
    auto t = ScriptValue::table();
    EXPECT_TRUE(t.get_field("missing").is_nil());
}

TEST(ScriptValue, NonTableFieldIsNil) {
    ScriptValue v(42);
    EXPECT_TRUE(v.get_field("x").is_nil());
}

// =============================================================================
// Equality
// =============================================================================

TEST(ScriptValue, EqualityNil) {
    EXPECT_EQ(ScriptValue::nil(), ScriptValue::nil());
}

TEST(ScriptValue, EqualityBool) {
    EXPECT_EQ(ScriptValue(true), ScriptValue(true));
    EXPECT_NE(ScriptValue(true), ScriptValue(false));
}

TEST(ScriptValue, EqualityInt) {
    EXPECT_EQ(ScriptValue(42), ScriptValue(42));
    EXPECT_NE(ScriptValue(42), ScriptValue(43));
}

TEST(ScriptValue, EqualityFloat) {
    EXPECT_EQ(ScriptValue(1.5f), ScriptValue(1.5f));
}

TEST(ScriptValue, EqualityString) {
    EXPECT_EQ(ScriptValue("hello"), ScriptValue("hello"));
    EXPECT_NE(ScriptValue("hello"), ScriptValue("world"));
}

TEST(ScriptValue, EqualityTypeMismatch) {
    EXPECT_NE(ScriptValue(42), ScriptValue("42"));
    EXPECT_NE(ScriptValue(true), ScriptValue(1));
}

TEST(ScriptValue, EqualityEntity) {
    EXPECT_EQ(ScriptValue::entity(5), ScriptValue::entity(5));
    EXPECT_NE(ScriptValue::entity(5), ScriptValue::entity(6));
}

// =============================================================================
// ToString
// =============================================================================

TEST(ScriptValue, ToStringNil) {
    EXPECT_EQ(ScriptValue::nil().to_string(), "nil");
}

TEST(ScriptValue, ToStringBool) {
    EXPECT_EQ(ScriptValue(true).to_string(), "true");
    EXPECT_EQ(ScriptValue(false).to_string(), "false");
}

TEST(ScriptValue, ToStringInt) {
    EXPECT_EQ(ScriptValue(42).to_string(), "42");
}

TEST(ScriptValue, ToStringString) {
    EXPECT_EQ(ScriptValue("hello").to_string(), "hello");
}

TEST(ScriptValue, ToStringEntity) {
    EXPECT_EQ(ScriptValue::entity(7).to_string(), "entity(7)");
}

TEST(ScriptValue, ToStringFunction) {
    ScriptValue fn(ScriptValue::FunctionType(
        [](const std::vector<ScriptValue>&) { return ScriptValue::nil(); }));
    EXPECT_EQ(fn.to_string(), "<function>");
}

TEST(ScriptValue, ToStringTable) {
    EXPECT_EQ(ScriptValue::table().to_string(), "<table>");
}

// =============================================================================
// ScriptContext Tests
// =============================================================================

TEST(ScriptContext, SetAndGet) {
    ScriptContext ctx;
    ctx.set("x", ScriptValue(42));
    EXPECT_EQ(ctx.get("x").as_int(), 42);
}

TEST(ScriptContext, MissingIsNil) {
    ScriptContext ctx;
    EXPECT_TRUE(ctx.get("missing").is_nil());
}

TEST(ScriptContext, Has) {
    ScriptContext ctx;
    ctx.set("x", ScriptValue(1));
    EXPECT_TRUE(ctx.has("x"));
    EXPECT_FALSE(ctx.has("y"));
}

TEST(ScriptContext, ParentLookup) {
    ScriptContext parent;
    parent.set("x", ScriptValue(10));

    ScriptContext child(&parent);
    EXPECT_EQ(child.get("x").as_int(), 10);
    EXPECT_TRUE(child.has("x"));
}

TEST(ScriptContext, ChildShadowsParent) {
    ScriptContext parent;
    parent.set("x", ScriptValue(10));

    ScriptContext child(&parent);
    child.set("x", ScriptValue(20));
    EXPECT_EQ(child.get("x").as_int(), 20);
    EXPECT_EQ(parent.get("x").as_int(), 10);
}

TEST(ScriptContext, SetUpvalue) {
    ScriptContext parent;
    parent.set("x", ScriptValue(10));

    ScriptContext child(&parent);
    child.set_upvalue("x", ScriptValue(20));  // modifies parent
    EXPECT_EQ(parent.get("x").as_int(), 20);
}

TEST(ScriptContext, SetUpvalueCreatesLocal) {
    ScriptContext parent;
    ScriptContext child(&parent);
    child.set_upvalue("y", ScriptValue(99));  // not in parent, so set locally
    EXPECT_EQ(child.get("y").as_int(), 99);
    EXPECT_TRUE(parent.get("y").is_nil());
}

TEST(ScriptContext, Locals) {
    ScriptContext ctx;
    ctx.set("a", ScriptValue(1));
    ctx.set("b", ScriptValue(2));
    EXPECT_EQ(ctx.locals().size(), 2u);
}
