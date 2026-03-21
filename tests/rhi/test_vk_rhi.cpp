// ============================================================================
// test_vk_rhi.cpp — VulkanRHI backend unit tests
// ============================================================================

#include <gtest/gtest.h>
#include "nexus/rhi/vk_rhi.h"
#include "nexus/rhi/rhi.h"

using namespace nexus;
using namespace nexus::rhi;

// =============================================================================
// Lifecycle
// =============================================================================

TEST(VulkanRHI, InitAndShutdown) {
    VulkanRHI rhi;
    EXPECT_FALSE(rhi.is_initialized());

    EXPECT_TRUE(rhi.init());
    EXPECT_TRUE(rhi.is_initialized());

    rhi.shutdown();
    EXPECT_FALSE(rhi.is_initialized());
}

TEST(VulkanRHI, DoubleInitIsIdempotent) {
    VulkanRHI rhi;
    EXPECT_TRUE(rhi.init());
    EXPECT_TRUE(rhi.init());

    // Create a resource to verify handles are still valid after double init
    BufferDesc desc{BufferType::Vertex, BufferUsage::Static, nullptr, 64};
    auto h = rhi.create_buffer(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, ShutdownWithoutInit) {
    VulkanRHI rhi;
    // Should not crash
    rhi.shutdown();
    EXPECT_FALSE(rhi.is_initialized());
}

TEST(VulkanRHI, ReinitAfterShutdown) {
    VulkanRHI rhi;
    rhi.init();
    auto h = rhi.create_buffer({BufferType::Vertex, BufferUsage::Static, nullptr, 32});
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
    EXPECT_FALSE(rhi.is_initialized());

    // Re-init should work cleanly
    EXPECT_TRUE(rhi.init());
    EXPECT_EQ(rhi.live_buffer_count(), 0u);
    rhi.shutdown();
}

// =============================================================================
// Factory
// =============================================================================

TEST(VulkanRHI, FactoryCreateVulkan) {
    auto rhi = RHI::create(Backend::Vulkan);
    ASSERT_NE(rhi, nullptr);
    EXPECT_TRUE(rhi->init());
    rhi->shutdown();
}

TEST(VulkanRHI, FactoryCreateOpenGL) {
    // OpenGL backend also exists
    auto rhi = RHI::create(Backend::OpenGL);
    ASSERT_NE(rhi, nullptr);
}

TEST(VulkanRHI, FactoryDefaultBackend) {
    auto rhi = RHI::create();
    ASSERT_NE(rhi, nullptr);
}

// =============================================================================
// Buffers
// =============================================================================

TEST(VulkanRHI, CreateDestroyBuffer) {
    VulkanRHI rhi;
    rhi.init();

    BufferDesc desc;
    desc.type = BufferType::Vertex;
    desc.usage = BufferUsage::Static;
    desc.size = 128;
    desc.data = nullptr;

    BufferHandle h = rhi.create_buffer(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.destroy_buffer(h);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, CreateBufferWithData) {
    VulkanRHI rhi;
    rhi.init();

    float vertices[] = {1.0f, 2.0f, 3.0f, 4.0f};
    BufferDesc desc;
    desc.type = BufferType::Vertex;
    desc.usage = BufferUsage::Static;
    desc.size = sizeof(vertices);
    desc.data = vertices;

    BufferHandle h = rhi.create_buffer(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, CreateZeroSizeBuffer) {
    VulkanRHI rhi;
    rhi.init();

    BufferDesc desc{BufferType::Vertex, BufferUsage::Static, nullptr, 0};
    auto h = rhi.create_buffer(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, UpdateBuffer) {
    VulkanRHI rhi;
    rhi.init();

    BufferDesc desc;
    desc.type = BufferType::Vertex;
    desc.usage = BufferUsage::Dynamic;
    desc.size = 64;

    BufferHandle h = rhi.create_buffer(desc);
    float data[] = {1.0f, 2.0f};
    rhi.update_buffer(h, data, sizeof(data), 0);
    // Should not crash
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, UpdateBufferZeroSize) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_buffer({BufferType::Vertex, BufferUsage::Dynamic, nullptr, 64});
    // zero-size update should be a no-op
    float data = 1.0f;
    rhi.update_buffer(h, &data, 0, 0);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, UpdateBufferNullData) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_buffer({BufferType::Vertex, BufferUsage::Dynamic, nullptr, 64});
    // null data with non-zero size should not crash (grows buffer but no copy)
    rhi.update_buffer(h, nullptr, 32, 0);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, UpdateBufferBeyondSize) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_buffer({BufferType::Vertex, BufferUsage::Dynamic, nullptr, 16});
    float data[] = {1.0f, 2.0f};
    // offset=100 is beyond buffer size; should grow the buffer
    rhi.update_buffer(h, data, sizeof(data), 100);
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.shutdown();
}

TEST(VulkanRHI, DestroyInvalidBuffer) {
    VulkanRHI rhi;
    rhi.init();
    // Should not crash
    rhi.destroy_buffer(INVALID_HANDLE);
    rhi.destroy_buffer(999);
    rhi.shutdown();
}

TEST(VulkanRHI, DoubleDestroyBuffer) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_buffer({BufferType::Vertex, BufferUsage::Static, nullptr, 64});
    EXPECT_EQ(rhi.live_buffer_count(), 1u);

    rhi.destroy_buffer(h);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);

    // Double destroy should be idempotent
    rhi.destroy_buffer(h);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, UpdateDestroyedBuffer) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_buffer({BufferType::Vertex, BufferUsage::Dynamic, nullptr, 64});
    rhi.destroy_buffer(h);

    // Should be a no-op (buffer is dead)
    float data = 1.0f;
    rhi.update_buffer(h, &data, sizeof(data), 0);
    EXPECT_EQ(rhi.live_buffer_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, MultipleBufferTypes) {
    VulkanRHI rhi;
    rhi.init();

    BufferDesc vb{BufferType::Vertex, BufferUsage::Static, nullptr, 64};
    BufferDesc ib{BufferType::Index, BufferUsage::Static, nullptr, 32};
    BufferDesc ub{BufferType::Uniform, BufferUsage::Dynamic, nullptr, 256};

    auto h1 = rhi.create_buffer(vb);
    auto h2 = rhi.create_buffer(ib);
    auto h3 = rhi.create_buffer(ub);

    EXPECT_NE(h1, h2);
    EXPECT_NE(h2, h3);
    EXPECT_EQ(rhi.live_buffer_count(), 3u);

    rhi.destroy_buffer(h2);
    EXPECT_EQ(rhi.live_buffer_count(), 2u);

    rhi.shutdown();
}

