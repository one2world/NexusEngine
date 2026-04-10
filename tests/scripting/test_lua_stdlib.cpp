#include <gtest/gtest.h>
#include "nexus/scripting/script_engine.h"
#include "nexus/scripting/lua_stdlib.h"

#include <cmath>
#include <limits>

using namespace nexus;
using namespace nexus::scripting;

class LuaStdlibTest : public ::testing::Test {
protected:
    void SetUp() override {
        register_lua_stdlib(engine);
    }
    ScriptEngine engine;
};

// ── Math Library Tests ──────────────────────────────────────────────────────

TEST_F(LuaStdlibTest, MathAbs_PositiveInt) {
    auto result = engine.call_function("math.abs", {ScriptValue(i32(5))});
    EXPECT_EQ(result.as_int(), 5);
}

TEST_F(LuaStdlibTest, MathAbs_NegativeInt) {
    auto result = engine.call_function("math.abs", {ScriptValue(i32(-5))});
    EXPECT_EQ(result.as_int(), 5);
}

TEST_F(LuaStdlibTest, MathAbs_NegativeFloat) {
    auto result = engine.call_function("math.abs", {ScriptValue(-3.5f)});
    EXPECT_FLOAT_EQ(result.as_float(), 3.5f);
}

TEST_F(LuaStdlibTest, MathFloor) {
    auto result = engine.call_function("math.floor", {ScriptValue(3.7f)});
    EXPECT_FLOAT_EQ(result.as_float(), 3.0f);
}

TEST_F(LuaStdlibTest, MathFloor_Negative) {
    auto result = engine.call_function("math.floor", {ScriptValue(-2.3f)});
    EXPECT_FLOAT_EQ(result.as_float(), -3.0f);
}

TEST_F(LuaStdlibTest, MathCeil) {
    auto result = engine.call_function("math.ceil", {ScriptValue(3.2f)});
    EXPECT_FLOAT_EQ(result.as_float(), 4.0f);
}

TEST_F(LuaStdlibTest, MathCeil_Negative) {
    auto result = engine.call_function("math.ceil", {ScriptValue(-2.7f)});
    EXPECT_FLOAT_EQ(result.as_float(), -2.0f);
}

TEST_F(LuaStdlibTest, MathSqrt) {
    auto result = engine.call_function("math.sqrt", {ScriptValue(25.0f)});
    EXPECT_FLOAT_EQ(result.as_float(), 5.0f);
}

TEST_F(LuaStdlibTest, MathSqrt_Int) {
    auto result = engine.call_function("math.sqrt", {ScriptValue(i32(16))});
    EXPECT_FLOAT_EQ(result.as_float(), 4.0f);
}

TEST_F(LuaStdlibTest, MathSin) {
    auto result = engine.call_function("math.sin", {ScriptValue(0.0f)});
    EXPECT_NEAR(result.as_float(), 0.0f, 1e-6f);
}

TEST_F(LuaStdlibTest, MathCos) {
    auto result = engine.call_function("math.cos", {ScriptValue(0.0f)});
    EXPECT_NEAR(result.as_float(), 1.0f, 1e-6f);
}

TEST_F(LuaStdlibTest, MathTan) {
    auto result = engine.call_function("math.tan", {ScriptValue(0.0f)});
    EXPECT_NEAR(result.as_float(), 0.0f, 1e-6f);
}

TEST_F(LuaStdlibTest, MathAsin) {
    auto result = engine.call_function("math.asin", {ScriptValue(1.0f)});
    EXPECT_NEAR(result.as_float(), std::asin(1.0f), 1e-6f);
}

TEST_F(LuaStdlibTest, MathAcos) {
    auto result = engine.call_function("math.acos", {ScriptValue(1.0f)});
    EXPECT_NEAR(result.as_float(), 0.0f, 1e-6f);
}

TEST_F(LuaStdlibTest, MathAtan_OneArg) {
    auto result = engine.call_function("math.atan", {ScriptValue(1.0f)});
    EXPECT_NEAR(result.as_float(), std::atan(1.0f), 1e-6f);
}

TEST_F(LuaStdlibTest, MathAtan_TwoArgs) {
    auto result = engine.call_function("math.atan", {ScriptValue(1.0f), ScriptValue(1.0f)});
    EXPECT_NEAR(result.as_float(), std::atan2(1.0f, 1.0f), 1e-6f);
}

TEST_F(LuaStdlibTest, MathExp) {
    auto result = engine.call_function("math.exp", {ScriptValue(1.0f)});
    EXPECT_NEAR(result.as_float(), std::exp(1.0f), 1e-5f);
}

TEST_F(LuaStdlibTest, MathLog) {
    auto result = engine.call_function("math.log", {ScriptValue(1.0f)});
    EXPECT_NEAR(result.as_float(), 0.0f, 1e-6f);
}

