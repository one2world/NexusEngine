#include <gtest/gtest.h>
#include <nexus/physics/physics_world_3d.h>
#include <nexus/physics/constraints.h>
#include <cmath>

namespace nexus::physics::tests {

static u32 make_dynamic_body(PhysicsWorld3D& world, Vec3 pos) {
    Body3D desc;
    desc.type = Body3D::Dynamic;
    desc.shape = Body3D::Sphere;
    desc.radius = 0.5f;
    desc.mass = 1.0f;
    desc.position = pos;
    return world.create_body(desc);
}

TEST(Constraints, DistanceJointMaintainsDistance) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f}); // no gravity

    u32 a = make_dynamic_body(world, {0.0f, 0.0f, 0.0f});
    u32 b = make_dynamic_body(world, {3.0f, 0.0f, 0.0f});

    u32 jid = world.create_joint<DistanceJoint>(a, b);
    auto* joint = dynamic_cast<DistanceJoint*>(world.get_joint(jid));
    ASSERT_NE(joint, nullptr);
    joint->distance = 3.0f;
    joint->stiffness = 1.0f;

    // Push bodies apart
    world.apply_impulse(a, {-5.0f, 0.0f, 0.0f});
    world.apply_impulse(b, { 5.0f, 0.0f, 0.0f});

    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f, 16);
    }

    auto* ba = world.get_body(a);
    auto* bb = world.get_body(b);
    float dist = glm::length(bb->position - ba->position);
    EXPECT_NEAR(dist, 3.0f, 0.5f); // should converge toward target distance
}

TEST(Constraints, BallJointKeepsBodiesConnected) {
    PhysicsWorld3D world({0.0f, -9.81f, 0.0f});

    u32 a = make_dynamic_body(world, {0.0f, 5.0f, 0.0f});
    u32 b = make_dynamic_body(world, {0.0f, 5.0f, 0.0f});

    world.create_joint<BallJoint>(a, b);

    for (int i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f, 8);
    }

    auto* ba = world.get_body(a);
    auto* bb = world.get_body(b);
    float dist = glm::length(bb->position - ba->position);
    // Ball joint should keep anchor points reasonably close
    EXPECT_LT(dist, 2.0f);
}

TEST(Constraints, SpringJointAppliesRestoring) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f}); // no gravity

    u32 a = make_dynamic_body(world, {0.0f, 0.0f, 0.0f});
    u32 b = make_dynamic_body(world, {5.0f, 0.0f, 0.0f});

    u32 jid = world.create_joint<SpringJoint>(a, b);
    auto* spring = dynamic_cast<SpringJoint*>(world.get_joint(jid));
    ASSERT_NE(spring, nullptr);
    spring->rest_length = 2.0f;
    spring->stiffness = 20.0f;
    spring->damping = 1.0f;

    float initial_dist = 5.0f;

    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f, 8);
    }

    auto* ba = world.get_body(a);
    auto* bb = world.get_body(b);
    float dist = glm::length(bb->position - ba->position);
    // Spring should pull bodies closer to rest length
    EXPECT_LT(dist, initial_dist);
}

TEST(Constraints, DestroyJoint) {
    PhysicsWorld3D world({0.0f, 0.0f, 0.0f});

    u32 a = make_dynamic_body(world, {0.0f, 0.0f, 0.0f});
    u32 b = make_dynamic_body(world, {3.0f, 0.0f, 0.0f});

    u32 jid = world.create_joint<DistanceJoint>(a, b);
    EXPECT_NE(world.get_joint(jid), nullptr);

    world.destroy_joint(jid);
    EXPECT_EQ(world.get_joint(jid), nullptr);
}

} // namespace nexus::physics::tests
