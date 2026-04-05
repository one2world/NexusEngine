#include <gtest/gtest.h>
#include "nexus/perf/frame_graph.h"

using namespace nexus;

TEST(FrameGraph, AddResourcesAndPasses) {
    FrameGraph fg;

    u32 gbuffer = fg.add_resource("GBuffer", FrameResourceType::Framebuffer, 1920, 1080);
    u32 depth = fg.add_resource("Depth", FrameResourceType::Texture, 1920, 1080);

    u32 geo_pass = fg.add_pass("GeometryPass");
    u32 light_pass = fg.add_pass("LightingPass");

    EXPECT_EQ(fg.resource_count(), 2u);
    EXPECT_EQ(fg.pass_count(), 2u);
    EXPECT_EQ(fg.resource(gbuffer).name, "GBuffer");
    EXPECT_EQ(fg.resource(depth).name, "Depth");
    EXPECT_EQ(fg.pass(geo_pass).name, "GeometryPass");
    EXPECT_EQ(fg.pass(light_pass).name, "LightingPass");
}

TEST(FrameGraph, CompileLinearChain) {
    FrameGraph fg;

    u32 gbuffer = fg.add_resource("GBuffer", FrameResourceType::Framebuffer);
    u32 final_color = fg.add_resource("FinalColor", FrameResourceType::Texture);

    u32 geo = fg.add_pass("Geometry");
    fg.pass_writes(geo, gbuffer);
    fg.set_pass_execute(geo, []() {});

    u32 light = fg.add_pass("Lighting");
    fg.pass_reads(light, gbuffer);
    fg.pass_writes(light, final_color);
    fg.set_pass_execute(light, []() {});

    fg.mark_output(final_color);
    EXPECT_TRUE(fg.compile());

    const auto& order = fg.execution_order();
    EXPECT_EQ(order.size(), 2u);
    // Geometry must come before Lighting
    EXPECT_EQ(order[0], geo);
    EXPECT_EQ(order[1], light);
}

TEST(FrameGraph, CullUnusedPasses) {
    FrameGraph fg;

    u32 gbuffer = fg.add_resource("GBuffer", FrameResourceType::Framebuffer);
    u32 debug_out = fg.add_resource("DebugOut", FrameResourceType::Texture);
    u32 final_color = fg.add_resource("FinalColor", FrameResourceType::Texture);

    // Main path: geo -> lighting -> final
    u32 geo = fg.add_pass("Geometry");
    fg.pass_writes(geo, gbuffer);
    fg.set_pass_execute(geo, []() {});

    u32 light = fg.add_pass("Lighting");
    fg.pass_reads(light, gbuffer);
    fg.pass_writes(light, final_color);
    fg.set_pass_execute(light, []() {});

    // Unused path: debug pass writes debug_out, which nobody needs
    u32 debug = fg.add_pass("DebugPass");
    fg.pass_reads(debug, gbuffer);
    fg.pass_writes(debug, debug_out);
    fg.set_pass_execute(debug, []() {});

    fg.mark_output(final_color);
    EXPECT_TRUE(fg.compile());

    // Debug pass should be culled
    EXPECT_TRUE(fg.pass(debug).culled);
    EXPECT_EQ(fg.active_pass_count(), 2u);
    EXPECT_EQ(fg.execution_order().size(), 2u);
}

TEST(FrameGraph, ExecuteRunsInOrder) {
    FrameGraph fg;
    std::vector<int> exec_log;

    u32 res_a = fg.add_resource("A", FrameResourceType::Buffer);
    u32 res_b = fg.add_resource("B", FrameResourceType::Buffer);

    u32 p1 = fg.add_pass("Pass1");
    fg.pass_writes(p1, res_a);
    fg.set_pass_execute(p1, [&]() { exec_log.push_back(1); });

    u32 p2 = fg.add_pass("Pass2");
    fg.pass_reads(p2, res_a);
    fg.pass_writes(p2, res_b);
    fg.set_pass_execute(p2, [&]() { exec_log.push_back(2); });

    fg.mark_output(res_b);
    EXPECT_TRUE(fg.compile());
    fg.execute();

    ASSERT_EQ(exec_log.size(), 2u);
    EXPECT_EQ(exec_log[0], 1);
    EXPECT_EQ(exec_log[1], 2);
}

TEST(FrameGraph, ResetAllowsRecompile) {
    FrameGraph fg;
    u32 res = fg.add_resource("Out", FrameResourceType::Texture);
    u32 pass = fg.add_pass("Pass");
    fg.pass_writes(pass, res);
    fg.set_pass_execute(pass, []() {});
    fg.mark_output(res);

    EXPECT_TRUE(fg.compile());
    EXPECT_EQ(fg.execution_order().size(), 1u);

    fg.reset();
    EXPECT_TRUE(fg.execution_order().empty());

    EXPECT_TRUE(fg.compile());
    EXPECT_EQ(fg.execution_order().size(), 1u);
}

TEST(FrameGraph, ClearRemovesEverything) {
    FrameGraph fg;
    fg.add_resource("R", FrameResourceType::Texture);
    fg.add_pass("P");

    fg.clear();
    EXPECT_EQ(fg.pass_count(), 0u);
    EXPECT_EQ(fg.resource_count(), 0u);
}

