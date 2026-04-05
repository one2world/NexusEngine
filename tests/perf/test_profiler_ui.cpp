#include <gtest/gtest.h>
#include "nexus/perf/memory_profiler_ui.h"
#include "nexus/perf/gpu_profiler.h"

using namespace nexus;

// ── Memory Profiler UI Tests ──────────────────────────────────────────────

TEST(MemoryProfilerUI, FormatBytes) {
    EXPECT_EQ(MemoryProfilerUI::format_bytes(512), "512 B");
    EXPECT_EQ(MemoryProfilerUI::format_bytes(1024), "1.0 KB");
    EXPECT_EQ(MemoryProfilerUI::format_bytes(1536), "1.5 KB");
    EXPECT_EQ(MemoryProfilerUI::format_bytes(1048576), "1.0 MB");
    EXPECT_EQ(MemoryProfilerUI::format_bytes(1073741824), "1.00 GB");
}

TEST(MemoryProfilerUI, UpdateSnapshot) {
    MemoryTracker tracker;
    int dummy1 = 0;
    int dummy2 = 0;
    tracker.record_alloc(&dummy1, 1024, "textures");
    tracker.record_alloc(&dummy2, 2048, "meshes");

    MemoryProfilerUI ui(tracker);
    ui.update();

    const auto& snap = ui.snapshot();
    EXPECT_EQ(snap.total_current, static_cast<std::size_t>(3072));
    EXPECT_EQ(snap.total_live, static_cast<u64>(2));
    EXPECT_EQ(snap.categories.size(), static_cast<std::size_t>(2));
}

TEST(MemoryProfilerUI, History) {
    MemoryTracker tracker;
    MemoryProfilerUI ui(tracker);

    for (u32 i = 0; i < 5; ++i) {
        ui.update();
    }

    EXPECT_EQ(ui.history().size(), static_cast<std::size_t>(5));
}

// ── GPU Profiler Tests ────────────────────────────────────────────────────

TEST(GPUProfiler, BasicFrame) {
    GPUProfiler profiler;

    profiler.begin_frame();
    profiler.record_timing("ShadowPass", 500.0);
    profiler.record_timing("GBufferPass", 1000.0);
    profiler.record_timing("LightingPass", 800.0);
    profiler.end_frame();

    const auto& summary = profiler.summary();
    EXPECT_EQ(summary.passes.size(), static_cast<std::size_t>(3));
    EXPECT_GT(summary.total_gpu_ms, 0.0);
}

TEST(GPUProfiler, MultipleFrames) {
    GPUProfiler profiler;

    for (u32 i = 0; i < 10; ++i) {
        profiler.begin_frame();
        profiler.record_timing("MainPass", 1000.0);
        profiler.end_frame();
    }

    EXPECT_EQ(profiler.history().size(), static_cast<std::size_t>(10));
    EXPECT_GT(profiler.summary().avg_gpu_ms, 0.0);
}

TEST(GPUProfiler, Disabled) {
    GPUProfiler profiler;
    profiler.set_enabled(false);

    profiler.begin_frame();
    profiler.record_timing("Pass", 1000.0);
    profiler.end_frame();

    EXPECT_EQ(profiler.summary().passes.size(), static_cast<std::size_t>(0));
}

TEST(GPUProfiler, NestedPasses) {
    GPUProfiler profiler;

    profiler.begin_frame();
    profiler.begin_pass("Outer");
    profiler.record_timing("Inner1", 500.0);
    profiler.begin_pass("SubPass");
    profiler.record_timing("Inner2", 300.0);
    profiler.end_pass();
    profiler.end_pass();
    profiler.end_frame();

    const auto& passes = profiler.summary().passes;
    EXPECT_GE(passes.size(), static_cast<std::size_t>(2));
}

TEST(GPUProfiler, HistoryLimit) {
    GPUProfiler profiler;

    for (u32 i = 0; i < 200; ++i) {
        profiler.begin_frame();
        profiler.record_timing("Pass", 100.0);
        profiler.end_frame();
    }

    EXPECT_LE(profiler.history().size(), static_cast<std::size_t>(GPUProfiler::MAX_HISTORY));
}
