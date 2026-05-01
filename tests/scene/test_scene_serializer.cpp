#include <gtest/gtest.h>
#include <nexus/scene/scene.h>
#include <nexus/scene/scene_serializer.h>
#include <nexus/scene/binary_serializer.h>
#include <nexus/scene/hierarchy.h>

namespace nexus::tests {

TEST(SceneSerializer, SerializeDeserializeRoundTrip) {
    // Create a scene with entities
    Scene scene;
    Entity e1 = scene.create_entity_3d("Cube");
    auto& t1 = scene.registry().get_component<Transform3DComponent>(e1);
    t1.position = Vec3(1.0f, 2.0f, 3.0f);
    t1.scale = Vec3(2.0f);

    Entity e2 = scene.create_entity_2d("Sprite");
    auto& t2 = scene.registry().get_component<Transform2DComponent>(e2);
    t2.position = Vec2(100.0f, 200.0f);
    t2.rotation = 1.5f;

    Entity e3 = scene.create_entity("Camera");
    scene.registry().add_component<CameraComponent>(e3, CameraComponent{true, false, 60.0f});

    // Serialize
    SceneSerializer serializer(scene);
    std::string json = serializer.to_json();
    EXPECT_FALSE(json.empty());

    // Deserialize into a new scene
    Scene scene2;
    SceneSerializer serializer2(scene2);
    EXPECT_TRUE(serializer2.from_json(json));

    // Verify entity count
    auto entities = scene2.registry().view<TagComponent>();
    EXPECT_EQ(entities.size(), 3u);
}

TEST(SceneSerializer, PreservesTransform3D) {
    Scene scene;
    Entity e = scene.create_entity_3d("TestObj");
    auto& t = scene.registry().get_component<Transform3DComponent>(e);
    t.position = Vec3(5.0f, 10.0f, 15.0f);
    t.scale = Vec3(3.0f, 4.0f, 5.0f);

    SceneSerializer serializer(scene);
    std::string json = serializer.to_json();

    Scene scene2;
    SceneSerializer serializer2(scene2);
    serializer2.from_json(json);

    auto entities = scene2.registry().view<Transform3DComponent>();
    ASSERT_EQ(entities.size(), 1u);

    auto& loaded = scene2.registry().get_component<Transform3DComponent>(entities[0]);
    EXPECT_FLOAT_EQ(loaded.position.x, 5.0f);
    EXPECT_FLOAT_EQ(loaded.position.y, 10.0f);
    EXPECT_FLOAT_EQ(loaded.position.z, 15.0f);
    EXPECT_FLOAT_EQ(loaded.scale.x, 3.0f);
    EXPECT_FLOAT_EQ(loaded.scale.y, 4.0f);
    EXPECT_FLOAT_EQ(loaded.scale.z, 5.0f);
}

TEST(SceneSerializer, PreservesTagName) {
    Scene scene;
    scene.create_entity("MySpecialEntity");

    SceneSerializer serializer(scene);
    std::string json = serializer.to_json();

    Scene scene2;
    SceneSerializer serializer2(scene2);
    serializer2.from_json(json);

    auto entities = scene2.registry().view<TagComponent>();
    ASSERT_EQ(entities.size(), 1u);

    auto& tag = scene2.registry().get_component<TagComponent>(entities[0]);
    EXPECT_EQ(tag.name, "MySpecialEntity");
}

TEST(SceneSerializer, InvalidJsonReturnsError) {
    Scene scene;
    SceneSerializer serializer(scene);
    EXPECT_FALSE(serializer.from_json("not valid json"));
    EXPECT_FALSE(serializer.from_json("{}"));
}

TEST(SceneSerializer, PreservesLightComponents) {
    Scene scene;
    Entity e = scene.create_entity_3d("Light");
    scene.registry().add_component<PointLightComponent>(e,
        PointLightComponent{{1.0f, 0.5f, 0.2f}, 2.0f, 15.0f});

    SceneSerializer serializer(scene);
    std::string json = serializer.to_json();

    Scene scene2;
    SceneSerializer serializer2(scene2);
    serializer2.from_json(json);

    auto entities = scene2.registry().view<PointLightComponent>();
    ASSERT_EQ(entities.size(), 1u);

    auto& light = scene2.registry().get_component<PointLightComponent>(entities[0]);
    EXPECT_FLOAT_EQ(light.color.r, 1.0f);
    EXPECT_FLOAT_EQ(light.color.g, 0.5f);
    EXPECT_FLOAT_EQ(light.intensity, 2.0f);
    EXPECT_FLOAT_EQ(light.radius, 15.0f);
}

TEST(SceneSerializer, DuplicateEntityCreatesIndependentCopy) {
    Scene scene;
    Entity src = scene.create_entity_3d("Box");
    scene.registry().add_component<MeshRendererComponent>(
        src, MeshRendererComponent{1u, 0u, {1.0f, 0.2f, 0.3f, 1.0f}});

    SceneSerializer serializer(scene);
    Entity dup = serializer.duplicate_entity(src);

    ASSERT_NE(dup, INVALID_ENTITY);
    EXPECT_NE(dup, src);
    EXPECT_TRUE(scene.registry().alive(dup));
    EXPECT_TRUE(scene.registry().has_component<MeshRendererComponent>(dup));

    auto& src_mr = scene.registry().get_component<MeshRendererComponent>(src);
    auto& dup_mr = scene.registry().get_component<MeshRendererComponent>(dup);
    EXPECT_EQ(dup_mr.mesh_id, src_mr.mesh_id);
    EXPECT_FLOAT_EQ(dup_mr.tint.r, src_mr.tint.r);

    auto& dup_tag = scene.registry().get_component<TagComponent>(dup);
    EXPECT_NE(dup_tag.name, "Box");
    EXPECT_NE(dup_tag.name.find("(1)"), std::string::npos);

    // Mutating the duplicate must not touch the source — verifies independence.
    dup_mr.tint = {0.0f, 0.0f, 0.0f, 1.0f};
    EXPECT_FLOAT_EQ(src_mr.tint.r, 1.0f);
}

TEST(SceneSerializer, DuplicateEntityCopiesDescendants) {
    Scene scene;
    Entity parent = scene.create_entity_3d("Parent");
    Entity child  = scene.create_entity_3d("Child");
    Hierarchy::set_parent(scene.registry(), child, parent);

    SceneSerializer serializer(scene);
    Entity dup_parent = serializer.duplicate_entity(parent);
    ASSERT_NE(dup_parent, INVALID_ENTITY);

    auto dup_children = Hierarchy::get_children(scene.registry(), dup_parent);
    ASSERT_EQ(dup_children.size(), 1u);
    EXPECT_NE(dup_children[0], child);
    EXPECT_TRUE(scene.registry().has_component<TagComponent>(dup_children[0]));
}

TEST(SceneSerializer, DuplicateEntityReturnsInvalidForDeadSource) {
    Scene scene;
    SceneSerializer serializer(scene);
    EXPECT_EQ(serializer.duplicate_entity(INVALID_ENTITY), INVALID_ENTITY);
}

TEST(SceneSerializer, PreservesLockedComponent) {
    Scene scene;
    Entity locked = scene.create_entity_3d("Fixture");
    scene.registry().add_component<LockedComponent>(locked, LockedComponent{});
    Entity free = scene.create_entity_3d("Mover");

    SceneSerializer serializer(scene);
    std::string j = serializer.to_json();

    Scene reloaded;
    SceneSerializer reloader(reloaded);
    ASSERT_TRUE(reloader.from_json(j));

    // Walk entities by tag so we decouple from entity-id reuse.
    bool saw_locked = false;
    bool saw_free = false;
    reloaded.registry().each<TagComponent>(
        [&](Entity e, TagComponent& tc) {
            const bool is_locked =
                reloaded.registry().has_component<LockedComponent>(e);
            if (tc.name == "Fixture") {
                EXPECT_TRUE(is_locked);
                saw_locked = true;
            } else if (tc.name == "Mover") {
                EXPECT_FALSE(is_locked);
                saw_free = true;
            }
        });
    EXPECT_TRUE(saw_locked);
    EXPECT_TRUE(saw_free);
    (void)locked;
    (void)free;
}

TEST(BinarySerializer, PreservesLockedComponent) {
    Scene scene;
    Entity locked = scene.create_entity_3d("Fixture");
    scene.registry().add_component<LockedComponent>(locked, LockedComponent{});
    Entity free_e = scene.create_entity_3d("Mover");

    nexus::BinarySceneSerializer ser(scene);
    auto bytes = ser.to_binary();
    ASSERT_FALSE(bytes.empty());

    Scene reloaded;
    nexus::BinarySceneSerializer reloader(reloaded);
    ASSERT_TRUE(reloader.from_binary(bytes));

    bool saw_locked = false, saw_free = false;
    reloaded.registry().each<TagComponent>(
        [&](Entity e, TagComponent& tc) {
            const bool is_locked =
                reloaded.registry().has_component<LockedComponent>(e);
            if (tc.name == "Fixture") {
                EXPECT_TRUE(is_locked);
                saw_locked = true;
            } else if (tc.name == "Mover") {
                EXPECT_FALSE(is_locked);
                saw_free = true;
            }
        });
    EXPECT_TRUE(saw_locked);
    EXPECT_TRUE(saw_free);
    (void)locked;
    (void)free_e;
}

TEST(BinarySerializer, PreservesInactiveFlag) {
    Scene scene;
    Entity hidden = scene.create_entity_3d("Ghost");
    scene.registry().set_active(hidden, false);
    Entity shown = scene.create_entity_3d("Visible");

    nexus::BinarySceneSerializer ser(scene);
    auto bytes = ser.to_binary();
    ASSERT_FALSE(bytes.empty());

    Scene reloaded;
    nexus::BinarySceneSerializer reloader(reloaded);
    ASSERT_TRUE(reloader.from_binary(bytes));

    bool saw_ghost = false, saw_visible = false;
    reloaded.registry().each<TagComponent>(
        [&](Entity e, TagComponent& tc) {
            if (tc.name == "Ghost") {
                EXPECT_FALSE(reloaded.registry().is_active(e));
                saw_ghost = true;
            } else if (tc.name == "Visible") {
                EXPECT_TRUE(reloaded.registry().is_active(e));
                saw_visible = true;
            }
        });
    EXPECT_TRUE(saw_ghost);
    EXPECT_TRUE(saw_visible);
    (void)hidden;
    (void)shown;
}

TEST(SceneSerializer, PreservesInactiveFlag) {
    Scene scene;
    Entity hidden = scene.create_entity_3d("Ghost");
    scene.registry().set_active(hidden, false);
    Entity shown = scene.create_entity_3d("Visible");

    SceneSerializer serializer(scene);
    std::string j = serializer.to_json();

    Scene reloaded;
    SceneSerializer reloader(reloaded);
    ASSERT_TRUE(reloader.from_json(j));

    bool saw_ghost = false;
    bool saw_visible = false;
    reloaded.registry().each<TagComponent>(
        [&](Entity e, TagComponent& tc) {
            if (tc.name == "Ghost") {
                EXPECT_FALSE(reloaded.registry().is_active(e));
                saw_ghost = true;
            } else if (tc.name == "Visible") {
                EXPECT_TRUE(reloaded.registry().is_active(e));
                saw_visible = true;
            }
        });
    EXPECT_TRUE(saw_ghost);
    EXPECT_TRUE(saw_visible);
    (void)hidden;
    (void)shown;
}

// =============================================================================
// MeshRendererComponent — shadows / probes / dynamic-occlusion round-trip
// =============================================================================

TEST(SceneSerializer, PreservesMeshRendererProbeAndShadowFields) {
    Scene scene;
    Entity e = scene.create_entity_3d("Pillar");
    auto& mr = scene.registry().add_component<MeshRendererComponent>(
        e, MeshRendererComponent{});
    mr.cast_shadows       = false;
    mr.receive_shadows    = false;
    mr.light_probes       = LightProbesMode::Off;
    mr.reflection_probes  = ReflectionProbesMode::Simple;
    mr.dynamic_occlusion  = false;
    mr.tint               = Vec4(0.25f, 0.50f, 0.75f, 1.0f);

    SceneSerializer ser(scene);
    const std::string json = ser.to_json();
    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(json));

