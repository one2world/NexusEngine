#include <gtest/gtest.h>
#include <nexus/animation/sprite_animation.h>

namespace nexus::anim::tests {

static SpriteAnimation make_4_frame_anim(bool looping = true) {
    SpriteAnimation anim;
    anim.name = "walk";
    anim.looping = looping;
    for (int i = 0; i < 4; ++i) {
        SpriteFrame f;
        float x = static_cast<float>(i) * 0.25f;
        f.uv_min = {x, 0.0f};
        f.uv_max = {x + 0.25f, 1.0f};
        f.duration = 0.1f;
        anim.frames.push_back(f);
    }
    return anim;
}

TEST(SpriteAnimator, PlaySetsCurrentAnimation) {
    SpriteAnimator animator;
    animator.add_animation("walk", make_4_frame_anim());
    animator.play("walk");
    EXPECT_EQ(animator.current_animation(), "walk");
}

TEST(SpriteAnimator, AdvancesFrames) {
    SpriteAnimator animator;
    animator.add_animation("walk", make_4_frame_anim());
    animator.play("walk");

    EXPECT_EQ(animator.current_frame_index(), 0u);

    animator.update(0.1f); // advance to frame 1
    EXPECT_EQ(animator.current_frame_index(), 1u);

    animator.update(0.1f); // frame 2
    EXPECT_EQ(animator.current_frame_index(), 2u);
}

TEST(SpriteAnimator, LoopingWraps) {
    SpriteAnimator animator;
    animator.add_animation("walk", make_4_frame_anim(true));
    animator.play("walk");

    // Advance past all 4 frames (0.4s total)
    animator.update(0.45f);
    // Should have wrapped around
    EXPECT_LT(animator.current_frame_index(), 4u);
    EXPECT_FALSE(animator.finished());
}

TEST(SpriteAnimator, NonLoopingFinishes) {
    SpriteAnimator animator;
    animator.add_animation("die", make_4_frame_anim(false));
    animator.play("die");

    animator.update(0.5f); // past all frames
    EXPECT_TRUE(animator.finished());
    EXPECT_EQ(animator.current_frame_index(), 3u); // stays on last frame
}

TEST(SpriteAnimator, SpeedMultiplier) {
    SpriteAnimator animator;
    animator.add_animation("walk", make_4_frame_anim());
    animator.play("walk");
    animator.set_speed(2.0f);

    animator.update(0.05f); // effective: 0.1s
    EXPECT_EQ(animator.current_frame_index(), 1u);
}

TEST(SpriteAnimator, FromSheet) {
    auto anim = SpriteAnimator::from_sheet("run", 4, 2, 8, 12.0f);
    EXPECT_EQ(anim.frames.size(), 8u);
    EXPECT_NEAR(anim.frames[0].uv_min.x, 0.0f, 1e-5f);
    EXPECT_NEAR(anim.frames[0].uv_max.x, 0.25f, 1e-5f);
    EXPECT_NEAR(anim.frames[4].uv_min.y, 0.5f, 1e-5f);
}

TEST(SpriteAnimator, UVCoordinatesUpdate) {
    SpriteAnimator animator;
    animator.add_animation("walk", make_4_frame_anim());
    animator.play("walk");

    Vec2 uv0 = animator.current_uv_min();
    EXPECT_NEAR(uv0.x, 0.0f, 1e-5f);

    animator.update(0.1f);
    Vec2 uv1 = animator.current_uv_min();
    EXPECT_NEAR(uv1.x, 0.25f, 1e-5f);
}

} // namespace nexus::anim::tests
