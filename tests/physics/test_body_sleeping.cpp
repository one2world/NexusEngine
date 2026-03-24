#include <gtest/gtest.h>
#include <nexus/physics/physics_world_3d.h>

namespace nexus::physics::tests {

TEST(BodySleeping, BodyFallsAsleep) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f}); // no gravity

    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;
    desc.position = {0.0f, 0.0f, 0.0f};
    desc.velocity = {0.0f, 0.0f, 0.0f};

    u32 id = world.create_body(desc);

    // Step enough to exceed SLEEP_TIME (0.5s)
    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->sleeping);
}

TEST(BodySleeping, MovingBodyStaysAwake) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;
    desc.position = {0.0f, 0.0f, 0.0f};
    desc.velocity = {10.0f, 0.0f, 0.0f}; // moving

    u32 id = world.create_body(desc);

    world.step(1.0f / 60.0f);

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_FALSE(body->sleeping);
}

TEST(BodySleeping, ForceWakesBody) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;
    desc.position = {0.0f, 0.0f, 0.0f};

    u32 id = world.create_body(desc);

    // Put to sleep
    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }
    auto* body = world.get_body(id);
    ASSERT_TRUE(body->sleeping);

    // Apply force should wake it
    world.apply_force(id, {10.0f, 0.0f, 0.0f});
    EXPECT_FALSE(body->sleeping);
}

TEST(BodySleeping, ImpulseWakesBody) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;

    u32 id = world.create_body(desc);

    // Put to sleep
    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }
    auto* body = world.get_body(id);
    ASSERT_TRUE(body->sleeping);

    world.apply_impulse(id, {5.0f, 0.0f, 0.0f});
    EXPECT_FALSE(body->sleeping);
}

TEST(BodySleeping, StaticBodyDoesNotSleep) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Static;
    desc.shape = Body3D::Box;
    desc.half_extents = {5.0f, 0.5f, 5.0f};
    desc.mass = 0.0f;

    u32 id = world.create_body(desc);

    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    // Static bodies don't have sleeping mechanics
    EXPECT_FALSE(body->sleeping);
}

} // namespace nexus::physics::tests