    bool saw = false;
    loaded.registry().each<TagComponent>(
        [&](Entity ent, TagComponent& tc) {
            if (tc.name != "Pillar") return;
            saw = true;
            ASSERT_TRUE(loaded.registry().has_component<MeshRendererComponent>(ent));
            const auto& got =
                loaded.registry().get_component<MeshRendererComponent>(ent);
            EXPECT_FALSE(got.cast_shadows);
            EXPECT_FALSE(got.receive_shadows);
            EXPECT_EQ(got.light_probes, LightProbesMode::Off);
            EXPECT_EQ(got.reflection_probes, ReflectionProbesMode::Simple);
            EXPECT_FALSE(got.dynamic_occlusion);
            EXPECT_NEAR(got.tint.x, 0.25f, 1e-5f);
            EXPECT_NEAR(got.tint.y, 0.50f, 1e-5f);
            EXPECT_NEAR(got.tint.z, 0.75f, 1e-5f);
        });
    EXPECT_TRUE(saw);
}

TEST(SceneSerializer, MeshRendererDefaultsAreElidedAndStillReload) {
    // When all fields match defaults, the serializer should still round-trip
    // because non-default fields get omitted intentionally.  A pristine
    // MeshRenderer must come back identical.
    Scene scene;
    Entity e = scene.create_entity_3d("Plain");
    scene.registry().add_component<MeshRendererComponent>(
        e, MeshRendererComponent{});

    SceneSerializer ser(scene);
    const std::string json = ser.to_json();
    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(json));

    bool saw = false;
    loaded.registry().each<TagComponent>(
        [&](Entity ent, TagComponent& tc) {
            if (tc.name != "Plain") return;
            saw = true;
            ASSERT_TRUE(loaded.registry().has_component<MeshRendererComponent>(ent));
            const auto& got =
                loaded.registry().get_component<MeshRendererComponent>(ent);
            EXPECT_TRUE(got.cast_shadows);
            EXPECT_TRUE(got.receive_shadows);
            EXPECT_EQ(got.light_probes, LightProbesMode::BlendProbes);
            EXPECT_EQ(got.reflection_probes, ReflectionProbesMode::BlendProbes);
            EXPECT_TRUE(got.dynamic_occlusion);
        });
    EXPECT_TRUE(saw);
}

