#include <nexus/perf/memory_tracker.h>
#include <gtest/gtest.h>

using namespace nexus;

TEST(MemoryTracker, InitialState) {
    MemoryTracker mt;
    EXPECT_EQ(mt.total_allocated(), 0u);
    EXPECT_EQ(mt.total_peak(), 0u);
    EXPECT_EQ(mt.live_count(), 0u);
    EXPECT_TRUE(mt.live_allocations().empty());
    EXPECT_TRUE(mt.all_stats().empty());
}

TEST(MemoryTracker, RecordAllocFree) {
    MemoryTracker mt;
    int dummy = 0;
    mt.record_alloc(&dummy, 1024, "gpu");

    EXPECT_EQ(mt.total_allocated(), 1024u);
    EXPECT_EQ(mt.live_count(), 1u);

    mt.record_free(&dummy);
    EXPECT_EQ(mt.total_allocated(), 0u);
    EXPECT_EQ(mt.live_count(), 0u);
}

TEST(MemoryTracker, PeakTracking) {
    MemoryTracker mt;
    int a = 0, b = 0;
    mt.record_alloc(&a, 500, "test");
    mt.record_alloc(&b, 300, "test");
    EXPECT_EQ(mt.total_allocated(), 800u);
    EXPECT_EQ(mt.total_peak(), 800u);

    mt.record_free(&a);
    EXPECT_EQ(mt.total_allocated(), 300u);
    // Peak should still be 800.
    EXPECT_EQ(mt.total_peak(), 800u);
}

TEST(MemoryTracker, MultipleTags) {
    MemoryTracker mt;
    int a = 0, b = 0;
    mt.record_alloc(&a, 100, "gpu");
    mt.record_alloc(&b, 200, "cpu");

    auto gpu = mt.stats_for("gpu");
    EXPECT_EQ(gpu.current_bytes, 100u);
    EXPECT_EQ(gpu.total_allocations, 1u);

    auto cpu = mt.stats_for("cpu");
    EXPECT_EQ(cpu.current_bytes, 200u);

    EXPECT_EQ(mt.total_allocated(), 300u);
}

TEST(MemoryTracker, AllStats) {
    MemoryTracker mt;
    int a = 0, b = 0;
    mt.record_alloc(&a, 100, "gpu");
    mt.record_alloc(&b, 200, "cpu");

    auto all = mt.all_stats();
    EXPECT_EQ(all.size(), 2u);
}

TEST(MemoryTracker, LiveAllocations) {
    MemoryTracker mt;
    int a = 0, b = 0;
    mt.record_alloc(&a, 64, "mesh");
    mt.record_alloc(&b, 128, "texture");

    auto live = mt.live_allocations();
    EXPECT_EQ(live.size(), 2u);

    mt.record_free(&a);
    live = mt.live_allocations();
    EXPECT_EQ(live.size(), 1u);
    EXPECT_EQ(live[0].tag, "texture");
}

TEST(MemoryTracker, FreeUnknownPtr) {
    MemoryTracker mt;
    int x = 0;
    mt.record_free(&x); // Should not crash.
    EXPECT_EQ(mt.live_count(), 0u);
}

TEST(MemoryTracker, NullPtrIgnored) {
    MemoryTracker mt;
    mt.record_alloc(nullptr, 100, "test");
    EXPECT_EQ(mt.live_count(), 0u);
    mt.record_free(nullptr);
    EXPECT_EQ(mt.live_count(), 0u);
}

TEST(MemoryTracker, Reset) {
    MemoryTracker mt;
    int a = 0;
    mt.record_alloc(&a, 100, "test");
    mt.reset();
    EXPECT_EQ(mt.total_allocated(), 0u);
    EXPECT_EQ(mt.live_count(), 0u);
    EXPECT_TRUE(mt.all_stats().empty());
}

TEST(MemoryTracker, DeallocationCount) {
    MemoryTracker mt;
    int a = 0;
    mt.record_alloc(&a, 100, "test");
    mt.record_free(&a);

    auto s = mt.stats_for("test");
    EXPECT_EQ(s.total_allocations, 1u);
    EXPECT_EQ(s.total_deallocations, 1u);
}

TEST(MemoryTracker, DefaultTag) {
    MemoryTracker mt;
    int a = 0;
    mt.record_alloc(&a, 50);

    auto s = mt.stats_for("default");
    EXPECT_EQ(s.current_bytes, 50u);
}
