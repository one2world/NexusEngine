#include <gtest/gtest.h>
#include <nexus/animation/skeleton.h>
#include <nexus/animation/animation_clip.h>
#include <nexus/animation/animation_state_machine.h>

namespace nexus::anim::tests {

static Skeleton make_simple_skeleton() {
    Skeleton skel;
    Bone root;
    root.name = "root";
    root.parent_index = -1;
    root.local_position = {0, 0, 0};
    root.inverse_bind_pose = Mat4(1.0f);
    skel.add_bone(root);

    Bone spine;
    spine.name = "spine";
    spine.parent_index = 0;
    spine.local_position = {0, 1, 0};
    spine.inverse_bind_pose = glm::inverse(glm::translate(Mat4(1.0f), Vec3(0, 1, 0)));
    skel.add_bone(spine);

    Bone head;
    head.name = "head";
    head.parent_index = 1;
    head.local_position = {0, 1, 0};
    head.inverse_bind_pose = glm::inverse(glm::translate(Mat4(1.0f), Vec3(0, 2, 0)));
    skel.add_bone(head);

    return skel;
}

TEST(Skeleton, AddAndFindBone) {
    auto skel = make_simple_skeleton();
    EXPECT_EQ(skel.bone_count(), 3u);
    EXPECT_EQ(skel.find_bone("root"), 0);
    EXPECT_EQ(skel.find_bone("spine"), 1);
    EXPECT_EQ(skel.find_bone("head"), 2);
    EXPECT_EQ(skel.find_bone("nonexistent"), -1);
}

TEST(Skeleton, BindPose) {
    auto skel = make_simple_skeleton();
    auto pose = skel.get_bind_pose();
    EXPECT_EQ(pose.size(), 3u);
    EXPECT_FLOAT_EQ(pose[1].position.y, 1.0f);
}

TEST(Skeleton, SkinMatrices) {
    auto skel = make_simple_skeleton();
    auto pose = skel.get_bind_pose();
    auto matrices = skel.compute_skin_matrices(pose);
    EXPECT_EQ(matrices.size(), 3u);

    // At bind pose, skin matrices should be identity (world * inverse_bind = I)
    for (size_t i = 0; i < 3; ++i) {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                float expected = (r == c) ? 1.0f : 0.0f;
                EXPECT_NEAR(matrices[i][c][r], expected, 1e-4f)
                    << "bone=" << i << " row=" << r << " col=" << c;
            }
        }
    }
}

TEST(AnimationClip, SampleSingleKey) {
    AnimationClip clip("test", 1.0f);
    BoneChannel ch;
    ch.bone_index = 0;
    ch.positions.push_back({0.0f, {0, 0, 0}});
    ch.positions.push_back({1.0f, {10, 0, 0}});
    ch.rotations.push_back({0.0f, Quat(1, 0, 0, 0)});
    ch.scales.push_back({0.0f, Vec3(1.0f)});
    clip.add_channel(ch);

    std::vector<BonePose> poses(1);
    clip.sample(0.5f, poses);

    EXPECT_NEAR(poses[0].position.x, 5.0f, 0.01f);
}

TEST(AnimationClip, BlendTwoPoses) {
    std::vector<BonePose> a(2), b(2), out;
    a[0].position = {0, 0, 0};
    b[0].position = {10, 0, 0};
    a[1].position = {0, 5, 0};
    b[1].position = {0, 15, 0};

    AnimationClip::blend(a, b, 0.5f, out);

    EXPECT_EQ(out.size(), 2u);
    EXPECT_NEAR(out[0].position.x, 5.0f, 0.01f);
    EXPECT_NEAR(out[1].position.y, 10.0f, 0.01f);
}

TEST(AnimationStateMachine, BasicTransition) {
    auto skel = make_simple_skeleton();

    AnimationClip idle("idle", 1.0f);
    {
        BoneChannel ch;
        ch.bone_index = 0;
        ch.positions.push_back({0.0f, {0, 0, 0}});
        ch.positions.push_back({1.0f, {0, 0, 0}});
        ch.rotations.push_back({0.0f, Quat(1, 0, 0, 0)});
        ch.scales.push_back({0.0f, Vec3(1.0f)});
        idle.add_channel(ch);
    }

    AnimationClip walk("walk", 1.0f);
    {
        BoneChannel ch;
        ch.bone_index = 0;
        ch.positions.push_back({0.0f, {0, 0, 0}});
        ch.positions.push_back({1.0f, {5, 0, 0}});
        ch.rotations.push_back({0.0f, Quat(1, 0, 0, 0)});
        ch.scales.push_back({0.0f, Vec3(1.0f)});
        walk.add_channel(ch);
    }

    AnimationStateMachine sm;
    sm.add_state("idle", &idle);
    sm.add_state("walk", &walk);

    bool should_walk = false;
    sm.add_transition("idle", "walk", 0.2f, [&]() { return should_walk; });
    sm.set_state("idle");

    std::vector<BonePose> pose;
    sm.update(0.1f, skel, pose);
    EXPECT_EQ(sm.current_state(), "idle");

    should_walk = true;
    sm.update(0.1f, skel, pose);
    EXPECT_TRUE(sm.is_transitioning());

    // Complete transition
    sm.update(0.3f, skel, pose);
    EXPECT_EQ(sm.current_state(), "walk");
    EXPECT_FALSE(sm.is_transitioning());
}

TEST(AnimationStateMachine, Parameters) {
    AnimationStateMachine sm;
    sm.set_float("speed", 5.0f);
    sm.set_bool("grounded", true);

    EXPECT_FLOAT_EQ(sm.get_float("speed"), 5.0f);
    EXPECT_TRUE(sm.get_bool("grounded"));
    EXPECT_FLOAT_EQ(sm.get_float("nonexistent"), 0.0f);
    EXPECT_FALSE(sm.get_bool("nonexistent"));
}

} // namespace nexus::anim::tests