// =============================================================================
// Textures
// =============================================================================

TEST(VulkanRHI, CreateDestroyTexture) {
    VulkanRHI rhi;
    rhi.init();

    TextureDesc desc;
    desc.width = 256;
    desc.height = 256;
    desc.format = TextureFormat::RGBA8;

    TextureHandle h = rhi.create_texture(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_texture_count(), 1u);

    rhi.destroy_texture(h);
    EXPECT_EQ(rhi.live_texture_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, CreateZeroDimensionTexture) {
    VulkanRHI rhi;
    rhi.init();

    TextureDesc desc;
    desc.width = 0;
    desc.height = 0;
    desc.format = TextureFormat::RGBA8;

    auto h = rhi.create_texture(desc);
    // Zero-dimension texture should still create (handle valid)
    EXPECT_NE(h, INVALID_HANDLE);

    rhi.shutdown();
}

TEST(VulkanRHI, DoubleDestroyTexture) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_texture({64, 64, TextureFormat::RGBA8});
    rhi.destroy_texture(h);
    EXPECT_EQ(rhi.live_texture_count(), 0u);

    // Double destroy should be idempotent
    rhi.destroy_texture(h);
    EXPECT_EQ(rhi.live_texture_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, TextureFormats) {
    VulkanRHI rhi;
    rhi.init();

    TextureFormat formats[] = {
        TextureFormat::RGBA8,
        TextureFormat::RGB8,
        TextureFormat::R8,
        TextureFormat::RGBA16F,
        TextureFormat::RGBA32F,
        TextureFormat::Depth32F,
        TextureFormat::Depth24Stencil8,
    };

    for (auto fmt : formats) {
        TextureDesc desc;
        desc.width = 64;
        desc.height = 64;
        desc.format = fmt;
        TextureHandle h = rhi.create_texture(desc);
        EXPECT_NE(h, INVALID_HANDLE);
    }

    EXPECT_EQ(rhi.live_texture_count(), 7u);
    rhi.shutdown();
}

// =============================================================================
// Shaders
// =============================================================================

TEST(VulkanRHI, CreateDestroyShader) {
    VulkanRHI rhi;
    rhi.init();

    ShaderHandle h = rhi.create_shader(
        "void main() { gl_Position = vec4(0); }",
        "void main() { fragColor = vec4(1); }"
    );
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_shader_count(), 1u);

    rhi.destroy_shader(h);
    EXPECT_EQ(rhi.live_shader_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, CreateShaderEmptySource) {
    VulkanRHI rhi;
    rhi.init();

    ShaderHandle h = rhi.create_shader("", "");
    EXPECT_EQ(h, INVALID_HANDLE);

    h = rhi.create_shader("vertex", "");
    EXPECT_EQ(h, INVALID_HANDLE);

    rhi.shutdown();
}

TEST(VulkanRHI, DoubleDestroyShader) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_shader("vs", "fs");
    rhi.destroy_shader(h);
    EXPECT_EQ(rhi.live_shader_count(), 0u);

    rhi.destroy_shader(h);
    EXPECT_EQ(rhi.live_shader_count(), 0u);

    rhi.shutdown();
}

