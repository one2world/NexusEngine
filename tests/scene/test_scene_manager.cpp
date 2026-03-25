#include <gtest/gtest.h>
#include <nexus/scene/scene_manager.h>

namespace nexus::tests {

TEST(SceneManager, RegisterAndLoad) {
    SceneManager mgr;
    mgr.register_scene("main", [](Scene& scene) {
        scene.create_entity("Player");
    });

    EXPECT_TRUE(mgr.has_scene("main"));
    EXPECT_TRUE(mgr.load_scene("main"));
    EXPECT_NE(mgr.active_scene(), nullptr);
    EXPECT_EQ(mgr.active_scene_name(), "main");
}

TEST(SceneManager, LoadUnregisteredFails) {
    SceneManager mgr;
    EXPECT_FALSE(mgr.load_scene("nonexistent"));
    EXPECT_EQ(mgr.active_scene(), nullptr);
}

TEST(SceneManager, QueueAndProcessPending) {
    SceneManager mgr;
    bool loaded = false;
    mgr.register_scene("level1", [&](Scene&) { loaded = true; });

    mgr.queue_load_scene("level1");
    EXPECT_EQ(mgr.active_scene(), nullptr); // not loaded yet

    mgr.process_pending();
    EXPECT_TRUE(loaded);
    EXPECT_NE(mgr.active_scene(), nullptr);
}

TEST(SceneManager, SwitchScenesCallsCallbacks) {
    SceneManager mgr;
    std::string unloaded_name;
    std::string loaded_name;

    mgr.set_on_scene_unload([&](const std::string& n) { unloaded_name = n; });
    mgr.set_on_scene_loaded([&](const std::string& n) { loaded_name = n; });

    mgr.register_scene("a", nullptr);
    mgr.register_scene("b", nullptr);

    mgr.load_scene("a");
    EXPECT_EQ(loaded_name, "a");

    mgr.load_scene("b");
    EXPECT_EQ(unloaded_name, "a");
    EXPECT_EQ(loaded_name, "b");
}

TEST(SceneManager, SceneNames) {
    SceneManager mgr;
    mgr.register_scene("alpha", nullptr);
    mgr.register_scene("beta", nullptr);

    auto names = mgr.scene_names();
    EXPECT_EQ(names.size(), 2u);
}

} // namespace nexus::tests
