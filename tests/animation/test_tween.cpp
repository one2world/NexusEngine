#include <gtest/gtest.h>
#include <nexus/animation/tween.h>
#include <cmath>

namespace nexus::anim::tests {

TEST(Ease, LinearBoundaries) {
    EXPECT_FLOAT_EQ(ease::linear(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease::linear(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ease::linear(0.5f), 0.5f);
}

TEST(Ease, QuadBoundaries) {
    EXPECT_FLOAT_EQ(ease::in_quad(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease::in_quad(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ease::out_quad(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease::out_quad(1.0f), 1.0f);
}

TEST(Ease, CubicBoundaries) {
    EXPECT_FLOAT_EQ(ease::in_cubic(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease::in_cubic(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ease::out_cubic(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease::out_cubic(1.0f), 1.0f);
}

TEST(Ease, SineBoundaries) {
    EXPECT_NEAR(ease::in_sine(0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(ease::in_sine(1.0f), 1.0f, 1e-5f);
    EXPECT_NEAR(ease::out_sine(0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(ease::out_sine(1.0f), 1.0f, 1e-5f);
}

TEST(Ease, BounceBoundaries) {
    EXPECT_NEAR(ease::out_bounce(0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(ease::out_bounce(1.0f), 1.0f, 1e-5f);
    EXPECT_NEAR(ease::in_bounce(0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(ease::in_bounce(1.0f), 1.0f, 1e-5f);
}

TEST(Ease, BackOvershoot) {
    // in_back at t=0.5 should be negative (overshooting backward)
    EXPECT_LT(ease::in_back(0.3f), 0.0f);
    // out_back should overshoot past 1.0
    EXPECT_GT(ease::out_back(0.7f), 1.0f);
}

TEST(Tween, AnimatesToTarget) {
    float value = 0.0f;
    Tween tween(&value, 0.0f, 10.0f, 1.0f, ease::linear);

    tween.update(0.5f);
    EXPECT_NEAR(value, 5.0f, 0.01f);

    tween.update(0.5f);
    EXPECT_NEAR(value, 10.0f, 0.01f);
    EXPECT_TRUE(tween.finished());
}

TEST(Tween, EasingApplied) {
    float value = 0.0f;
    Tween tween(&value, 0.0f, 1.0f, 1.0f, ease::in_quad);

    tween.update(0.5f);
    // in_quad at 0.5 = 0.25
    EXPECT_NEAR(value, 0.25f, 0.01f);
}

TEST(Tween, OnCompleteCallback) {
    float value = 0.0f;
    bool completed = false;

    Tween tween(&value, 0.0f, 1.0f, 0.5f);
    tween.on_complete([&]() { completed = true; });

    tween.update(0.6f);
    EXPECT_TRUE(completed);
}

TEST(Tween, Delay) {
    float value = 0.0f;
    Tween tween(&value, 0.0f, 1.0f, 1.0f);
    tween.set_delay(0.5f);

    tween.update(0.3f); // still in delay
    EXPECT_FLOAT_EQ(value, 0.0f);

    tween.update(0.7f); // 0.5s effective time
    EXPECT_NEAR(value, 0.5f, 0.01f);
}

TEST(TweenManager, ManagesMultipleTweens) {
    float a = 0.0f, b = 0.0f;
    TweenManager mgr;

    mgr.add(&a, 0.0f, 10.0f, 1.0f);
    mgr.add(&b, 0.0f, 20.0f, 2.0f);

    EXPECT_EQ(mgr.active_count(), 2u);

    mgr.update(1.0f);
    EXPECT_NEAR(a, 10.0f, 0.01f);
    EXPECT_EQ(mgr.active_count(), 1u); // a finished

    mgr.update(1.0f);
    EXPECT_NEAR(b, 20.0f, 0.01f);
    EXPECT_EQ(mgr.active_count(), 0u);
}

} // namespace nexus::anim::tests
