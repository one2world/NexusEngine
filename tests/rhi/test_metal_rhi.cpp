// ============================================================================
// test_metal_rhi.cpp — MetalRHI backend tests.
//
// These tests run only on hosts where Metal frameworks are available and the
// build system enabled the backend (NEXUS_ENABLE_METAL && NEXUS_HAVE_METAL).
// They exercise the real MTLDevice / MTLCommandQueue acquisition path plus
// pipeline-state compilation and command-buffer / render-encoder encoding;
// there is no CPU-simulated fallback to cover.
// ============================================================================

#include <gtest/gtest.h>
#include "nexus/rhi/metal_rhi.h"
#include "nexus/rhi/rhi.h"

using namespace nexus;
using namespace nexus::rhi;

TEST(MetalRHI, AcquiresRealDevice) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init()) << "Expected MTLCreateSystemDefaultDevice to succeed";
    EXPECT_TRUE(rhi.is_initialized());
    EXPECT_FALSE(rhi.device_name().empty());
    EXPECT_NE(rhi.raw_device(), nullptr);
    rhi.shutdown();
    EXPECT_FALSE(rhi.is_initialized());
}

TEST(MetalRHI, ShutdownWithoutInitIsSafe) {
    MetalRHI rhi;
    rhi.shutdown();
    EXPECT_FALSE(rhi.is_initialized());
}

TEST(MetalRHI, BufferRoundTrip) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init());

    const float payload[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    BufferDesc desc{BufferType::Vertex, BufferUsage::Static, payload, sizeof(payload)};
    auto h = rhi.create_buffer(desc);
    ASSERT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    // Update path writes through the MTLBuffer contents pointer.
    const float patch[2] = {9.0f, 8.0f};
    rhi.update_buffer(h, patch, sizeof(patch), 0);

    rhi.destroy_buffer(h);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);
    rhi.shutdown();
}

TEST(MetalRHI, TextureCreationProducesLiveRecord) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init());

    TextureDesc td;
    td.width  = 4;
    td.height = 4;
    td.format = TextureFormat::RGBA8;
    td.generate_mipmaps = false;

    auto t = rhi.create_texture(td);
    ASSERT_NE(t, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_texture_count(), 1u);

    rhi.destroy_texture(t);
    EXPECT_EQ(rhi.live_texture_count(), 0u);
    rhi.shutdown();
}

TEST(MetalRHI, FactoryCreatesMetalBackend) {
    auto rhi = RHI::create(Backend::Metal);
    ASSERT_NE(rhi, nullptr);
    ASSERT_TRUE(rhi->init());
    rhi->shutdown();
}

TEST(MetalRHI, ShaderCompilesToMTLLibrary) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init());

    // Default translator accepts bare GLSL-ish source and swaps in a known
    // MSL pass-through shader exposing vs_main/fs_main.
    auto s = rhi.create_shader("void main() {}", "void main() {}");
    ASSERT_NE(s, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_shader_count(), 1u);

    rhi.destroy_shader(s);
    EXPECT_EQ(rhi.live_shader_count(), 0u);
    rhi.shutdown();
}

TEST(MetalRHI, PipelineStateCompiles) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init());

    auto s = rhi.create_shader("_", "_");
    ASSERT_NE(s, INVALID_HANDLE);

    PipelineDesc pd{};
    pd.shader      = s;
    pd.blend       = BlendMode::Alpha;
    pd.cull        = CullMode::Back;
    pd.depth       = DepthFunc::LessEqual;
    pd.depth_write = true;
    pd.depth_test  = true;
    pd.primitive   = PrimitiveType::Triangles;
    pd.vertex_layout.stride = sizeof(float) * 9;
    pd.vertex_layout.attributes = {
        {0, 3, 0,  false},                      // position
        {1, 2, sizeof(float) * 3,  false},      // uv
        {2, 4, sizeof(float) * 5,  false},      // color
    };

    auto p = rhi.create_pipeline(pd);
    ASSERT_NE(p, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_pipeline_count(), 1u);

    rhi.destroy_pipeline(p);
    EXPECT_EQ(rhi.live_pipeline_count(), 0u);
    rhi.shutdown();
}