TEST_F(LuaStdlibTest, MathPow) {
    auto result = engine.call_function("math.pow", {ScriptValue(2.0f), ScriptValue(10.0f)});
    EXPECT_NEAR(result.as_float(), 1024.0f, 1e-3f);
}

TEST_F(LuaStdlibTest, MathFmod) {
    auto result = engine.call_function("math.fmod", {ScriptValue(7.0f), ScriptValue(3.0f)});
    EXPECT_NEAR(result.as_float(), 1.0f, 1e-6f);
}

TEST_F(LuaStdlibTest, MathMax) {
    auto result = engine.call_function("math.max",
        {ScriptValue(i32(1)), ScriptValue(i32(5)), ScriptValue(i32(3))});
    EXPECT_FLOAT_EQ(result.as_float(), 5.0f);
}

TEST_F(LuaStdlibTest, MathMin) {
    auto result = engine.call_function("math.min",
        {ScriptValue(i32(1)), ScriptValue(i32(5)), ScriptValue(i32(3))});
    EXPECT_FLOAT_EQ(result.as_float(), 1.0f);
}

TEST_F(LuaStdlibTest, MathRandom_NoArgs) {
    engine.call_function("math.randomseed", {ScriptValue(i32(42))});
    auto result = engine.call_function("math.random", {});
    EXPECT_TRUE(result.is_float());
    EXPECT_GE(result.as_float(), 0.0f);
    EXPECT_LE(result.as_float(), 1.0f);
}

TEST_F(LuaStdlibTest, MathRandom_OneArg) {
    engine.call_function("math.randomseed", {ScriptValue(i32(42))});
    auto result = engine.call_function("math.random", {ScriptValue(i32(10))});
    EXPECT_TRUE(result.is_int());
    EXPECT_GE(result.as_int(), 1);
    EXPECT_LE(result.as_int(), 10);
}

TEST_F(LuaStdlibTest, MathRandom_TwoArgs) {
    engine.call_function("math.randomseed", {ScriptValue(i32(42))});
    auto result = engine.call_function("math.random", {ScriptValue(i32(5)), ScriptValue(i32(10))});
    EXPECT_TRUE(result.is_int());
    EXPECT_GE(result.as_int(), 5);
    EXPECT_LE(result.as_int(), 10);
}

TEST_F(LuaStdlibTest, MathPi) {
    auto result = engine.get_global("math.pi");
    EXPECT_TRUE(result.is_float());
    EXPECT_NEAR(result.as_float(), 3.14159265f, 1e-5f);
}

TEST_F(LuaStdlibTest, MathHuge) {
    auto result = engine.get_global("math.huge");
    EXPECT_TRUE(result.is_float());
    EXPECT_TRUE(std::isinf(result.as_float()));
}

// ── Math Type Coercion ──────────────────────────────────────────────────────

TEST_F(LuaStdlibTest, MathFloor_IntInput) {
    auto result = engine.call_function("math.floor", {ScriptValue(i32(5))});
    EXPECT_FLOAT_EQ(result.as_float(), 5.0f);
}

// ── String Library Tests ────────────────────────────────────────────────────

TEST_F(LuaStdlibTest, StringLen) {
    auto result = engine.call_function("string.len", {ScriptValue("hello")});
    EXPECT_EQ(result.as_int(), 5);
}

TEST_F(LuaStdlibTest, StringLen_Empty) {
    auto result = engine.call_function("string.len", {ScriptValue("")});
    EXPECT_EQ(result.as_int(), 0);
}

TEST_F(LuaStdlibTest, StringSub_Basic) {
    auto result = engine.call_function("string.sub",
        {ScriptValue("hello"), ScriptValue(i32(2)), ScriptValue(i32(4))});
    EXPECT_EQ(result.as_string(), "ell");
}

TEST_F(LuaStdlibTest, StringSub_NegativeIndex) {
    auto result = engine.call_function("string.sub",
        {ScriptValue("hello"), ScriptValue(i32(-3))});
    EXPECT_EQ(result.as_string(), "llo");
}

TEST_F(LuaStdlibTest, StringSub_NoEnd) {
    auto result = engine.call_function("string.sub",
        {ScriptValue("hello"), ScriptValue(i32(2))});
    EXPECT_EQ(result.as_string(), "ello");
}

TEST_F(LuaStdlibTest, StringSub_OutOfRange) {
    auto result = engine.call_function("string.sub",
        {ScriptValue("hello"), ScriptValue(i32(3)), ScriptValue(i32(1))});
    EXPECT_EQ(result.as_string(), "");
}

TEST_F(LuaStdlibTest, StringUpper) {
    auto result = engine.call_function("string.upper", {ScriptValue("hello")});
    EXPECT_EQ(result.as_string(), "HELLO");
}

