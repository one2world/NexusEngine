#include <gtest/gtest.h>

#include "nexus/animation/animation_clip.h"

namespace nexus::anim::tests {

// ── ensure_channel ──────────────────────────────────────────────────────────

TEST(AnimationClipEdit, EnsureChannelCreatesWhenAbsent) {
    AnimationClip clip("c", 1.0f);
    EXPECT_EQ(clip.channels().size(), 0u);
    auto& ch = clip.ensure_channel(7);
    EXPECT_EQ(clip.channels().size(), 1u);
    EXPECT_EQ(ch.bone_index, 7);
}

TEST(AnimationClipEdit, EnsureChannelReusesExisting) {
    AnimationClip clip("c", 1.0f);
    clip.ensure_channel(3);
    auto& ch_again = clip.ensure_channel(3);
    EXPECT_EQ(clip.channels().size(), 1u);
    EXPECT_EQ(ch_again.bone_index, 3);
}

TEST(AnimationClipEdit, EnsureChannelClampsNegativeBoneIndex) {
    AnimationClip clip("c", 1.0f);
    auto& ch = clip.ensure_channel(-5);
    EXPECT_EQ(ch.bone_index, 0);
}

// ── add_*_key ───────────────────────────────────────────────────────────────

TEST(AnimationClipEdit, AddPositionKeyMaintainsSortedOrder) {
    AnimationClip clip("c", 0.0f);
    clip.add_position_key(0, 0.5f, Vec3(2.0f, 0, 0));
    clip.add_position_key(0, 0.1f, Vec3(0.5f, 0, 0));
    clip.add_position_key(0, 0.3f, Vec3(1.0f, 0, 0));
    const auto& ch = clip.channels()[0];
    ASSERT_EQ(ch.positions.size(), 3u);
    EXPECT_FLOAT_EQ(ch.positions[0].time, 0.1f);
    EXPECT_FLOAT_EQ(ch.positions[1].time, 0.3f);
    EXPECT_FLOAT_EQ(ch.positions[2].time, 0.5f);
}

TEST(AnimationClipEdit, AddKeyExtendsDurationWhenPastCurrentEnd) {
    AnimationClip clip("c", 1.0f);
    clip.add_position_key(0, 2.5f, Vec3(0));
    EXPECT_NEAR(clip.duration(), 2.5f, 1e-5f);
}

TEST(AnimationClipEdit, AddKeyDoesNotShrinkDuration) {
    AnimationClip clip("c", 5.0f);
    clip.add_position_key(0, 1.0f, Vec3(0));
    EXPECT_NEAR(clip.duration(), 5.0f, 1e-5f);
}

TEST(AnimationClipEdit, AddNegativeTimeClampsToZero) {
    AnimationClip clip("c", 1.0f);
    clip.add_position_key(0, -0.5f, Vec3(7));
    const auto& ch = clip.channels()[0];
    ASSERT_EQ(ch.positions.size(), 1u);
    EXPECT_FLOAT_EQ(ch.positions[0].time, 0.0f);
    EXPECT_FLOAT_EQ(ch.positions[0].value.x, 7.0f);
}

TEST(AnimationClipEdit, AddRotationKeySorts) {
    AnimationClip clip("c", 0.0f);
    clip.add_rotation_key(0, 1.0f, Quat(1, 0, 0, 0));
    clip.add_rotation_key(0, 0.5f, Quat(1, 0, 0, 0));
    const auto& ch = clip.channels()[0];
    EXPECT_FLOAT_EQ(ch.rotations[0].time, 0.5f);
    EXPECT_FLOAT_EQ(ch.rotations[1].time, 1.0f);
}

TEST(AnimationClipEdit, AddScaleKeySortsAndExtendsDuration) {
    AnimationClip clip("c", 0.5f);
    clip.add_scale_key(0, 1.5f, Vec3(2));
    clip.add_scale_key(0, 0.0f, Vec3(1));
    const auto& ch = clip.channels()[0];
    EXPECT_FLOAT_EQ(ch.scales[0].time, 0.0f);
    EXPECT_FLOAT_EQ(ch.scales[1].time, 1.5f);
    EXPECT_NEAR(clip.duration(), 1.5f, 1e-5f);
}

TEST(AnimationClipEdit, AddKeyToMissingBoneCreatesChannel) {
    AnimationClip clip("c", 0.0f);
    clip.add_position_key(2, 0.0f, Vec3(0));
    EXPECT_EQ(clip.channels().size(), 1u);
    EXPECT_EQ(clip.channels()[0].bone_index, 2);
}

// ── remove_*_key ────────────────────────────────────────────────────────────

TEST(AnimationClipEdit, RemovePositionKeyByIndex) {
    AnimationClip clip("c", 0.0f);
    clip.add_position_key(0, 0.0f, Vec3(0));
    clip.add_position_key(0, 1.0f, Vec3(1));
    clip.add_position_key(0, 2.0f, Vec3(2));
    EXPECT_TRUE(clip.remove_position_key(0, 1));  // remove middle
    const auto& ch = clip.channels()[0];
    ASSERT_EQ(ch.positions.size(), 2u);
    EXPECT_FLOAT_EQ(ch.positions[0].time, 0.0f);
    EXPECT_FLOAT_EQ(ch.positions[1].time, 2.0f);
}

TEST(AnimationClipEdit, RemoveOutOfRangeIsNoop) {
    AnimationClip clip("c", 0.0f);
    clip.add_position_key(0, 0.0f, Vec3(0));
    EXPECT_FALSE(clip.remove_position_key(0, 99));
    EXPECT_EQ(clip.channels()[0].positions.size(), 1u);
}

TEST(AnimationClipEdit, RemoveOnUnknownBoneReturnsFalse) {
    AnimationClip clip("c", 0.0f);
    EXPECT_FALSE(clip.remove_position_key(99, 0));
    EXPECT_FALSE(clip.remove_rotation_key(99, 0));
    EXPECT_FALSE(clip.remove_scale_key(99, 0));
}

// ── set_name / set_duration ─────────────────────────────────────────────────

TEST(AnimationClipEdit, SetNameUpdatesAccessor) {
    AnimationClip clip("orig", 1.0f);
    clip.set_name("renamed");
    EXPECT_EQ(clip.name(), "renamed");
}

TEST(AnimationClipEdit, SetDurationClampsNegativeToZero) {
    AnimationClip clip("c", 1.0f);
    clip.set_duration(-2.0f);
    EXPECT_FLOAT_EQ(clip.duration(), 0.0f);
    clip.set_duration(3.5f);
    EXPECT_FLOAT_EQ(clip.duration(), 3.5f);
}

}  // namespace nexus::anim::tests