// =============================================================================
// Pipelines
// =============================================================================

TEST(VulkanRHI, CreateDestroyPipeline) {
    VulkanRHI rhi;
    rhi.init();

    PipelineDesc desc;
    desc.blend = BlendMode::Alpha;
    desc.cull = CullMode::Back;
    desc.depth = DepthFunc::Less;

    PipelineHandle h = rhi.create_pipeline(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_pipeline_count(), 1u);

    rhi.destroy_pipeline(h);
    EXPECT_EQ(rhi.live_pipeline_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, DoubleDestroyPipeline) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_pipeline({});
    rhi.destroy_pipeline(h);
    rhi.destroy_pipeline(h);
    EXPECT_EQ(rhi.live_pipeline_count(), 0u);

    rhi.shutdown();
}

// =============================================================================
// Framebuffers
// =============================================================================

TEST(VulkanRHI, CreateDestroyFramebuffer) {
    VulkanRHI rhi;
    rhi.init();

    FramebufferDesc desc;
    desc.width = 1024;
    desc.height = 768;
    desc.color_attachments = {TextureFormat::RGBA8};
    desc.has_depth = true;

    FramebufferHandle h = rhi.create_framebuffer(desc);
    EXPECT_NE(h, INVALID_HANDLE);
    EXPECT_EQ(rhi.live_framebuffer_count(), 1u);

    rhi.destroy_framebuffer(h);
    EXPECT_EQ(rhi.live_framebuffer_count(), 0u);

    rhi.shutdown();
}

TEST(VulkanRHI, FramebufferMultipleColorAttachments) {
    VulkanRHI rhi;
    rhi.init();

    FramebufferDesc desc;
    desc.width = 512;
    desc.height = 512;
    desc.color_attachments = {TextureFormat::RGBA8, TextureFormat::RGBA16F};
    desc.has_depth = true;

    FramebufferHandle h = rhi.create_framebuffer(desc);
    EXPECT_NE(h, INVALID_HANDLE);

    rhi.shutdown();
}

