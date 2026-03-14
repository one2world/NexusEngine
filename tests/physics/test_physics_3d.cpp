#include <gtest/gtest.h>
#include <nexus/physics/physics_world_3d.h>

namespace nexus::physics::tests {

TEST(PhysicsWorld3D, CreateAndDestroyBody) {
    PhysicsWorld3D world;
    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 1.0f;
    desc.mass = 1.0f;

    u32 id = world.create_body(desc);
    EXPECT_NE(id, 0u);
    EXPECT_NE(world.get_body(id), nullptr);

    world.destroy_body(id);
    EXPECT_EQ(world.get_body(id), nullptr);
}

TEST(PhysicsWorld3D, GravityFallsDynamic) {
    PhysicsWorld3D world({0.0f, -10.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;
    desc.position = {0.0f, 10.0f, 0.0f};

    u32 id = world.create_body(desc);

    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_LT(body->position.y, 10.0f);
}

TEST(PhysicsWorld3D, StaticBodyDoesNotMove) {
    PhysicsWorld3D world({0.0f, -10.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Static;
    desc.shape = Body3D::Box;
    desc.half_extents = {5.0f, 0.5f, 5.0f};
    desc.mass = 0.0f;
    desc.position = {0.0f, 0.0f, 0.0f};

    u32 id = world.create_body(desc);

    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_FLOAT_EQ(body->position.y, 0.0f);
}

TEST(PhysicsWorld3D, SphereVsSphereCollision) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D a;
    a.type = Body3D::Dynamic;
    a.shape = Body3D::Sphere;
    a.radius = 1.0f;
    a.mass = 1.0f;
    a.position = {0.0f, 0.0f, 0.0f};

    Body3D b;
    b.type = Body3D::Dynamic;
    b.shape = Body3D::Sphere;
    b.radius = 1.0f;
    b.mass = 1.0f;
    b.position = {1.5f, 0.0f, 0.0f};

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);
    EXPECT_GE(world.contacts().size(), 1u);
}

TEST(PhysicsWorld3D, BoxVsBoxCollision) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D a;
    a.type = Body3D::Dynamic;
    a.shape = Body3D::Box;
    a.half_extents = {1.0f, 1.0f, 1.0f};
    a.mass = 1.0f;
    a.position = {0.0f, 0.0f, 0.0f};

    Body3D b;
    b.type = Body3D::Dynamic;
    b.shape = Body3D::Box;
    b.half_extents = {1.0f, 1.0f, 1.0f};
    b.mass = 1.0f;
    b.position = {1.5f, 0.0f, 0.0f};

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);
    EXPECT_GE(world.contacts().size(), 1u);
}

TEST(PhysicsWorld3D, SphereVsBoxCollision) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D a;
    a.type = Body3D::Dynamic;
    a.shape = Body3D::Sphere;
    a.radius = 1.0f;
    a.mass = 1.0f;
    a.position = {0.0f, 0.0f, 0.0f};

    Body3D b;
    b.type = Body3D::Dynamic;
    b.shape = Body3D::Box;
    b.half_extents = {1.0f, 1.0f, 1.0f};
    b.mass = 1.0f;
    b.position = {1.5f, 0.0f, 0.0f};

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);
    EXPECT_GE(world.contacts().size(), 1u);
}

TEST(PhysicsWorld3D, RaycastHitsSphere) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Static;
    desc.shape = Body3D::Sphere;
    desc.radius = 1.0f;
    desc.mass = 0.0f;
    desc.position = {5.0f, 0.0f, 0.0f};

    u32 id = world.create_body(desc);

    RayHit3D hit;
    bool result = world.raycast({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 100.0f, hit);
    EXPECT_TRUE(result);
    EXPECT_EQ(hit.body_id, id);
    EXPECT_NEAR(hit.distance, 4.0f, 0.01f);
}

TEST(PhysicsWorld3D, RaycastHitsBox) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Static;
    desc.shape = Body3D::Box;
    desc.half_extents = {1.0f, 1.0f, 1.0f};
    desc.mass = 0.0f;
    desc.position = {5.0f, 0.0f, 0.0f};

    u32 id = world.create_body(desc);

    RayHit3D hit;
    bool result = world.raycast({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 100.0f, hit);
    EXPECT_TRUE(result);
    EXPECT_EQ(hit.body_id, id);
    EXPECT_NEAR(hit.distance, 4.0f, 0.01f);
}

TEST(PhysicsWorld3D, OverlapSphere) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Static;
    desc.shape = Body3D::Sphere;
    desc.radius = 1.0f;
    desc.mass = 0.0f;
    desc.position = {3.0f, 0.0f, 0.0f};
    world.create_body(desc);

    desc.position = {20.0f, 0.0f, 0.0f};
    world.create_body(desc);

    auto results = world.overlap_sphere({0.0f, 0.0f, 0.0f}, 5.0f);
    EXPECT_EQ(results.size(), 1u);
}

TEST(PhysicsWorld3D, ApplyForce) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;
    desc.position = {0.0f, 0.0f, 0.0f};

    u32 id = world.create_body(desc);
    world.apply_force(id, {100.0f, 0.0f, 0.0f});
    world.step(1.0f / 60.0f);

    auto* body = world.get_body(id);
    ASSERT_NE(body, nullptr);
    EXPECT_GT(body->position.x, 0.0f);
}

TEST(PhysicsWorld3D, CollisionLayerFiltering) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    Body3D a;
    a.type = Body3D::Dynamic;
    a.shape = Body3D::Sphere;
    a.radius = 1.0f;
    a.mass = 1.0f;
    a.position = {0.0f, 0.0f, 0.0f};
    a.layer = 1;
    a.mask = 2;

    Body3D b;
    b.type = Body3D::Dynamic;
    b.shape = Body3D::Sphere;
    b.radius = 1.0f;
    b.mass = 1.0f;
    b.position = {1.0f, 0.0f, 0.0f};
    b.layer = 1;
    b.mask = 2;

    world.create_body(a);
    world.create_body(b);

    world.step(1.0f / 60.0f);
    EXPECT_EQ(world.contacts().size(), 0u);
}

} // namespace nexus::physics::tests
