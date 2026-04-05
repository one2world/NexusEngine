#include <gtest/gtest.h>
#include "nexus/platform/touch_input.h"

using namespace nexus;

TEST(TouchInput, InitialState) {
    TouchInput input;
    EXPECT_EQ(input.active_touch_count(), 0u);
    EXPECT_FALSE(input.is_touching());
}

TEST(TouchInput, SingleTouch) {
    TouchInput input;

    input.on_touch_begin(0, Vec2(100.0f, 200.0f));
    EXPECT_EQ(input.active_touch_count(), 1u);
    EXPECT_TRUE(input.is_touching());

    input.on_touch_move(0, Vec2(110.0f, 210.0f));
    input.update(0.016);

    input.on_touch_end(0, Vec2(110.0f, 210.0f));
    EXPECT_EQ(input.active_touch_count(), 0u);
    EXPECT_FALSE(input.is_touching());
}

TEST(TouchInput, MultiTouch) {
    TouchInput input;

    input.on_touch_begin(0, Vec2(100.0f, 100.0f));
    input.on_touch_begin(1, Vec2(200.0f, 200.0f));
    EXPECT_EQ(input.active_touch_count(), 2u);

    input.on_touch_end(0, Vec2(100.0f, 100.0f));
    EXPECT_EQ(input.active_touch_count(), 1u);

    input.on_touch_end(1, Vec2(200.0f, 200.0f));
    EXPECT_EQ(input.active_touch_count(), 0u);
}

TEST(TouchInput, TouchCancel) {
    TouchInput input;

    input.on_touch_begin(0, Vec2(50.0f, 50.0f));
    EXPECT_EQ(input.active_touch_count(), 1u);

    input.on_touch_cancel(0);
    EXPECT_EQ(input.active_touch_count(), 0u);
}

TEST(TouchInput, MaxTouchPoints) {
    TouchInput input;

    for (u32 i = 0; i < MAX_TOUCH_POINTS; ++i) {
        input.on_touch_begin(i, Vec2(static_cast<float>(i) * 10.0f, 0.0f));
    }
    EXPECT_EQ(input.active_touch_count(), MAX_TOUCH_POINTS);

    // One more should be ignored
    input.on_touch_begin(99, Vec2(999.0f, 999.0f));
    EXPECT_EQ(input.active_touch_count(), MAX_TOUCH_POINTS);
}

TEST(TouchInput, GestureTap) {
    TouchInput input;
    input.set_tap_threshold(20.0f);

    // First update at time 1.0 to initialize timestamps away from 0
    input.update(1.0);

    input.on_touch_begin(0, Vec2(100.0f, 100.0f));
    input.update(1.01);

    // End very close to start, within short time
    input.on_touch_end(0, Vec2(101.0f, 101.0f));
    input.update(1.1);

    EXPECT_TRUE(input.gestures().tapped);
    EXPECT_EQ(input.gestures().tap_count, 1u);
}

TEST(TouchInput, GesturePan) {
    TouchInput input;

    input.on_touch_begin(0, Vec2(100.0f, 100.0f));
    input.update(0.016);

    input.on_touch_move(0, Vec2(150.0f, 150.0f));
    input.update(0.032);

    EXPECT_TRUE(input.gestures().panning);
}
