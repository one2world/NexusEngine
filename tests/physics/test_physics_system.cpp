#include <gtest/gtest.h>
#include <nexus/physics/physics_system.h>
#include <nexus/scene/components.h>

namespace nexus::physics::tests {

TEST(PhysicsSystem, SyncCreatesBody2D) {
    PhysicsSystem sys;
    Registry reg;

    Entity e = reg.create();
    reg.add_component<Transform2DComponent>(e, Transform2DComponent{{5.0f, 10.0f}});
    reg.add_component<RigidBody2DComponent>(e, RigidBody2DComponent{});
    reg.add_component<Collider2DComponent>(e, Collider2DComponent{});

    sys.sync_to_physics(reg);

    EXPECT_EQ(sys.world_2d().bodies().size(), 1u);
    EXPECT_FLOAT_EQ(sys.world_2d().bodies()[0].position.x, 5.0f);
}

TEST(PhysicsSystem, SyncCreatesBody3D) {
    PhysicsSystem sys;
    Registry reg;

    Entity e = reg.create();
    reg.add_component<Transform3DComponent>(e, Transform3DComponent{{1.0f, 2.0f, 3.0f}});
    RigidBody3DComponent rb;
    rb.mass = 2.0f;
    reg.add_component<RigidBody3DComponent>(e, rb);
    Collider3DComponent col;
    col.shape = Collider3DComponent::Sphere;
    col.radius = 1.5f;
    reg.add_component<Collider3DComponent>(e, col);

    sys.sync_to_physics(reg);

    EXPECT_EQ(sys.world_3d().bodies().size(), 1u);
    EXPECT_FLOAT_EQ(sys.world_3d().bodies()[0].position.y, 2.0f);
}

TEST(PhysicsSystem, FullUpdateCycle) {
    PhysicsSystem sys;
    sys.set_gravity_2d({0.0f, -10.0f});

    Registry reg;

    Entity e = reg.create();
    Transform2DComponent t;
    t.position = {0.0f, 10.0f};
    reg.add_component<Transform2DComponent>(e, t);
    reg.add_component<RigidBody2DComponent>(e, RigidBody2DComponent{});
    reg.add_component<Collider2DComponent>(e, Collider2DComponent{});

    // Run for ~1 second
    for (int i = 0; i < 60; ++i) {
        sys.update(reg, 1.0f / 60.0f);
    }

    auto& transform = reg.get_component<Transform2DComponent>(e);
    EXPECT_LT(transform.position.y, 10.0f); // Should have fallen
}

TEST(PhysicsSystem, Raycast2D) {
    PhysicsSystem sys;
    Registry reg;

    Entity e = reg.create();
    Transform2DComponent t;
    t.position = {5.0f, 0.0f};
    reg.add_component<Transform2DComponent>(e, t);
    reg.add_component<RigidBody2DComponent>(e, RigidBody2DComponent{
        RigidBody2DComponent::Static
    });
    Collider2DComponent col;
    col.shape = Collider2DComponent::Circle;
    col.radius = 1.0f;
    reg.add_component<Collider2DComponent>(e, col);

    sys.sync_to_physics(reg);

    RayHit hit;
    bool result = sys.raycast_2d({0.0f, 0.0f}, {1.0f, 0.0f}, 100.0f, hit);
    EXPECT_TRUE(result);
    EXPECT_EQ(hit.entity, e);
    EXPECT_NEAR(hit.distance, 4.0f, 0.01f);
}

TEST(PhysicsSystem, Raycast3D) {
    PhysicsSystem sys;
    Registry reg;

    Entity e = reg.create();
    Transform3DComponent t;
    t.position = {0.0f, 0.0f, -10.0f};
    reg.add_component<Transform3DComponent>(e, t);
    reg.add_component<RigidBody3DComponent>(e, RigidBody3DComponent{
        RigidBody3DComponent::Static
    });
    Collider3DComponent col;
    col.shape = Collider3DComponent::Sphere;
    col.radius = 1.0f;
    reg.add_component<Collider3DComponent>(e, col);

    sys.sync_to_physics(reg);

    RayHit hit;
    bool result = sys.raycast_3d({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, 100.0f, hit);
    EXPECT_TRUE(result);
    EXPECT_EQ(hit.entity, e);
    EXPECT_NEAR(hit.distance, 9.0f, 0.01f);
}

TEST(PhysicsSystem, FixedTimestep) {
    PhysicsSystem sys;
    sys.set_fixed_timestep(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(sys.fixed_timestep(), 1.0f / 60.0f);
}

} // namespace nexus::physics::tests
