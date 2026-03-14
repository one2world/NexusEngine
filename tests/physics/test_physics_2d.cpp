#include <gtest/gtest.h>
#include <nexus/physics/physics_world_2d.h>

namespace nexus::physics::tests {

TEST(PhysicsWorld2D, CreateAndDestroyBody) {
    PhysicsWorld2D world;
    Body2D desc;
    desc.type = Body2D::Dynamic;
    desc.shape = Body2D::Circle;
    desc.radius = 1.0f;
    desc.density = 1.0f;

    u32 id = world.create_body(desc);
    EXPECT_NE(id, 0u);
    EXPECT_NE(world.get_body(id), nullptr);

    world.destroy_body(id);
    EXPECT_EQ(world.get_body(id), nullptr);
}

TEST(PhysicsWorld2D, GravityFallsDynamic) {
    PhysicsWorld2D world({0.0f, -10.0f});

    Body2D desc;
    desc.type = Body2D::Dynamic;
    desc.shape = Body2D::Circle;
    desc.radius = 0.5f;
    desc.density = 1.0f;
    desc.position = {0.0f, 10.0f};

    u32 id = world.create_body(desc);

    // Step for 1 second (60 steps at 1/60)
    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_LT(body->position.y, 10.0f); // Should have fallen
}

TEST(PhysicsWorld2D, StaticBodyDoesNotMove) {
    PhysicsWorld2D world({0.0f, -10.0f});

    Body2D desc;
    desc.type = Body2D::Static;
    desc.shape = Body2D::Box;
    desc.half_size = {5.0f, 0.5f};
    desc.position = {0.0f, 0.0f};
    desc.density = 1.0f;

    u32 id = world.create_body(desc);

    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_FLOAT_EQ(body->position.y, 0.0f);
}

TEST(PhysicsWorld2D, CircleVsCircleCollision) {
    PhysicsWorld2D world({0.0f, 0.0f}); // no gravity

    Body2D a;
    a.type = Body2D::Dynamic;
    a.shape = Body2D::Circle;
    a.radius = 1.0f;
    a.density = 1.0f;
    a.position = {0.0f, 0.0f};
    a.velocity = {1.0f, 0.0f};

    Body2D b;
    b.type = Body2D::Dynamic;
    b.shape = Body2D::Circle;
    b.radius = 1.0f;
    b.density = 1.0f;
    b.position = {1.5f, 0.0f};
    b.velocity = {0.0f, 0.0f};

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);

    // There should be contacts detected
    EXPECT_GE(world.contacts().size(), 1u);
}

TEST(PhysicsWorld2D, BoxVsBoxCollision) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D a;
    a.type = Body2D::Dynamic;
    a.shape = Body2D::Box;
    a.half_size = {1.0f, 1.0f};
    a.density = 1.0f;
    a.position = {0.0f, 0.0f};

    Body2D b;
    b.type = Body2D::Dynamic;
    b.shape = Body2D::Box;
    b.half_size = {1.0f, 1.0f};
    b.density = 1.0f;
    b.position = {1.5f, 0.0f}; // overlapping

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);
    EXPECT_GE(world.contacts().size(), 1u);
}

TEST(PhysicsWorld2D, ApplyForce) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D desc;
    desc.type = Body2D::Dynamic;
    desc.shape = Body2D::Circle;
    desc.radius = 0.5f;
    desc.density = 1.0f;
    desc.position = {0.0f, 0.0f};

    u32 id = world.create_body(desc);
    world.apply_force(id, {100.0f, 0.0f});
    world.step(1.0f / 60.0f);

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_GT(body->position.x, 0.0f);
}

TEST(PhysicsWorld2D, ApplyImpulse) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D desc;
    desc.type = Body2D::Dynamic;
    desc.shape = Body2D::Circle;
    desc.radius = 0.5f;
    desc.density = 1.0f;
    desc.position = {0.0f, 0.0f};

    u32 id = world.create_body(desc);
    world.apply_impulse(id, {5.0f, 0.0f});
    world.step(1.0f / 60.0f);

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_GT(body->velocity.x, 0.0f);
}

