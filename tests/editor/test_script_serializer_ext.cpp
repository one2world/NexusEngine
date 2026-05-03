#include <gtest/gtest.h>

#include "nexus/editor/script_serializer_ext.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/scene_serializer.h"
#include "nexus/scripting/script_component.h"

namespace nexus::editor::tests {

// ── Editor-side ScriptComponent serialiser ──────────────────────────────────
//
// The extension hook itself is exercised by the engine-level
// SceneSerializerExt suite.  These tests verify the editor's specific
// Script writer/reader pair: name + enabled round-trip, and that
// `initialized` always loads as false (so ScriptSystem rebinds + fires
// on_create on the next tick).

class ScriptSerializerExtFixture : public ::testing::Test {
protected:
    void SetUp() override {
        SceneSerializer::clear_extensions();
        install_script_serializer_extension();
    }
    void TearDown() override {
        SceneSerializer::clear_extensions();
    }
};

TEST_F(ScriptSerializerExtFixture, RoundTripPreservesNameAndEnabled) {
    Scene scene;
    Entity e = scene.create_entity_3d("Player");
    scripting::ScriptComponent sc;
    sc.script_name = "scripts/player_controller.lua";
    sc.enabled     = false;
    sc.initialized = true;  // should NOT be carried — runtime state
    scene.registry().add_component<scripting::ScriptComponent>(e, sc);

    SceneSerializer ser(scene);
    const std::string js = ser.to_json();

    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(js));

    bool seen = false;
    loaded.registry().each<TagComponent>(
        [&](Entity ent, TagComponent& tc) {
            if (tc.name != "Player") return;
            seen = true;
            ASSERT_TRUE(loaded.registry()
                .has_component<scripting::ScriptComponent>(ent));
            const auto& got = loaded.registry()
                .get_component<scripting::ScriptComponent>(ent);
            EXPECT_EQ(got.script_name,
                      "scripts/player_controller.lua");
            EXPECT_FALSE(got.enabled);
            // initialized must reset so ScriptSystem rebinds and re-fires
            // on_create at the next tick.
            EXPECT_FALSE(got.initialized);
        });
    EXPECT_TRUE(seen);
}

TEST_F(ScriptSerializerExtFixture, EntitiesWithoutScriptComponentDontGainOne) {
    Scene scene;
    Entity e = scene.create_entity_3d("Plain");
    EXPECT_FALSE(scene.registry()
        .has_component<scripting::ScriptComponent>(e));

    SceneSerializer ser(scene);
    const std::string js = ser.to_json();

    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(js));

    loaded.registry().each<TagComponent>(
        [&](Entity ent, TagComponent& tc) {
            if (tc.name != "Plain") return;
            EXPECT_FALSE(loaded.registry()
                .has_component<scripting::ScriptComponent>(ent));
        });
}

TEST_F(ScriptSerializerExtFixture, EmptyScriptNameRoundTrips) {
    Scene scene;
    Entity e = scene.create_entity_3d("Stub");
    scripting::ScriptComponent sc;
    sc.script_name = "";
    sc.enabled     = true;
    scene.registry().add_component<scripting::ScriptComponent>(e, sc);

    SceneSerializer ser(scene);
    const std::string js = ser.to_json();

    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(js));

    loaded.registry().each<TagComponent>(
        [&](Entity ent, TagComponent& tc) {
            if (tc.name != "Stub") return;
            ASSERT_TRUE(loaded.registry()
                .has_component<scripting::ScriptComponent>(ent));
            const auto& got = loaded.registry()
                .get_component<scripting::ScriptComponent>(ent);
            EXPECT_EQ(got.script_name, "");
            EXPECT_TRUE(got.enabled);
        });
}

}  // namespace nexus::editor::tests
