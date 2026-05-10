#include <gtest/gtest.h>
#include "nexus/scripting/script_engine.h"

using namespace nexus;
using namespace nexus::scripting;

// =============================================================================
// Function Registration
// =============================================================================

TEST(ScriptEngine, RegisterAndCall) {
    ScriptEngine engine;
    engine.register_function("Math", "add",
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
            return ScriptValue(args[0].as_int() + args[1].as_int());
        }, 2, 2);

    auto result = engine.call_function("Math.add", {ScriptValue(3), ScriptValue(4)});
    EXPECT_EQ(result.as_int(), 7);
}

TEST(ScriptEngine, CallNonexistent) {
    ScriptEngine engine;
    auto result = engine.call_function("doesnt.exist");
    EXPECT_TRUE(result.is_nil());
    EXPECT_EQ(engine.errors().size(), 1u);
}

TEST(ScriptEngine, CallTooFewArgs) {
    ScriptEngine engine;
    engine.register_function("", "need_two",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil();
        }, 2, 2);

    auto result = engine.call_function("need_two", {ScriptValue(1)});
    EXPECT_TRUE(result.is_nil());
    EXPECT_GE(engine.errors().size(), 1u);
}

TEST(ScriptEngine, CallTooManyArgs) {
    ScriptEngine engine;
    engine.register_function("", "need_one",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            return ScriptValue::nil();
        }, 1, 1);

    auto result = engine.call_function("need_one", {ScriptValue(1), ScriptValue(2)});
    EXPECT_TRUE(result.is_nil());
    EXPECT_GE(engine.errors().size(), 1u);
}

TEST(ScriptEngine, FindFunction) {
    ScriptEngine engine;
    engine.register_function("Foo", "bar",
        [](const std::vector<ScriptValue>&) { return ScriptValue::nil(); });

    EXPECT_NE(engine.find_function("Foo", "bar"), nullptr);
    EXPECT_EQ(engine.find_function("Foo", "baz"), nullptr);
}

TEST(ScriptEngine, RegisteredFunctionsList) {
    ScriptEngine engine;
    engine.register_function("A", "func1",
        [](const std::vector<ScriptValue>&) { return ScriptValue::nil(); });
    engine.register_function("B", "func2",
        [](const std::vector<ScriptValue>&) { return ScriptValue::nil(); });

    EXPECT_EQ(engine.registered_functions().size(), 2u);
}

TEST(ScriptEngine, FunctionDescription) {
    ScriptEngine engine;
    engine.register_function("X", "y",
        [](const std::vector<ScriptValue>&) { return ScriptValue::nil(); },
        0, 0, "Does nothing");

    auto* nf = engine.find_function("X", "y");
    ASSERT_NE(nf, nullptr);
    EXPECT_EQ(nf->description, "Does nothing");
}

// =============================================================================
// Globals
// =============================================================================

TEST(ScriptEngine, SetGetGlobal) {
    ScriptEngine engine;
    engine.set_global("pi", ScriptValue(3.14f));
    EXPECT_NEAR(engine.get_global("pi").as_float(), 3.14f, 0.001f);
}

TEST(ScriptEngine, GlobalMissingIsNil) {
    ScriptEngine engine;
    EXPECT_TRUE(engine.get_global("missing").is_nil());
}

TEST(ScriptEngine, RegisteredFunctionAsGlobal) {
    ScriptEngine engine;
    engine.register_function("", "greet",
        [](const std::vector<ScriptValue>&) {
            return ScriptValue("hello");
        });

    // Should also be accessible as a global
    auto fn = engine.get_global("greet");
    EXPECT_TRUE(fn.is_function());
    auto result = fn.call({});
    EXPECT_EQ(result.as_string(), "hello");
}

// =============================================================================
// Coroutines
// =============================================================================

TEST(ScriptEngine, CoroutineBasic) {
    ScriptEngine engine;
    std::vector<int> order;

    engine.start_coroutine([&](Coroutine& co) {
        order.push_back(1);
        co.yield();
        // In our simple callback model, yield() sets a flag but the body
        // continues to completion. This is a simplified coroutine model
        // without actual stack switching.
        order.push_back(3);
    });

    engine.update_coroutines(0.016f);
    order.push_back(2);

    // Body runs fully: pushes 1 and 3, then we push 2 after update
    EXPECT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(order[2], 2);
}

TEST(ScriptEngine, CoroutineWait) {
    ScriptEngine engine;
    bool completed = false;

    engine.start_coroutine([&](Coroutine& co) {
        co.wait(0.5f);
        // In our simple model, wait + yield means the coroutine is suspended
        // After wait expires, it will be resumed but body has already returned
        completed = true;
    });

    EXPECT_EQ(engine.active_coroutine_count(), 1u);

    // Tick but not enough time
    engine.update_coroutines(0.1f);
    EXPECT_EQ(engine.active_coroutine_count(), 1u);

    // Tick past wait time
    engine.update_coroutines(0.5f);
    // Coroutine body already completed on first resume
    EXPECT_TRUE(completed);
}

TEST(ScriptEngine, CoroutineStopById) {
    ScriptEngine engine;
    u32 id = engine.start_coroutine([](Coroutine& co) {
        co.yield();
    });

    EXPECT_EQ(engine.active_coroutine_count(), 1u);
    engine.stop_coroutine(id);
    EXPECT_EQ(engine.active_coroutine_count(), 0u);
}