TEST(FrameGraph, EmptyGraphCompiles) {
    FrameGraph fg;
    EXPECT_TRUE(fg.compile());
    EXPECT_TRUE(fg.execution_order().empty());
}

TEST(FrameGraph, DumpOutput) {
    FrameGraph fg;
    u32 res = fg.add_resource("Color", FrameResourceType::Texture, 1920, 1080);
    u32 pass = fg.add_pass("MainPass");
    fg.pass_writes(pass, res);
    fg.set_pass_execute(pass, []() {});
    fg.mark_output(res);
    fg.compile();

    std::string dump = fg.dump();
    EXPECT_NE(dump.find("Frame Graph"), std::string::npos);
    EXPECT_NE(dump.find("MainPass"), std::string::npos);
    EXPECT_NE(dump.find("Color"), std::string::npos);
    EXPECT_NE(dump.find("1920x1080"), std::string::npos);
}

TEST(FrameGraph, PassTimingRecorded) {
    FrameGraph fg;
    u32 res = fg.add_resource("Out", FrameResourceType::Texture);
    u32 pass = fg.add_pass("WorkPass");
    fg.pass_writes(pass, res);
    fg.set_pass_execute(pass, []() {
        // Do a tiny bit of work
        volatile int sum = 0;
        for (int i = 0; i < 1000; ++i) sum += i;
        (void)sum;
    });
    fg.mark_output(res);

    fg.compile();
    fg.execute();

    EXPECT_GE(fg.pass(pass).last_duration_us, 0.0);
    EXPECT_GE(fg.pass(pass).avg_duration_us, 0.0);
}

TEST(FrameGraph, DisabledPassSkipped) {
    FrameGraph fg;
    u32 res = fg.add_resource("Out", FrameResourceType::Texture);
    u32 pass = fg.add_pass("Disabled");
    fg.pass_writes(pass, res);
    fg.set_pass_execute(pass, []() {});
    fg.pass(pass).enabled = false;

    fg.mark_output(res);
    fg.compile();

    EXPECT_EQ(fg.active_pass_count(), 0u);
}

TEST(FrameGraph, ComplexDAG) {
    // Build a realistic render pipeline:
    //   Shadow -> GBuffer -> Lighting -> PostProcess -> Final
    //                    \-> SSAO -----/
    FrameGraph fg;

    u32 shadow_map = fg.add_resource("ShadowMap", FrameResourceType::Texture, 2048, 2048);
    u32 gbuffer = fg.add_resource("GBuffer", FrameResourceType::Framebuffer, 1920, 1080);
    u32 ssao_tex = fg.add_resource("SSAOTex", FrameResourceType::Texture, 960, 540);
    u32 lit_color = fg.add_resource("LitColor", FrameResourceType::Texture, 1920, 1080);
    u32 final_out = fg.add_resource("FinalOutput", FrameResourceType::Texture, 1920, 1080);

    u32 shadow_pass = fg.add_pass("ShadowPass");
    fg.pass_writes(shadow_pass, shadow_map);
    fg.set_pass_execute(shadow_pass, []() {});

    u32 gbuffer_pass = fg.add_pass("GBufferPass");
    fg.pass_writes(gbuffer_pass, gbuffer);
    fg.set_pass_execute(gbuffer_pass, []() {});

    u32 ssao_pass = fg.add_pass("SSAOPass");
    fg.pass_reads(ssao_pass, gbuffer);
    fg.pass_writes(ssao_pass, ssao_tex);
    fg.set_pass_execute(ssao_pass, []() {});

    u32 lighting_pass = fg.add_pass("LightingPass");
    fg.pass_reads(lighting_pass, gbuffer);
    fg.pass_reads(lighting_pass, shadow_map);
    fg.pass_reads(lighting_pass, ssao_tex);
    fg.pass_writes(lighting_pass, lit_color);
    fg.set_pass_execute(lighting_pass, []() {});

    u32 post_pass = fg.add_pass("PostProcess");
    fg.pass_reads(post_pass, lit_color);
    fg.pass_writes(post_pass, final_out);
    fg.set_pass_execute(post_pass, []() {});

    fg.mark_output(final_out);
    EXPECT_TRUE(fg.compile());

    EXPECT_EQ(fg.active_pass_count(), 5u);

    const auto& order = fg.execution_order();
    EXPECT_EQ(order.size(), 5u);

    // Verify topological ordering constraints
    auto index_of = [&](u32 id) -> u32 {
        for (u32 i = 0; i < static_cast<u32>(order.size()); ++i) {
            if (order[i] == id) return i;
        }
        return UINT32_MAX;
    };

    EXPECT_LT(index_of(shadow_pass), index_of(lighting_pass));
    EXPECT_LT(index_of(gbuffer_pass), index_of(ssao_pass));
    EXPECT_LT(index_of(gbuffer_pass), index_of(lighting_pass));
    EXPECT_LT(index_of(ssao_pass), index_of(lighting_pass));
    EXPECT_LT(index_of(lighting_pass), index_of(post_pass));
}
