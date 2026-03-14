#include <gtest/gtest.h>
#include <nexus/scene/scene.h>
#include <nexus/scene/scene_serializer.h>

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

} // namespace nexus::tests