TEST(ScriptEngine, CoroutineCountZeroAfterDead) {
    ScriptEngine engine;
    engine.start_coroutine([](Coroutine&) {
        // body completes immediately, no yield → dead
    });

    engine.update_coroutines(0.016f);
    EXPECT_EQ(engine.active_coroutine_count(), 0u);
}

// =============================================================================
// Hot Reload
// =============================================================================

TEST(ScriptEngine, RegisterScript) {
    ScriptEngine engine;
    engine.register_script("player", "on_create = function() end");
    EXPECT_EQ(engine.script_count(), 1u);
    EXPECT_EQ(engine.get_script_source("player"), "on_create = function() end");
}

TEST(ScriptEngine, RegisterMultipleScripts) {
    ScriptEngine engine;
    engine.register_script("a", "source_a");
    engine.register_script("b", "source_b");
    EXPECT_EQ(engine.script_count(), 2u);
}

TEST(ScriptEngine, MissingScriptSourceEmpty) {
    ScriptEngine engine;
    EXPECT_EQ(engine.get_script_source("nonexistent"), "");
}

// ── M44: get_script_path companion to get_script_source ──────────────────

TEST(ScriptEngine, GetScriptPathReturnsRegisteredFilePath) {
    ScriptEngine engine;
    engine.register_script("player",
                            "on_create = function() end",
                            "scripts/player.lua");
    EXPECT_EQ(engine.get_script_path("player"), "scripts/player.lua");
}

TEST(ScriptEngine, GetScriptPathEmptyForInlineSource) {
    // register_script with no file_path argument ⇒ inline source,
    // no on-disk path.  Reveal-in-Asset-Browser uses this signal
    // to know it can't scroll to a non-existent file.
    ScriptEngine engine;
    engine.register_script("inline", "x = 1");
    EXPECT_EQ(engine.get_script_path("inline"), "");
}

TEST(ScriptEngine, GetScriptPathEmptyForUnknownName) {
    ScriptEngine engine;
    EXPECT_EQ(engine.get_script_path("never_registered"), "");
}

TEST(ScriptEngine, ReloadNonexistentReturnsFalse) {
    ScriptEngine engine;
    EXPECT_FALSE(engine.reload_script("missing"));
}

// =============================================================================
// Error Handling
// =============================================================================

TEST(ScriptEngine, ErrorsCollected) {
    ScriptEngine engine;
    engine.call_function("nonexistent");
    engine.call_function("also_nonexistent");
    EXPECT_EQ(engine.errors().size(), 2u);
}

TEST(ScriptEngine, ClearErrors) {
    ScriptEngine engine;
    engine.call_function("nonexistent");
    EXPECT_EQ(engine.errors().size(), 1u);
    engine.clear_errors();
    EXPECT_EQ(engine.errors().size(), 0u);
}

TEST(ScriptEngine, ErrorHandler) {
    ScriptEngine engine;
    std::string last_error;
    engine.set_error_handler([&](const ScriptError& err) {
        last_error = err.message;
    });

    engine.call_function("bad_func");
    EXPECT_FALSE(last_error.empty());
    EXPECT_NE(last_error.find("bad_func"), std::string::npos);
}

TEST(ScriptEngine, ErrorFromException) {
    ScriptEngine engine;
    engine.register_function("", "throws",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            throw std::runtime_error("boom");
        });

    auto result = engine.call_function("throws");
    EXPECT_TRUE(result.is_nil());
    EXPECT_GE(engine.errors().size(), 1u);
    EXPECT_NE(engine.errors().back().message.find("boom"), std::string::npos);
}

// =============================================================================
// Coroutine Class Direct
// =============================================================================

TEST(Coroutine, InitialStatus) {
    Coroutine co([](Coroutine&) {});
    EXPECT_EQ(co.status(), Coroutine::Status::Suspended);
}

TEST(Coroutine, ResumeRunsBody) {
    bool ran = false;
    Coroutine co([&](Coroutine&) { ran = true; });
    co.resume();
    EXPECT_TRUE(ran);
}

TEST(Coroutine, DeadAfterCompletion) {
    Coroutine co([](Coroutine&) {});
    co.resume();
    EXPECT_EQ(co.status(), Coroutine::Status::Dead);
}

TEST(Coroutine, YieldSuspends) {
    Coroutine co([](Coroutine& c) {
        c.yield(ScriptValue(42));
    });
    co.resume();
    EXPECT_EQ(co.status(), Coroutine::Status::Suspended);
    EXPECT_EQ(co.last_yield().as_int(), 42);
}

TEST(Coroutine, WaitSetsTimer) {
    Coroutine co([](Coroutine& c) {
        c.wait(1.0f);
    });
    co.resume();
    EXPECT_TRUE(co.is_waiting());
    EXPECT_NEAR(co.wait_time(), 1.0f, 0.01f);
}

TEST(Coroutine, TickWait) {
    Coroutine co([](Coroutine& c) {
        c.wait(0.5f);
    });
    co.resume();
    EXPECT_FALSE(co.tick_wait(0.3f));
    EXPECT_TRUE(co.is_waiting());
    EXPECT_TRUE(co.tick_wait(0.3f));
    EXPECT_FALSE(co.is_waiting());
}

TEST(Coroutine, UniqueIds) {
    Coroutine a([](Coroutine&) {});
    Coroutine b([](Coroutine&) {});
    EXPECT_NE(a.id(), b.id());
}
