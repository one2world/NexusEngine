#include <gtest/gtest.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>

namespace nexus::tests {

TEST(ECS, CreateEntity) {
    nexus::Registry registry;
    auto e = registry.create();
    EXPECT_TRUE(registry.alive(e));
}

TEST(ECS, DestroyEntity) {
    nexus::Registry registry;
    auto e = registry.create();
    registry.destroy(e);
    EXPECT_FALSE(registry.alive(e));
}

TEST(ECS, AddAndGetComponent) {
    nexus::Registry registry;
    auto e = registry.create();

    registry.add_component<TagComponent>(e, TagComponent{"Player"});
    EXPECT_TRUE(registry.has_component<TagComponent>(e));

    auto& tag = registry.get_component<TagComponent>(e);
    EXPECT_EQ(tag.name, "Player");
}

TEST(ECS, RemoveComponent) {
    nexus::Registry registry;
    auto e = registry.create();
    registry.add_component<TagComponent>(e, TagComponent{"Test"});
    EXPECT_TRUE(registry.has_component<TagComponent>(e));

    registry.remove_component<TagComponent>(e);
    EXPECT_FALSE(registry.has_component<TagComponent>(e));
}

TEST(ECS, MultipleComponents) {
    nexus::Registry registry;
    auto e = registry.create();

    registry.add_component<TagComponent>(e, TagComponent{"Entity"});
    registry.add_component<Transform2DComponent>(e, Transform2DComponent{});

    EXPECT_TRUE(registry.has_component<TagComponent>(e));
    EXPECT_TRUE(registry.has_component<Transform2DComponent>(e));

    auto& transform = registry.get_component<Transform2DComponent>(e);
    EXPECT_FLOAT_EQ(transform.position.x, 0.0f);
    EXPECT_FLOAT_EQ(transform.position.y, 0.0f);
}

TEST(ECS, ViewIteration) {
    nexus::Registry registry;

    for (int i = 0; i < 5; ++i) {
        auto e = registry.create();
        registry.add_component<TagComponent>(e, TagComponent{"Entity" + std::to_string(i)});
    }

    int count = 0;
    registry.each<TagComponent>([&](Entity, TagComponent&) {
        ++count;
    });
    EXPECT_EQ(count, 5);
}

TEST(ECS, MultipleEntities) {
    nexus::Registry registry;
    auto e1 = registry.create();
    auto e2 = registry.create();
    auto e3 = registry.create();

    EXPECT_NE(e1, e2);
    EXPECT_NE(e2, e3);
    EXPECT_TRUE(registry.alive(e1));
    EXPECT_TRUE(registry.alive(e2));
    EXPECT_TRUE(registry.alive(e3));
}

TEST(ECS, Transform3DComponent) {
    nexus::Registry registry;
    auto e = registry.create();

    Transform3DComponent t;
    t.position = nexus::Vec3(1.0f, 2.0f, 3.0f);
    t.scale = nexus::Vec3(2.0f);
    registry.add_component<Transform3DComponent>(e, t);

    auto& comp = registry.get_component<Transform3DComponent>(e);
    EXPECT_FLOAT_EQ(comp.position.x, 1.0f);
    EXPECT_FLOAT_EQ(comp.position.y, 2.0f);
    EXPECT_FLOAT_EQ(comp.position.z, 3.0f);
    EXPECT_FLOAT_EQ(comp.scale.x, 2.0f);
}

} // namespace nexus::tests