TEST(MetalRHI, FrameEncodeRoundTrip) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init());

    // Build a full framebuffer + pipeline + draw path using real Metal objects.
    FramebufferDesc fd{};
    fd.width             = 64;
    fd.height            = 64;
    fd.color_attachments = {TextureFormat::RGBA8};
    fd.has_depth         = true;
    auto fb = rhi.create_framebuffer(fd);
    ASSERT_NE(fb, INVALID_HANDLE);

    auto sh = rhi.create_shader("_", "_");
    ASSERT_NE(sh, INVALID_HANDLE);

    PipelineDesc pd{};
    pd.shader    = sh;
    pd.primitive = PrimitiveType::Triangles;
    pd.vertex_layout.stride = sizeof(float) * 9;
    pd.vertex_layout.attributes = {
        {0, 3, 0, false},
        {1, 2, sizeof(float) * 3, false},
        {2, 4, sizeof(float) * 5, false},
    };
    auto pipe = rhi.create_pipeline(pd);
    ASSERT_NE(pipe, INVALID_HANDLE);

    const float verts[3 * 9] = {
        // pos,          uv,      color
        -1.0f, -1.0f, 0, 0, 0,  1, 0, 0, 1,
         1.0f, -1.0f, 0, 1, 0,  0, 1, 0, 1,
         0.0f,  1.0f, 0, 0.5f, 1, 0, 0, 1, 1,
    };
    BufferDesc vbd{BufferType::Vertex, BufferUsage::Static, verts, sizeof(verts)};
    auto vb = rhi.create_buffer(vbd);
    ASSERT_NE(vb, INVALID_HANDLE);

    rhi.begin_frame();
    rhi.clear(Vec4{0.1f, 0.2f, 0.3f, 1.0f}, 1.0f);
    rhi.bind_framebuffer(fb);
    rhi.set_viewport(0, 0, 64, 64);
    rhi.set_scissor(0, 0, 64, 64);
    rhi.bind_pipeline(pipe);
    rhi.bind_vertex_buffer(vb);
    rhi.draw(3, 0);
    rhi.unbind_framebuffer();
    rhi.end_frame();

    EXPECT_EQ(rhi.draw_call_count(), 1u);
    EXPECT_GT(rhi.state_change_count(), 0u);

    rhi.destroy_buffer(vb);
    rhi.destroy_pipeline(pipe);
    rhi.destroy_shader(sh);
    rhi.destroy_framebuffer(fb);
    rhi.shutdown();
}

TEST(MetalRHI, IndexedDrawExecutes) {
    MetalRHI rhi;
    ASSERT_TRUE(rhi.init());

    FramebufferDesc fd{};
    fd.width             = 32;
    fd.height            = 32;
    fd.color_attachments = {TextureFormat::RGBA8};
    fd.has_depth         = true;
    auto fb = rhi.create_framebuffer(fd);

    auto sh = rhi.create_shader("_", "_");
    PipelineDesc pd{};
    pd.shader    = sh;
    pd.primitive = PrimitiveType::Triangles;
    pd.vertex_layout.stride = sizeof(float) * 9;
    pd.vertex_layout.attributes = {
        {0, 3, 0, false},
        {1, 2, sizeof(float) * 3, false},
        {2, 4, sizeof(float) * 5, false},
    };
    auto pipe = rhi.create_pipeline(pd);
    const float verts[4 * 9] = {
        -1, -1, 0, 0, 0, 1, 1, 1, 1,
         1, -1, 0, 1, 0, 1, 1, 1, 1,
         1,  1, 0, 1, 1, 1, 1, 1, 1,
        -1,  1, 0, 0, 1, 1, 1, 1, 1,
    };
    const u32 indices[6] = {0, 1, 2, 2, 3, 0};

    auto vb = rhi.create_buffer({BufferType::Vertex, BufferUsage::Static, verts, sizeof(verts)});
    auto ib = rhi.create_buffer({BufferType::Index,  BufferUsage::Static, indices, sizeof(indices)});

    rhi.begin_frame();
    rhi.clear(Vec4{0,0,0,1}, 1.0f);
    rhi.bind_framebuffer(fb);
    rhi.set_viewport(0, 0, 32, 32);
    rhi.bind_pipeline(pipe);
    rhi.bind_vertex_buffer(vb);
    rhi.bind_index_buffer(ib);
    rhi.draw_indexed(6, 0);
    rhi.unbind_framebuffer();
    rhi.end_frame();

    EXPECT_EQ(rhi.draw_call_count(), 1u);

    rhi.destroy_buffer(ib);
    rhi.destroy_buffer(vb);
    rhi.destroy_pipeline(pipe);
    rhi.destroy_shader(sh);
    rhi.destroy_framebuffer(fb);
    rhi.shutdown();
}
