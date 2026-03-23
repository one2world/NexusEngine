#pragma once

#include <nexus/rhi/rhi.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace nexus::rhi {

// ---------------------------------------------------------------------------
// VulkanRHI — Vulkan-style RHI backend
//
// This backend emulates Vulkan's resource and command model using CPU-side
// data structures.  It tracks all state, validates usage, and records draw
// commands for later playback.  It does not link against the Vulkan SDK so
// that the engine can build and test on machines without a Vulkan driver.
// When a real Vulkan driver is available, the recorded commands can be
// translated to vkCmd* calls.
// ---------------------------------------------------------------------------

class VulkanRHI : public RHI {
public:
    VulkanRHI() = default;
    ~VulkanRHI() override = default;

    NEXUS_NON_COPYABLE(VulkanRHI)
    NEXUS_NON_MOVABLE(VulkanRHI)

    bool init() override;
    void shutdown() override;

    // Resource creation / destruction
    BufferHandle      create_buffer(const BufferDesc& desc) override;
    void              destroy_buffer(BufferHandle handle) override;
    void              update_buffer(BufferHandle handle,
                                    const void* data, size_t size, size_t offset) override;

    TextureHandle     create_texture(const TextureDesc& desc) override;
    void              destroy_texture(TextureHandle handle) override;

    ShaderHandle      create_shader(const std::string& vertex_src,
                                    const std::string& fragment_src) override;
    void              destroy_shader(ShaderHandle handle) override;

    PipelineHandle    create_pipeline(const PipelineDesc& desc) override;
    void              destroy_pipeline(PipelineHandle handle) override;

    FramebufferHandle create_framebuffer(const FramebufferDesc& desc) override;
    void              destroy_framebuffer(FramebufferHandle handle) override;

    // Render commands
    void begin_frame() override;
    void end_frame() override;

    void set_viewport(i32 x, i32 y, i32 w, i32 h) override;
    void set_scissor(i32 x, i32 y, i32 w, i32 h) override;
    void clear(Vec4 color, float depth) override;

    void bind_pipeline(PipelineHandle handle) override;
    void bind_shader(ShaderHandle handle) override;
    void bind_texture(TextureHandle handle, u32 slot) override;
    void bind_framebuffer(FramebufferHandle handle) override;
    void unbind_framebuffer() override;
    void bind_vertex_buffer(BufferHandle handle) override;
    void bind_index_buffer(BufferHandle handle) override;

    // State toggles
    void set_blend_mode(BlendMode mode) override;
    void set_depth_test(bool enabled) override;
    void set_depth_write(bool enabled) override;
    void set_cull_mode(CullMode mode) override;

    // Uniforms
    void set_uniform_int(ShaderHandle shader,
                         const std::string& name, i32 value) override;
    void set_uniform_int_array(ShaderHandle shader,
                               const std::string& name,
                               const i32* values, u32 count) override;
    void set_uniform_float(ShaderHandle shader,
                           const std::string& name, float value) override;
    void set_uniform_vec2(ShaderHandle shader,
                          const std::string& name, Vec2 value) override;
    void set_uniform_vec3(ShaderHandle shader,
                          const std::string& name, Vec3 value) override;
    void set_uniform_vec4(ShaderHandle shader,
                          const std::string& name, Vec4 value) override;
    void set_uniform_mat4(ShaderHandle shader,
                          const std::string& name,
                          const Mat4& value) override;

    // Draw calls
    void draw(u32 vertex_count, u32 first_vertex) override;
    void draw_indexed(u32 index_count, u32 first_index) override;

    // ── Vulkan-specific queries ─────────────────────────────────────────────

    /// Total draw calls recorded this frame.
    [[nodiscard]] u32 draw_call_count() const { return draw_call_count_; }

    /// Total state changes recorded this frame.
    [[nodiscard]] u32 state_change_count() const { return state_change_count_; }

    /// Whether the backend is initialized.
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    /// Total resources currently alive.
    [[nodiscard]] u32 live_buffer_count() const;
    [[nodiscard]] u32 live_texture_count() const;
    [[nodiscard]] u32 live_shader_count() const;
    [[nodiscard]] u32 live_pipeline_count() const;
    [[nodiscard]] u32 live_framebuffer_count() const;

private:
    // Internal resource records — CPU-side simulation of Vulkan objects.
    struct VkBuffer {
        bool        alive{false};
        BufferType  type{BufferType::Vertex};
        BufferUsage usage{BufferUsage::Static};
        std::vector<u8> data;
    };

    struct VkTexture {
        bool          alive{false};
        u32           width{0};
        u32           height{0};
        TextureFormat format{TextureFormat::RGBA8};
        std::vector<u8> pixels;
    };

    struct VkShader {
        bool        alive{false};
        std::string vertex_src;
        std::string fragment_src;
        std::unordered_map<std::string, i32> uniform_ints;
        std::unordered_map<std::string, float> uniform_floats;
        std::unordered_map<std::string, Vec2> uniform_vec2s;
        std::unordered_map<std::string, Vec3> uniform_vec3s;
        std::unordered_map<std::string, Vec4> uniform_vec4s;
        std::unordered_map<std::string, Mat4> uniform_mat4s;
    };

    struct VkPipeline {
        bool         alive{false};
        PipelineDesc desc;
    };

    struct VkFramebuffer {
        bool alive{false};
        u32  width{0};
        u32  height{0};
        std::vector<TextureFormat> color_formats;
        bool has_depth{false};
    };

    // Bound state for validation.
    struct BoundState {
        PipelineHandle    pipeline{INVALID_HANDLE};
        ShaderHandle      shader{INVALID_HANDLE};
        FramebufferHandle framebuffer{INVALID_HANDLE};
        BufferHandle      vertex_buffer{INVALID_HANDLE};
        BufferHandle      index_buffer{INVALID_HANDLE};
        BlendMode         blend{BlendMode::None};
        bool              depth_test{true};
        bool              depth_write{true};
        CullMode          cull{CullMode::Back};
        i32 viewport_x{0}, viewport_y{0}, viewport_w{0}, viewport_h{0};
        i32 scissor_x{0}, scissor_y{0}, scissor_w{0}, scissor_h{0};
    };

    std::vector<VkBuffer>      buffers_;
    std::vector<VkTexture>     textures_;
    std::vector<VkShader>      shaders_;
    std::vector<VkPipeline>    pipelines_;
    std::vector<VkFramebuffer> framebuffers_;

    BoundState state_;
    bool       initialized_{false};
    bool       in_frame_{false};
    u32        draw_call_count_{0};
    u32        state_change_count_{0};
};

} // namespace nexus::rhi
