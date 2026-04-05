// ============================================================================
// test_webgl_rhi.cpp — WebGL RHI backend unit tests (CPU-simulated mode)
// ============================================================================

#include <gtest/gtest.h>
#include "nexus/rhi/webgl_rhi.h"
#include "nexus/rhi/rhi.h"

using namespace nexus;
using namespace nexus::rhi;

// ── Lifecycle ───────────────────────────────────────────────────────────────

TEST(WebGLRHI, InitAndShutdown) {
    WebGLRHI rhi;
    EXPECT_FALSE(rhi.is_initialized());

    EXPECT_TRUE(rhi.init());
    EXPECT_TRUE(rhi.is_initialized());

    rhi.shutdown();
    EXPECT_FALSE(rhi.is_initialized());
}

TEST(WebGLRHI, DoubleInitIsIdempotent) {
    WebGLRHI rhi;
    EXPECT_TRUE(rhi.init());
    EXPECT_TRUE(rhi.init());
    rhi.shutdown();
}

// ── Buffers ─────────────────────────────────────────────────────────────────

TEST(WebGLRHI, CreateAndDestroyBuffer) {
    WebGLRHI rhi;
    rhi.init();

    BufferDesc desc{BufferType::Vertex, BufferUsage::Static, nullptr, 256};
    auto handle = rhi.create_buffer(desc);
    EXPECT_NE(handle, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.destroy_buffer(handle);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);

    rhi.shutdown();
}

TEST(WebGLRHI, MultipleBuffers) {
    WebGLRHI rhi;
    rhi.init();

    BufferDesc vb{BufferType::Vertex, BufferUsage::Dynamic, nullptr, 1024};
    BufferDesc ib{BufferType::Index, BufferUsage::Static, nullptr, 512};

    auto h1 = rhi.create_buffer(vb);
    auto h2 = rhi.create_buffer(ib);
    EXPECT_NE(h1, h2);
    EXPECT_EQ(rhi.live_buffer_count(), 2u);

    rhi.destroy_buffer(h1);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.destroy_buffer(h2);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);

    rhi.shutdown();
}

// ── Textures ────────────────────────────────────────────────────────────────

TEST(WebGLRHI, CreateAndDestroyTexture) {
    WebGLRHI rhi;
    rhi.init();

    TextureDesc desc;
    desc.width = 256;
    desc.height = 256;
    desc.format = TextureFormat::RGBA8;

    auto handle = rhi.create_texture(desc);
    EXPECT_NE(handle, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_texture_count(), 1u);

    rhi.destroy_texture(handle);
    EXPECT_EQ(rhi.live_texture_count(), 0u);

    rhi.shutdown();
}

// ── Shaders ─────────────────────────────────────────────────────────────────

TEST(WebGLRHI, CreateShader) {
    WebGLRHI rhi;
    rhi.init();

    // ES 3.0 shader sources
    std::string vs = R"(#version 300 es
        layout(location = 0) in vec3 a_position;
        void main() { gl_Position = vec4(a_position, 1.0); }
    )";
    std::string fs = R"(#version 300 es
        precision mediump float;
        out vec4 fragColor;
        void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }
    )";

    auto handle = rhi.create_shader(vs, fs);
    EXPECT_NE(handle, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_shader_count(), 1u);

    rhi.destroy_shader(handle);
    EXPECT_EQ(rhi.live_shader_count(), 0u);

    rhi.shutdown();
}

TEST(WebGLRHI, EmptyShaderFails) {
    WebGLRHI rhi;
    rhi.init();

    auto handle = rhi.create_shader("", "");
    EXPECT_EQ(handle, INVALID_HANDLE);

    rhi.shutdown();
}

// ── Frame / Draw ────────────────────────────────────────────────────────────

TEST(WebGLRHI, FrameAndDraw) {
    WebGLRHI rhi;
    rhi.init();

    rhi.begin_frame();
    EXPECT_EQ(rhi.draw_call_count(), 0u);

    rhi.draw(6, 0);
    EXPECT_EQ(rhi.draw_call_count(), 1u);

    rhi.draw_indexed(36, 0);
    EXPECT_EQ(rhi.draw_call_count(), 2u);

    rhi.end_frame();

    rhi.shutdown();
}

// ── State ───────────────────────────────────────────────────────────────────

TEST(WebGLRHI, StateChangesNoThrow) {
    WebGLRHI rhi;
    rhi.init();

    // These should all succeed without errors (CPU-simulated mode)
    rhi.set_viewport(0, 0, 800, 600);
    rhi.set_scissor(0, 0, 800, 600);
    rhi.clear(Vec4(0.1f, 0.1f, 0.1f, 1.0f), 1.0f);
    rhi.set_blend_mode(BlendMode::Alpha);
    rhi.set_depth_test(true);
    rhi.set_depth_write(false);
    rhi.set_cull_mode(CullMode::Back);

    rhi.shutdown();
}

// ── Capabilities ────────────────────────────────────────────────────────────

TEST(WebGLRHI, Capabilities) {
    WebGLRHI rhi;
    rhi.init();

    EXPECT_GT(rhi.max_texture_size(), 0u);
    EXPECT_GT(rhi.max_vertex_attribs(), 0u);
    EXPECT_TRUE(rhi.has_instancing()); // WebGL2 core feature

    rhi.shutdown();
}

// ── Factory ─────────────────────────────────────────────────────────────────

#if NEXUS_ENABLE_WEBGL
TEST(WebGLRHI, FactoryCreation) {
    auto rhi = RHI::create(Backend::WebGL);
    ASSERT_NE(rhi, nullptr);
    EXPECT_TRUE(rhi->init());
    rhi->shutdown();
}
#endif