TEST_F(LuaStdlibTest, StringLower) {
    auto result = engine.call_function("string.lower", {ScriptValue("HELLO")});
    EXPECT_EQ(result.as_string(), "hello");
}

TEST_F(LuaStdlibTest, StringRep) {
    auto result = engine.call_function("string.rep",
        {ScriptValue("ab"), ScriptValue(i32(3))});
    EXPECT_EQ(result.as_string(), "ababab");
}

TEST_F(LuaStdlibTest, StringRep_Zero) {
    auto result = engine.call_function("string.rep",
        {ScriptValue("ab"), ScriptValue(i32(0))});
    EXPECT_EQ(result.as_string(), "");
}

TEST_F(LuaStdlibTest, StringReverse) {
    auto result = engine.call_function("string.reverse", {ScriptValue("hello")});
    EXPECT_EQ(result.as_string(), "olleh");
}

TEST_F(LuaStdlibTest, StringReverse_Empty) {
    auto result = engine.call_function("string.reverse", {ScriptValue("")});
    EXPECT_EQ(result.as_string(), "");
}

TEST_F(LuaStdlibTest, StringByte) {
    auto result = engine.call_function("string.byte", {ScriptValue("A")});
    EXPECT_EQ(result.as_int(), 65);
}

TEST_F(LuaStdlibTest, StringByte_Position) {
    auto result = engine.call_function("string.byte",
        {ScriptValue("ABC"), ScriptValue(i32(2))});
    EXPECT_EQ(result.as_int(), 66); // 'B'
}

TEST_F(LuaStdlibTest, StringByte_OutOfRange) {
    auto result = engine.call_function("string.byte",
        {ScriptValue("A"), ScriptValue(i32(5))});
    EXPECT_TRUE(result.is_nil());
}

TEST_F(LuaStdlibTest, StringChar) {
    auto result = engine.call_function("string.char", {ScriptValue(i32(65))});
    EXPECT_EQ(result.as_string(), "A");
}

TEST_F(LuaStdlibTest, StringFind_Found) {
    auto result = engine.call_function("string.find",
        {ScriptValue("hello world"), ScriptValue("world")});
    EXPECT_EQ(result.as_int(), 7); // 1-based
}

TEST_F(LuaStdlibTest, StringFind_NotFound) {
    auto result = engine.call_function("string.find",
        {ScriptValue("hello world"), ScriptValue("xyz")});
    EXPECT_TRUE(result.is_nil());
}

TEST_F(LuaStdlibTest, StringFind_WithInit) {
    auto result = engine.call_function("string.find",
        {ScriptValue("hello hello"), ScriptValue("hello"), ScriptValue(i32(2))});
    EXPECT_EQ(result.as_int(), 7);
}

TEST_F(LuaStdlibTest, StringFormat_String) {
    auto result = engine.call_function("string.format",
        {ScriptValue("Hello %s!"), ScriptValue("world")});
    EXPECT_EQ(result.as_string(), "Hello world!");
}

TEST_F(LuaStdlibTest, StringFormat_Int) {
    auto result = engine.call_function("string.format",
        {ScriptValue("Value: %d"), ScriptValue(i32(42))});
    EXPECT_EQ(result.as_string(), "Value: 42");
}

TEST_F(LuaStdlibTest, StringFormat_Percent) {
    auto result = engine.call_function("string.format",
        {ScriptValue("100%%")});
    EXPECT_EQ(result.as_string(), "100%");
}

// ── Table Library Tests ─────────────────────────────────────────────────────

TEST_F(LuaStdlibTest, TableInsert_Append) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("b")});
    engine.call_function("table.insert", {t, ScriptValue("c")});

    auto tbl = t.as_table();
    EXPECT_EQ((*tbl)["1"].as_string(), "a");
    EXPECT_EQ((*tbl)["2"].as_string(), "b");
    EXPECT_EQ((*tbl)["3"].as_string(), "c");
}

TEST_F(LuaStdlibTest, TableInsert_AtPosition) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("c")});
    engine.call_function("table.insert", {t, ScriptValue(i32(2)), ScriptValue("b")});

    auto tbl = t.as_table();
    EXPECT_EQ((*tbl)["1"].as_string(), "a");
    EXPECT_EQ((*tbl)["2"].as_string(), "b");
    EXPECT_EQ((*tbl)["3"].as_string(), "c");
}

TEST_F(LuaStdlibTest, TableRemove_Last) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("b")});
    engine.call_function("table.insert", {t, ScriptValue("c")});

    auto removed = engine.call_function("table.remove", {t});
    EXPECT_EQ(removed.as_string(), "c");

    auto len = engine.call_function("table.getn", {t});
    EXPECT_EQ(len.as_int(), 2);
}

