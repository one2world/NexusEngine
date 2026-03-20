#include <nexus/perf/render_stats.h>
#include <gtest/gtest.h>

using namespace nexus;

TEST(RenderStats, InitialState) {
    RenderStatsCollector rsc;
    EXPECT_EQ(rsc.current().draw_calls, 0u);
    EXPECT_EQ(rsc.last_frame(), nullptr);
    EXPECT_TRUE(rsc.history().empty());
}

TEST(RenderStats, AccumulateDrawCalls) {
    RenderStatsCollector rsc;
    rsc.begin_frame();
    rsc.add_draw_call(100, 300);
    rsc.add_draw_call(200, 600);
    rsc.end_frame(1.0f / 60.0f);

    auto* last = rsc.last_frame();
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->draw_calls, 2u);
    EXPECT_EQ(last->triangles, 300u);
    EXPECT_EQ(last->vertices, 900u);
}

TEST(RenderStats, TextureAndShaderBinds) {
    RenderStatsCollector rsc;
    rsc.begin_frame();
    rsc.add_texture_bind();
    rsc.add_texture_bind();
    rsc.add_shader_switch();
    rsc.end_frame(1.0f / 60.0f);

    auto* last = rsc.last_frame();
    EXPECT_EQ(last->texture_binds, 2u);
    EXPECT_EQ(last->shader_switches, 1u);
}

TEST(RenderStats, GPUMemory) {
    RenderStatsCollector rsc;
    rsc.begin_frame();
    rsc.set_gpu_memory(512 * 1024 * 1024ULL);
    rsc.end_frame(1.0f / 60.0f);

    auto* last = rsc.last_frame();
    EXPECT_EQ(last->gpu_memory_used, 512u * 1024 * 1024);
}

TEST(RenderStats, FPSAndFrameTime) {
    RenderStatsCollector rsc;
    rsc.begin_frame();
    rsc.end_frame(1.0f / 60.0f);

    auto* last = rsc.last_frame();
    EXPECT_NEAR(last->fps, 60.0f, 0.5f);
    EXPECT_NEAR(last->frame_time_ms, 16.666f, 0.1f);
}

TEST(RenderStats, MaxHistory) {
    RenderStatsCollector rsc;
    for (u32 i = 0; i < RenderStatsCollector::MAX_HISTORY + 10; ++i) {
        rsc.begin_frame();
        rsc.end_frame(1.0f / 60.0f);
    }
    EXPECT_EQ(rsc.history().size(), RenderStatsCollector::MAX_HISTORY);
}

TEST(RenderStats, Average) {
    RenderStatsCollector rsc;
    for (int i = 0; i < 3; ++i) {
        rsc.begin_frame();
        rsc.add_draw_call(100, 300);
        rsc.end_frame(1.0f / 60.0f);
    }

    auto avg = rsc.average();
    EXPECT_EQ(avg.draw_calls, 1u);
    EXPECT_EQ(avg.triangles, 100u);
    EXPECT_NEAR(avg.fps, 60.0f, 0.5f);
}

TEST(RenderStats, FormatOverlay) {
    RenderStatsCollector rsc;
    rsc.begin_frame();
    rsc.add_draw_call(50, 150);
    rsc.set_gpu_memory(1024 * 1024);
    rsc.end_frame(1.0f / 60.0f);

    auto overlay = rsc.format_overlay();
    EXPECT_FALSE(overlay.empty());
    EXPECT_NE(overlay.find("FPS"), std::string::npos);
    EXPECT_NE(overlay.find("DC"), std::string::npos);
}

TEST(RenderStats, ResetOnBeginFrame) {
    RenderStatsCollector rsc;
    rsc.begin_frame();
    rsc.add_draw_call(100, 300);
    rsc.begin_frame(); // should reset
    EXPECT_EQ(rsc.current().draw_calls, 0u);
    EXPECT_EQ(rsc.current().triangles, 0u);
}

TEST(RenderStats, NoFrameFormatOverlay) {
    RenderStatsCollector rsc;
    auto overlay = rsc.format_overlay();
    EXPECT_EQ(overlay, "No render stats");
}
