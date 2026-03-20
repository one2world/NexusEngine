#include <nexus/perf/profiler.h>
#include <gtest/gtest.h>
#include <thread>

using namespace nexus;

TEST(Profiler, InitialState) {
    Profiler p;
    EXPECT_TRUE(p.is_active());
    EXPECT_TRUE(p.history().empty());
    EXPECT_EQ(p.last_frame(), nullptr);
    EXPECT_DOUBLE_EQ(p.average_cpu_us(), 0.0);
    EXPECT_DOUBLE_EQ(p.average_gpu_us(), 0.0);
}

TEST(Profiler, BeginEndFrame) {
    Profiler p;
    p.begin_frame();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    p.end_frame();

    EXPECT_EQ(p.history().size(), 1u);
    auto* last = p.last_frame();
    ASSERT_NE(last, nullptr);
    EXPECT_GT(last->total_cpu_us, 0.0);
    EXPECT_EQ(last->frame_number, 0u);
}

TEST(Profiler, MultipleFrames) {
    Profiler p;
    for (int i = 0; i < 5; ++i) {
        p.begin_frame();
        p.end_frame();
    }
    EXPECT_EQ(p.history().size(), 5u);
    EXPECT_EQ(p.last_frame()->frame_number, 4u);
}

TEST(Profiler, MaxHistory) {
    Profiler p;
    for (u32 i = 0; i < Profiler::MAX_HISTORY + 10; ++i) {
        p.begin_frame();
        p.end_frame();
    }
    EXPECT_EQ(p.history().size(), Profiler::MAX_HISTORY);
}

TEST(Profiler, CPUScopes) {
    Profiler p;
    p.begin_frame();
    p.begin_scope("Outer");
    p.begin_scope("Inner");
    p.end_scope();
    p.end_scope();
    p.end_frame();

    auto* last = p.last_frame();
    ASSERT_NE(last, nullptr);
    ASSERT_EQ(last->cpu_samples.size(), 2u);
    EXPECT_EQ(last->cpu_samples[0].name, "Outer");
    EXPECT_EQ(last->cpu_samples[0].depth, 0u);
    EXPECT_EQ(last->cpu_samples[1].name, "Inner");
    EXPECT_EQ(last->cpu_samples[1].depth, 1u);
    EXPECT_GT(last->cpu_samples[0].duration_us, 0.0);
    EXPECT_GT(last->cpu_samples[1].duration_us, 0.0);
}

TEST(Profiler, GPUTimestamps) {
    Profiler p;
    p.begin_frame();
    p.record_gpu_time("ShadowPass", 500.0);
    p.record_gpu_time("LightPass", 300.0);
    p.end_frame();

    auto* last = p.last_frame();
    ASSERT_NE(last, nullptr);
    ASSERT_EQ(last->gpu_timestamps.size(), 2u);
    EXPECT_EQ(last->gpu_timestamps[0].name, "ShadowPass");
    EXPECT_DOUBLE_EQ(last->gpu_timestamps[0].duration_us, 500.0);
    EXPECT_DOUBLE_EQ(last->total_gpu_us, 800.0);
}

TEST(Profiler, AverageTimes) {
    Profiler p;
    for (int i = 0; i < 3; ++i) {
        p.begin_frame();
        p.record_gpu_time("Pass", 100.0);
        p.end_frame();
    }
    EXPECT_GT(p.average_cpu_us(), 0.0);
    EXPECT_DOUBLE_EQ(p.average_gpu_us(), 100.0);
}

TEST(Profiler, SetInactive) {
    Profiler p;
    p.set_active(false);
    EXPECT_FALSE(p.is_active());
    p.begin_frame();
    p.begin_scope("Test");
    p.end_scope();
    p.end_frame();
    // Nothing should be recorded when inactive.
    EXPECT_TRUE(p.history().empty());
}

TEST(ScopedProfile, RAIIScope) {
    Profiler p;
    p.begin_frame();
    {
        ScopedProfile sp(p, "Scoped");
    }
    p.end_frame();

    auto* last = p.last_frame();
    ASSERT_NE(last, nullptr);
    ASSERT_EQ(last->cpu_samples.size(), 1u);
    EXPECT_EQ(last->cpu_samples[0].name, "Scoped");
    EXPECT_GT(last->cpu_samples[0].duration_us, 0.0);
}