TEST_F(LuaStdlibTest, TableRemove_AtPosition) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("b")});
    engine.call_function("table.insert", {t, ScriptValue("c")});

    auto removed = engine.call_function("table.remove", {t, ScriptValue(i32(2))});
    EXPECT_EQ(removed.as_string(), "b");

    auto tbl = t.as_table();
    EXPECT_EQ((*tbl)["1"].as_string(), "a");
    EXPECT_EQ((*tbl)["2"].as_string(), "c");
}

TEST_F(LuaStdlibTest, TableConcat) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("b")});
    engine.call_function("table.insert", {t, ScriptValue("c")});

    auto result = engine.call_function("table.concat", {t, ScriptValue(", ")});
    EXPECT_EQ(result.as_string(), "a, b, c");
}

TEST_F(LuaStdlibTest, TableConcat_NoSep) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("b")});

    auto result = engine.call_function("table.concat", {t});
    EXPECT_EQ(result.as_string(), "ab");
}

TEST_F(LuaStdlibTest, TableSort_Numbers) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue(i32(3))});
    engine.call_function("table.insert", {t, ScriptValue(i32(1))});
    engine.call_function("table.insert", {t, ScriptValue(i32(2))});

    engine.call_function("table.sort", {t});

    auto tbl = t.as_table();
    EXPECT_EQ((*tbl)["1"].as_int(), 1);
    EXPECT_EQ((*tbl)["2"].as_int(), 2);
    EXPECT_EQ((*tbl)["3"].as_int(), 3);
}

TEST_F(LuaStdlibTest, TableSort_Strings) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("banana")});
    engine.call_function("table.insert", {t, ScriptValue("apple")});
    engine.call_function("table.insert", {t, ScriptValue("cherry")});

    engine.call_function("table.sort", {t});

    auto tbl = t.as_table();
    EXPECT_EQ((*tbl)["1"].as_string(), "apple");
    EXPECT_EQ((*tbl)["2"].as_string(), "banana");
    EXPECT_EQ((*tbl)["3"].as_string(), "cherry");
}

TEST_F(LuaStdlibTest, TableGetn) {
    auto t = ScriptValue::table();
    engine.call_function("table.insert", {t, ScriptValue("a")});
    engine.call_function("table.insert", {t, ScriptValue("b")});

    auto result = engine.call_function("table.getn", {t});
    EXPECT_EQ(result.as_int(), 2);
}

TEST_F(LuaStdlibTest, TableGetn_Empty) {
    auto t = ScriptValue::table();
    auto result = engine.call_function("table.getn", {t});
    EXPECT_EQ(result.as_int(), 0);
}

TEST_F(LuaStdlibTest, TableKeys) {
    auto t = ScriptValue::table();
    auto tbl = t.as_table();
    (*tbl)["name"] = ScriptValue("test");
    (*tbl)["value"] = ScriptValue(i32(42));

    auto keys = engine.call_function("table.keys", {t});
    EXPECT_TRUE(keys.is_table());

    auto keys_tbl = keys.as_table();
    auto len = engine.call_function("table.getn", {keys});
    EXPECT_EQ(len.as_int(), 2);
}

// ── OS Library Tests ────────────────────────────────────────────────────────

TEST_F(LuaStdlibTest, OsClock) {
    auto result = engine.call_function("os.clock", {});
    EXPECT_TRUE(result.is_float());
    EXPECT_GE(result.as_float(), 0.0f);
}

TEST_F(LuaStdlibTest, OsTime) {
    auto result = engine.call_function("os.time", {});
    EXPECT_TRUE(result.is_int());
    EXPECT_GT(result.as_int(), 0);
}

// ── Edge Cases ──────────────────────────────────────────────────────────────

TEST_F(LuaStdlibTest, MathAbs_Zero) {
    auto result = engine.call_function("math.abs", {ScriptValue(i32(0))});
    EXPECT_EQ(result.as_int(), 0);
}

TEST_F(LuaStdlibTest, StringSub_FullString) {
    auto result = engine.call_function("string.sub",
        {ScriptValue("hello"), ScriptValue(i32(1)), ScriptValue(i32(5))});
    EXPECT_EQ(result.as_string(), "hello");
}

TEST_F(LuaStdlibTest, StringRep_Negative) {
    auto result = engine.call_function("string.rep",
        {ScriptValue("ab"), ScriptValue(i32(-1))});
    EXPECT_EQ(result.as_string(), "");
}

TEST_F(LuaStdlibTest, StringFind_EmptyPattern) {
    auto result = engine.call_function("string.find",
        {ScriptValue("hello"), ScriptValue("")});
    EXPECT_EQ(result.as_int(), 1); // Empty string found at position 1
}

TEST_F(LuaStdlibTest, MathSin_Pi) {
    auto pi = engine.get_global("math.pi");
    auto result = engine.call_function("math.sin", {pi});
    EXPECT_NEAR(result.as_float(), 0.0f, 1e-5f);
}