TEST(VulkanRHI, DoubleDestroyFramebuffer) {
    VulkanRHI rhi;
    rhi.init();

    FramebufferDesc desc;
    desc.width = 128;
    desc.height = 128;
    desc.color_attachments = {TextureFormat::RGBA8};

    auto h = rhi.create_framebuffer(desc);
    rhi.destroy_framebuffer(h);
    rhi.destroy_framebuffer(h);
    EXPECT_EQ(rhi.live_framebuffer_count(), 0u);

    rhi.shutdown();
}

// =============================================================================
// Frame & Draw Commands
// =============================================================================

TEST(VulkanRHI, FrameDrawCounting) {
    VulkanRHI rhi;
    rhi.init();

    rhi.begin_frame();
    EXPECT_EQ(rhi.draw_call_count(), 0u);
    EXPECT_EQ(rhi.state_change_count(), 0u);

    rhi.set_viewport(0, 0, 800, 600);
    rhi.set_scissor(0, 0, 800, 600);
    rhi.clear(Vec4{0, 0, 0, 1}, 1.0f);
    EXPECT_EQ(rhi.state_change_count(), 3u);

    rhi.draw(100, 0);
    rhi.draw_indexed(36, 0);
    EXPECT_EQ(rhi.draw_call_count(), 2u);

    rhi.end_frame();
    rhi.shutdown();
}

TEST(VulkanRHI, FrameResetsCounts) {
    VulkanRHI rhi;
    rhi.init();

    rhi.begin_frame();
    rhi.draw(10, 0);
    rhi.set_viewport(0, 0, 100, 100);
    EXPECT_EQ(rhi.draw_call_count(), 1u);
    EXPECT_EQ(rhi.state_change_count(), 1u);
    rhi.end_frame();

    // New frame resets
    rhi.begin_frame();
    EXPECT_EQ(rhi.draw_call_count(), 0u);
    EXPECT_EQ(rhi.state_change_count(), 0u);
    rhi.end_frame();

    rhi.shutdown();
}

// =============================================================================
// State Binding
// =============================================================================

TEST(VulkanRHI, BindPipelineAndShader) {
    VulkanRHI rhi;
    rhi.init();

    auto shader = rhi.create_shader("vs", "fs");
    PipelineDesc pd;
    pd.shader = shader;
    auto pipeline = rhi.create_pipeline(pd);

    rhi.begin_frame();
    rhi.bind_pipeline(pipeline);
    // Should not crash; pipeline binds its shader internally
    rhi.end_frame();

    rhi.shutdown();
}

TEST(VulkanRHI, BindVertexAndIndexBuffers) {
    VulkanRHI rhi;
    rhi.init();

    BufferDesc vb{BufferType::Vertex, BufferUsage::Static, nullptr, 64};
    BufferDesc ib{BufferType::Index, BufferUsage::Static, nullptr, 24};

    auto vh = rhi.create_buffer(vb);
    auto ih = rhi.create_buffer(ib);

    rhi.begin_frame();
    rhi.bind_vertex_buffer(vh);
    rhi.bind_index_buffer(ih);
    rhi.end_frame();

    rhi.shutdown();
}

TEST(VulkanRHI, BindAndUnbindFramebuffer) {
    VulkanRHI rhi;
    rhi.init();

    FramebufferDesc desc;
    desc.width = 512;
    desc.height = 512;
    desc.color_attachments = {TextureFormat::RGBA8};

    auto fb = rhi.create_framebuffer(desc);

    rhi.begin_frame();
    rhi.bind_framebuffer(fb);
    rhi.unbind_framebuffer();
    rhi.end_frame();

    rhi.shutdown();
}

// =============================================================================
// Uniforms
// =============================================================================

