#include <gtest/gtest.h>
#include <nexus/animation/animation_clip.h>
#include <vector>
#include <string>

namespace nexus::anim::tests {

TEST(AnimationEvents, AddAndRetrieveEvents) {
    AnimationClip clip("test", 2.0f);
    clip.add_event(0.5f, "footstep");
    clip.add_event(1.0f, "attack");
    clip.add_event(1.5f, "footstep");

    EXPECT_EQ(clip.events().size(), 3u);
    // Events should be sorted by time
    EXPECT_FLOAT_EQ(clip.events()[0].time, 0.5f);
    EXPECT_FLOAT_EQ(clip.events()[1].time, 1.0f);
    EXPECT_FLOAT_EQ(clip.events()[2].time, 1.5f);
}

TEST(AnimationEvents, CollectEventsForward) {
    AnimationClip clip("test", 2.0f);
    clip.add_event(0.3f, "a");
    clip.add_event(0.7f, "b");
    clip.add_event(1.5f, "c");

    std::vector<const AnimationEvent*> fired;
    clip.collect_events(0.0f, 1.0f, false, fired);

    ASSERT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[0]->name, "a");
    EXPECT_EQ(fired[1]->name, "b");
}

TEST(AnimationEvents, CollectEventsNoneInRange) {
    AnimationClip clip("test", 2.0f);
    clip.add_event(0.5f, "x");

    std::vector<const AnimationEvent*> fired;
    clip.collect_events(0.6f, 1.0f, false, fired);

    EXPECT_TRUE(fired.empty());
}

TEST(AnimationEvents, CollectEventsLoopWrap) {
    AnimationClip clip("test", 2.0f);
    clip.add_event(0.2f, "early");
    clip.add_event(1.8f, "late");

    std::vector<const AnimationEvent*> fired;
    // Looping: time wraps from 1.9 back to 0.3
    clip.collect_events(1.9f, 0.3f, true, fired);

    // Should fire "late" (1.8 is in [1.9, 2.0) — no, 1.8 < 1.9)
    // Actually: wrapping fires events in [1.9, duration=2.0) + [0, 0.3)
    // 1.8 is NOT in [1.9, 2.0), "early" at 0.2 IS in [0, 0.3)
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->name, "early");
}

TEST(AnimationEvents, CollectEventsExactTime) {
    AnimationClip clip("test", 1.0f);
    clip.add_event(0.5f, "exact");

    std::vector<const AnimationEvent*> fired;
    // collect_events uses (prev_time, curr_time] — strictly greater than prev
    // So when prev_time == event.time, event is NOT fired
    clip.collect_events(0.5f, 0.6f, false, fired);
    EXPECT_EQ(fired.size(), 0u);

    // But when prev_time < event.time <= curr_time, it fires
    fired.clear();
    clip.collect_events(0.4f, 0.5f, false, fired);
    EXPECT_EQ(fired.size(), 1u);
}

TEST(AnimationEvents, EmptyClipNoEvents) {
    AnimationClip clip("test", 1.0f);
    // No events added

    std::vector<const AnimationEvent*> fired;
    clip.collect_events(0.0f, 1.0f, false, fired);
    EXPECT_TRUE(fired.empty());
}

TEST(AnimationEvents, EventsSortedAfterInsert) {
    AnimationClip clip("test", 3.0f);
    clip.add_event(2.0f, "second");
    clip.add_event(0.5f, "first");
    clip.add_event(1.0f, "middle");

    EXPECT_EQ(clip.events()[0].name, "first");
    EXPECT_EQ(clip.events()[1].name, "middle");
    EXPECT_EQ(clip.events()[2].name, "second");
}

} // namespace nexus::anim::tests