// =============================================================================
// TagComponent — category + layer round-trip
// =============================================================================

TEST(SceneSerializer, PreservesTagCategoryAndLayer) {
    Scene scene;
    Entity hero = scene.create_entity_3d("Hero");
    {
        auto& tc = scene.registry().get_component<TagComponent>(hero);
        tc.category = "Player";
        tc.layer    = 8;
    }
    Entity npc = scene.create_entity_3d("NPC");
    {
        auto& tc = scene.registry().get_component<TagComponent>(npc);
        tc.category = "GameController";
        tc.layer    = 12;
    }

    SceneSerializer ser(scene);
    const std::string json = ser.to_json();
    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(json));

    int hits = 0;
    loaded.registry().each<TagComponent>(
        [&](Entity, TagComponent& tc) {
            if (tc.name == "Hero") {
                EXPECT_EQ(tc.category, "Player");
                EXPECT_EQ(tc.layer, 8);
                ++hits;
            } else if (tc.name == "NPC") {
                EXPECT_EQ(tc.category, "GameController");
                EXPECT_EQ(tc.layer, 12);
                ++hits;
            }
        });
    EXPECT_EQ(hits, 2);
}

TEST(SceneSerializer, TagDefaultsAreElidedAndStillReload) {
    Scene scene;
    scene.create_entity_3d("Untagged_Layer0");
    SceneSerializer ser(scene);
    const std::string json = ser.to_json();
    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(json));

    bool saw = false;
    loaded.registry().each<TagComponent>(
        [&](Entity, TagComponent& tc) {
            if (tc.name != "Untagged_Layer0") return;
            saw = true;
            EXPECT_EQ(tc.category, "Untagged");
            EXPECT_EQ(tc.layer, 0);
        });
    EXPECT_TRUE(saw);
}