TEST(VulkanRHI, SetUniforms) {
    VulkanRHI rhi;
    rhi.init();

    auto shader = rhi.create_shader("vs", "fs");

    // All of these should not crash
    rhi.set_uniform_int(shader, "u_int", 42);
    rhi.set_uniform_float(shader, "u_float", 3.14f);
    rhi.set_uniform_vec2(shader, "u_vec2", Vec2(1, 2));
    rhi.set_uniform_vec3(shader, "u_vec3", Vec3(1, 2, 3));
    rhi.set_uniform_vec4(shader, "u_vec4", Vec4(1, 2, 3, 4));
    rhi.set_uniform_mat4(shader, "u_mat4", Mat4(1.0f));

    i32 arr[] = {1, 2, 3};
    rhi.set_uniform_int_array(shader, "u_arr", arr, 3);

    rhi.shutdown();
}

TEST(VulkanRHI, SetUniformInvalidShader) {
    VulkanRHI rhi;
    rhi.init();
    // Should not crash
    rhi.set_uniform_int(INVALID_HANDLE, "u_int", 0);
    rhi.set_uniform_float(999, "u_float", 0);
    rhi.shutdown();
}

TEST(VulkanRHI, SetUniformOnDestroyedShader) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_shader("vs", "fs");
    rhi.destroy_shader(h);

    // Should silently do nothing (shader is dead)
    rhi.set_uniform_int(h, "u_int", 42);
    rhi.set_uniform_float(h, "u_float", 3.14f);
    rhi.set_uniform_vec2(h, "u_vec2", Vec2(1, 2));
    rhi.set_uniform_vec3(h, "u_vec3", Vec3(1, 2, 3));
    rhi.set_uniform_vec4(h, "u_vec4", Vec4(1, 2, 3, 4));
    rhi.set_uniform_mat4(h, "u_mat4", Mat4(1.0f));

    rhi.shutdown();
}

TEST(VulkanRHI, SetUniformIntArrayEdgeCases) {
    VulkanRHI rhi;
    rhi.init();

    auto h = rhi.create_shader("vs", "fs");

    // nullptr with count > 0 should be no-op
    rhi.set_uniform_int_array(h, "u_arr", nullptr, 3);

    // count = 0 should be no-op
    i32 arr[] = {1, 2, 3};
    rhi.set_uniform_int_array(h, "u_arr", arr, 0);

    rhi.shutdown();
}

// =============================================================================
// Blend & Depth
// =============================================================================

TEST(VulkanRHI, BlendModes) {
    VulkanRHI rhi;
    rhi.init();
    rhi.begin_frame();
    rhi.set_blend_mode(BlendMode::None);
    rhi.set_blend_mode(BlendMode::Alpha);
    rhi.set_blend_mode(BlendMode::Additive);
    rhi.set_blend_mode(BlendMode::Multiply);
    EXPECT_EQ(rhi.state_change_count(), 4u);
    rhi.end_frame();
    rhi.shutdown();
}

TEST(VulkanRHI, DepthTest) {
    VulkanRHI rhi;
    rhi.init();
    rhi.begin_frame();
    rhi.set_depth_test(true);
    rhi.set_depth_test(false);
    EXPECT_EQ(rhi.state_change_count(), 2u);
    rhi.end_frame();
    rhi.shutdown();
}

// =============================================================================
// Bind invalid/destroyed resources
// =============================================================================

TEST(VulkanRHI, BindDestroyedResources) {
    VulkanRHI rhi;
    rhi.init();

    auto buf = rhi.create_buffer({BufferType::Vertex, BufferUsage::Static, nullptr, 64});
    auto tex = rhi.create_texture({256, 256, TextureFormat::RGBA8});
    auto shader = rhi.create_shader("vs", "fs");
    PipelineDesc pd;
    pd.shader = shader;
    auto pipe = rhi.create_pipeline(pd);
    FramebufferDesc fbd;
    fbd.width = 128;
    fbd.height = 128;
    fbd.color_attachments = {TextureFormat::RGBA8};
    auto fb = rhi.create_framebuffer(fbd);

    // Destroy all
    rhi.destroy_buffer(buf);
    rhi.destroy_texture(tex);
    rhi.destroy_shader(shader);
    rhi.destroy_pipeline(pipe);
    rhi.destroy_framebuffer(fb);

    // Binding destroyed resources should not crash
    rhi.begin_frame();
    rhi.bind_vertex_buffer(buf);
    rhi.bind_index_buffer(buf);
    rhi.bind_texture(tex, 0);
    rhi.bind_shader(shader);
    rhi.bind_pipeline(pipe);
    rhi.bind_framebuffer(fb);
    rhi.end_frame();

    rhi.shutdown();
}