TEST(PhysicsWorld2D, RaycastHitsCircle) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D desc;
    desc.type = Body2D::Static;
    desc.shape = Body2D::Circle;
    desc.radius = 1.0f;
    desc.density = 1.0f;
    desc.position = {5.0f, 0.0f};

    u32 id = world.create_body(desc);

    RayHit2D hit;
    bool result = world.raycast({0.0f, 0.0f}, {1.0f, 0.0f}, 100.0f, hit);
    EXPECT_TRUE(result);
    EXPECT_EQ(hit.body_id, id);
    EXPECT_NEAR(hit.distance, 4.0f, 0.01f);
}

TEST(PhysicsWorld2D, RaycastMissesBody) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D desc;
    desc.type = Body2D::Static;
    desc.shape = Body2D::Circle;
    desc.radius = 1.0f;
    desc.density = 1.0f;
    desc.position = {5.0f, 5.0f};

    world.create_body(desc);

    RayHit2D hit;
    bool result = world.raycast({0.0f, 0.0f}, {1.0f, 0.0f}, 100.0f, hit);
    EXPECT_FALSE(result);
}

TEST(PhysicsWorld2D, OverlapCircle) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D desc;
    desc.type = Body2D::Static;
    desc.shape = Body2D::Circle;
    desc.radius = 1.0f;
    desc.density = 1.0f;
    desc.position = {3.0f, 0.0f};
    world.create_body(desc);

    desc.position = {10.0f, 0.0f};
    world.create_body(desc);

    auto results = world.overlap_circle({0.0f, 0.0f}, 5.0f);
    EXPECT_EQ(results.size(), 1u); // Only first body in range
}

TEST(PhysicsWorld2D, CollisionLayerFiltering) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D a;
    a.type = Body2D::Dynamic;
    a.shape = Body2D::Circle;
    a.radius = 1.0f;
    a.density = 1.0f;
    a.position = {0.0f, 0.0f};
    a.layer = 1;
    a.mask = 2; // Only collide with layer 2

    Body2D b;
    b.type = Body2D::Dynamic;
    b.shape = Body2D::Circle;
    b.radius = 1.0f;
    b.density = 1.0f;
    b.position = {1.0f, 0.0f};
    b.layer = 1; // Same layer as a
    b.mask = 2;

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);
    // Should not collide because a.mask(2) doesn't include b.layer(1)
    EXPECT_EQ(world.contacts().size(), 0u);
}

TEST(PhysicsWorld2D, TriggerDoesNotResolve) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D a;
    a.type = Body2D::Dynamic;
    a.shape = Body2D::Circle;
    a.radius = 1.0f;
    a.density = 1.0f;
    a.position = {0.0f, 0.0f};
    a.velocity = {1.0f, 0.0f};

    Body2D b;
    b.type = Body2D::Dynamic;
    b.shape = Body2D::Circle;
    b.radius = 1.0f;
    b.density = 1.0f;
    b.position = {1.5f, 0.0f};
    b.is_trigger = true;

    u32 id_a = world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);

    // Trigger should be detected but not resolved
    auto* body_a = world.get_body(id_a);
    ASSERT_NE(body_a, nullptr);
    // Velocity should remain positive (not bounced back)
    EXPECT_GT(body_a->velocity.x, 0.0f);
}

TEST(PhysicsWorld2D, ContactCallback) {
    PhysicsWorld2D world({0.0f, 0.0f});

    Body2D a;
    a.type = Body2D::Dynamic;
    a.shape = Body2D::Circle;
    a.radius = 1.0f;
    a.density = 1.0f;
    a.position = {0.0f, 0.0f};

    Body2D b;
    b.type = Body2D::Dynamic;
    b.shape = Body2D::Circle;
    b.radius = 1.0f;
    b.density = 1.0f;
    b.position = {1.0f, 0.0f};

    world.create_body(a);
    world.create_body(b);

    int callback_count = 0;
    world.set_contact_callback([&](const CollisionPair2D&) {
        ++callback_count;
    });

    world.step(1.0f / 60.0f);
    EXPECT_GT(callback_count, 0);
}

} // namespace nexus::physics::tests