// =============================================================================
// AnimatorComponent — JSON round-trip
// =============================================================================

TEST(SceneSerializer, PreservesAnimatorComponentBindings) {
    Scene scene;
    Entity e = scene.create_entity_3d("Animated");
    scene.registry().add_component<AnimatorComponent>(e, AnimatorComponent{});
    {
        auto& a = scene.registry().get_component<AnimatorComponent>(e);
        a.clip_id       = 12345;
        a.speed         = 1.5f;
        a.playing       = true;
        a.looping       = false;
        a.play_on_start = false;
        a.time          = 0.42f;  // should NOT round-trip (playback state)
    }

    SceneSerializer ser(scene);
    const std::string json = ser.to_json();
    Scene loaded;
    SceneSerializer reloader(loaded);
    ASSERT_TRUE(reloader.from_json(json));

    bool saw = false;
    loaded.registry().each<TagComponent>(
        [&](Entity ent, TagComponent& tc) {
            if (tc.name != "Animated") return;
            saw = true;
            ASSERT_TRUE(loaded.registry().has_component<AnimatorComponent>(ent));
            const auto& a = loaded.registry().get_component<AnimatorComponent>(ent);
            EXPECT_EQ(a.clip_id, 12345u);
            EXPECT_NEAR(a.speed, 1.5f, 1e-5f);
            EXPECT_TRUE(a.playing);
            EXPECT_FALSE(a.looping);
            EXPECT_FALSE(a.play_on_start);
            EXPECT_NEAR(a.time, 0.0f, 1e-5f);  // playback state resets
        });
    EXPECT_TRUE(saw);
}

} // namespace nexus::tests