TEST(VulkanRHI, BindInvalidHandles) {
    VulkanRHI rhi;
    rhi.init();

    rhi.begin_frame();
    rhi.bind_vertex_buffer(INVALID_HANDLE);
    rhi.bind_index_buffer(INVALID_HANDLE);
    rhi.bind_shader(INVALID_HANDLE);
    rhi.bind_pipeline(INVALID_HANDLE);
    rhi.bind_framebuffer(INVALID_HANDLE);
    rhi.bind_texture(INVALID_HANDLE, 0); // unbind is valid
    rhi.end_frame();

    rhi.shutdown();
}

// =============================================================================
// Full Render Pass Simulation
// =============================================================================

TEST(VulkanRHI, FullRenderPassSimulation) {
    VulkanRHI rhi;
    rhi.init();

    // Create resources
    float verts[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    BufferDesc vbd{BufferType::Vertex, BufferUsage::Static, verts, sizeof(verts)};
    auto vb = rhi.create_buffer(vbd);

    u32 indices[] = {0, 1, 2};
    BufferDesc ibd{BufferType::Index, BufferUsage::Static, indices, sizeof(indices)};
    auto ib = rhi.create_buffer(ibd);

    auto shader = rhi.create_shader(
        "#version 450\nvoid main() {}",
        "#version 450\nvoid main() {}"
    );

    PipelineDesc pd;
    pd.shader = shader;
    pd.blend = BlendMode::None;
    pd.depth_test = true;
    pd.cull = CullMode::Back;
    auto pipeline = rhi.create_pipeline(pd);

    TextureDesc td;
    td.width = 64;
    td.height = 64;
    td.format = TextureFormat::RGBA8;
    auto tex = rhi.create_texture(td);

    FramebufferDesc fbd;
    fbd.width = 1024;
    fbd.height = 768;
    fbd.color_attachments = {TextureFormat::RGBA8};
    fbd.has_depth = true;
    auto fb = rhi.create_framebuffer(fbd);

    // Simulate a render pass
    rhi.begin_frame();

    // Off-screen pass
    rhi.bind_framebuffer(fb);
    rhi.set_viewport(0, 0, 1024, 768);
    rhi.clear(Vec4{0.1f, 0.1f, 0.1f, 1.0f}, 1.0f);
    rhi.bind_pipeline(pipeline);
    rhi.bind_vertex_buffer(vb);
    rhi.bind_index_buffer(ib);
    rhi.bind_texture(tex, 0);
    rhi.set_uniform_mat4(shader, "u_mvp", Mat4(1.0f));
    rhi.draw_indexed(3, 0);
    rhi.unbind_framebuffer();

    // Main pass
    rhi.set_viewport(0, 0, 1920, 1080);
    rhi.clear(Vec4{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
    rhi.draw(6, 0);

    rhi.end_frame();

    EXPECT_EQ(rhi.draw_call_count(), 2u);
    EXPECT_GT(rhi.state_change_count(), 0u);

    // Cleanup
    rhi.destroy_framebuffer(fb);
    rhi.destroy_texture(tex);
    rhi.destroy_pipeline(pipeline);
    rhi.destroy_shader(shader);
    rhi.destroy_buffer(ib);
    rhi.destroy_buffer(vb);

    EXPECT_EQ(rhi.live_buffer_count(), 0u);
    EXPECT_EQ(rhi.live_texture_count(), 0u);
    EXPECT_EQ(rhi.live_shader_count(), 0u);
    EXPECT_EQ(rhi.live_pipeline_count(), 0u);
    EXPECT_EQ(rhi.live_framebuffer_count(), 0u);

    rhi.shutdown();
}
