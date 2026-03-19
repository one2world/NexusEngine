#include <gtest/gtest.h>
#include "nexus/scripting/script_component.h"
#include "nexus/scripting/script_engine.h"
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"

using namespace nexus;
using namespace nexus::scripting;

// =============================================================================
// ScriptComponent Tests
// =============================================================================

TEST(ScriptComponent, DefaultValues) {
    ScriptComponent sc;
    EXPECT_TRUE(sc.script_name.empty());
    EXPECT_EQ(sc.context, nullptr);
    EXPECT_EQ(sc.on_create, nullptr);
    EXPECT_EQ(sc.on_update, nullptr);
    EXPECT_EQ(sc.on_destroy, nullptr);
    EXPECT_EQ(sc.on_collision, nullptr);
    EXPECT_FALSE(sc.initialized);
    EXPECT_TRUE(sc.enabled);
}

// =============================================================================
// ScriptSystem Tests
// =============================================================================

TEST(ScriptSystem, InitializeScripts) {
    ScriptEngine engine;
    Registry registry;
    ScriptSystem system(&engine);

    // Register a script callback
    bool created = false;
    engine.register_function("player", "on_create",
        [&](const std::vector<ScriptValue>&) -> ScriptValue {
            created = true;
            return ScriptValue::nil();
        });

    auto entity = registry.create();
    ScriptComponent sc;
    sc.script_name = "player";
    registry.add_component<ScriptComponent>(entity, std::move(sc));

    system.initialize_scripts(registry);
    EXPECT_TRUE(created);
    EXPECT_TRUE(registry.get_component<ScriptComponent>(entity).initialized);
}

TEST(ScriptSystem, UpdateScripts) {
    ScriptEngine engine;
    Registry registry;
    ScriptSystem system(&engine);

    float received_dt = 0.0f;
    engine.register_function("enemy", "on_update",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2) received_dt = args[1].as_float();
            return ScriptValue::nil();
        });

    auto entity = registry.create();
    ScriptComponent sc;
    sc.script_name = "enemy";
    registry.add_component<ScriptComponent>(entity, std::move(sc));

    // Initialize first
    system.initialize_scripts(registry);

    // Update
    system.update_scripts(registry, 0.016f);
    EXPECT_NEAR(received_dt, 0.016f, 0.001f);
}

TEST(ScriptSystem, DestroyScripts) {
    ScriptEngine engine;
    Registry registry;
    ScriptSystem system(&engine);

    bool destroyed = false;
    engine.register_function("item", "on_destroy",
        [&](const std::vector<ScriptValue>&) -> ScriptValue {
            destroyed = true;
            return ScriptValue::nil();
        });

    auto entity = registry.create();
    ScriptComponent sc;
    sc.script_name = "item";
    registry.add_component<ScriptComponent>(entity, std::move(sc));

    system.initialize_scripts(registry);
    system.destroy_scripts(registry);
    EXPECT_TRUE(destroyed);
    EXPECT_FALSE(registry.get_component<ScriptComponent>(entity).initialized);
}

TEST(ScriptSystem, OnCollision) {
    ScriptEngine engine;
    Registry registry;
    ScriptSystem system(&engine);

    u32 collided_with = 0;
    engine.register_function("player", "on_collision",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2) collided_with = args[1].as_entity();
            return ScriptValue::nil();
        });

    auto player = registry.create();
    auto wall = registry.create();

    ScriptComponent sc;
    sc.script_name = "player";
    registry.add_component<ScriptComponent>(player, std::move(sc));

    system.initialize_scripts(registry);
    system.on_collision(registry, player, wall);
    EXPECT_EQ(collided_with, wall);
}

TEST(ScriptSystem, DisabledScriptNotUpdated) {
    ScriptEngine engine;
    Registry registry;
    ScriptSystem system(&engine);

    bool updated = false;
    engine.register_function("npc", "on_update",
        [&](const std::vector<ScriptValue>&) -> ScriptValue {
            updated = true;
            return ScriptValue::nil();
        });

    auto entity = registry.create();
    ScriptComponent sc;
    sc.script_name = "npc";
    registry.add_component<ScriptComponent>(entity, std::move(sc));

    system.initialize_scripts(registry);

    // Disable the script
    registry.get_component<ScriptComponent>(entity).enabled = false;
    system.update_scripts(registry, 0.016f);
    EXPECT_FALSE(updated);
}

TEST(ScriptSystem, EntityPassedToCallbacks) {
    ScriptEngine engine;
    Registry registry;
    ScriptSystem system(&engine);

    u32 received_entity = 0;
    engine.register_function("test", "on_create",
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args.empty()) received_entity = args[0].as_entity();
            return ScriptValue::nil();
        });

    auto entity = registry.create();
    ScriptComponent sc;
    sc.script_name = "test";
    registry.add_component<ScriptComponent>(entity, std::move(sc));

    system.initialize_scripts(registry);
    EXPECT_EQ(received_entity, entity);
}

TEST(ScriptSystem, BindCreatesContext) {
    ScriptEngine engine;
    ScriptSystem system(&engine);

    ScriptComponent sc;
    sc.script_name = "test";
    system.bind_script(sc);

    EXPECT_NE(sc.context, nullptr);
    EXPECT_EQ(sc.context->parent(), &engine.globals());
}
