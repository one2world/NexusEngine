#pragma once

// ---------------------------------------------------------------------------
// metal_rhi.h — Apple Metal RHI backend (macOS / iOS).
//
// Links against the real Metal / QuartzCore / Foundation frameworks and is
// compiled only when CMake detects them (NEXUS_ENABLE_METAL && NEXUS_HAVE_METAL).
// There is no CPU-simulated fallback: on platforms without Metal, the backend
// is not built and the factory returns nullptr for Backend::Metal.
//
// Resource model:
//   - init()     → real `id<MTLDevice>` + `id<MTLCommandQueue>`.
//   - buffers    → real `id<MTLBuffer>` (shared storage, host coherent).
//   - textures   → real `id<MTLTexture>` (private storage for GPU usage).
//   - shaders    → source text retained; translated to MSL on pipeline build
//                  (uses a small GLSL→MSL surface for the built-in renderer
//                  paths; bespoke MSL sources are accepted verbatim when the
//                  input contains `#include <metal_stdlib>`).
//   - pipelines  → real `id<MTLRenderPipelineState>` + depth-stencil state +
//                  vertex-descriptor derived from the RHI VertexLayout.
//   - framebuffers → `id<MTLTexture>` color + depth attachments + a
//                    per-framebuffer MTLRenderPassDescriptor.
//
// Frame model:
//   begin_frame()              → allocates an `id<MTLCommandBuffer>`.
//   bind_framebuffer()         → starts an `id<MTLRenderCommandEncoder>`.
//   set_viewport/set_scissor/… → forwarded to the current encoder.
//   draw / draw_indexed        → real encoder primitive draw calls.
//   unbind_framebuffer()       → ends the current encoder.
//   end_frame()                → commits the command buffer.
// ---------------------------------------------------------------------------

#include <nexus/rhi/rhi.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::rhi {

class MetalRHI : public RHI {
public:
    MetalRHI();
    ~MetalRHI() override;

    NEXUS_NON_COPYABLE(MetalRHI)
    NEXUS_NON_MOVABLE(MetalRHI)

    bool init() override;
    void shutdown() override;

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

    void set_blend_mode(BlendMode mode) override;
    void set_depth_test(bool enabled) override;
    void set_depth_write(bool enabled) override;
    void set_cull_mode(CullMode mode) override;

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

    void draw(u32 vertex_count, u32 first_vertex) override;
    void draw_indexed(u32 index_count, u32 first_index) override;

    // ── Metal-specific queries ──────────────────────────────────────────────

    /// Whether a real MTLDevice was acquired on init().
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    /// Human-readable name of the Metal device acquired on init().
    [[nodiscard]] const std::string& device_name() const { return device_name_; }

    /// Draw calls issued since the last begin_frame().
    [[nodiscard]] u32 draw_call_count() const { return draw_call_count_; }

    /// Distinct pipeline / encoder state changes since begin_frame().
    [[nodiscard]] u32 state_change_count() const { return state_change_count_; }

    [[nodiscard]] u32 live_buffer_count() const;
    [[nodiscard]] u32 live_texture_count() const;
    [[nodiscard]] u32 live_shader_count() const;
    [[nodiscard]] u32 live_pipeline_count() const;
    [[nodiscard]] u32 live_framebuffer_count() const;

    /// Internal device pointer (id<MTLDevice>) — used by platform-layer code
    /// that bridges a CAMetalLayer's drawable back into this RHI.  Treated as
    /// opaque outside the .mm translation unit.
    [[nodiscard]] void* raw_device() const { return device_; }

private:
    // Opaque Metal objects stored as `void*` so the header stays pure C++.
    // The .mm translation unit bridges back to the proper `id<...>` pointers.
    void* device_          = nullptr;  // id<MTLDevice>
    void* command_queue_   = nullptr;  // id<MTLCommandQueue>
    void* current_cmd_     = nullptr;  // id<MTLCommandBuffer>
    void* current_encoder_ = nullptr;  // id<MTLRenderCommandEncoder>
    void* offscreen_color_ = nullptr;  // id<MTLTexture> — default back-buffer
    void* offscreen_depth_ = nullptr;  // id<MTLTexture> — default depth buffer

    bool        initialized_{false};
    bool        in_frame_{false};
    u32         draw_call_count_{0};
    u32         state_change_count_{0};
    std::string device_name_;

    // Pending clear values applied when the next encoder is opened.
    Vec4  pending_clear_color_{0.0f, 0.0f, 0.0f, 1.0f};
    float pending_clear_depth_{1.0f};
    bool  pending_clear_{false};

    // Currently bound pipeline / resource handles (resolved into live Metal
    // objects when the encoder is active).
    PipelineHandle    bound_pipeline_{INVALID_HANDLE};
    ShaderHandle      bound_shader_{INVALID_HANDLE};
    BufferHandle      bound_vertex_buffer_{INVALID_HANDLE};
    BufferHandle      bound_index_buffer_{INVALID_HANDLE};
    FramebufferHandle bound_framebuffer_{INVALID_HANDLE};
    BlendMode         blend_mode_{BlendMode::None};
    bool              depth_test_{true};
    bool              depth_write_{true};
    CullMode          cull_mode_{CullMode::Back};

    // ── Resource records ────────────────────────────────────────────────────
    struct BufferRecord {
        bool        alive{false};
        BufferType  type{BufferType::Vertex};
        BufferUsage usage{BufferUsage::Static};
        void*       mtl_buffer{nullptr};  // id<MTLBuffer>
        size_t      size{0};
    };

    struct TextureRecord {
        bool          alive{false};
        u32           width{0};
        u32           height{0};
        TextureFormat format{TextureFormat::RGBA8};
        void*         mtl_texture{nullptr};  // id<MTLTexture>
    };

    struct ShaderRecord {
        bool        alive{false};
        std::string vertex_src;
        std::string fragment_src;
        // Translated MSL sources (built on first use in create_pipeline).
        std::string msl_source;
        void*       library{nullptr};     // id<MTLLibrary>
        void*       vertex_fn{nullptr};   // id<MTLFunction>
        void*       fragment_fn{nullptr}; // id<MTLFunction>
        std::unordered_map<std::string, i32>   uniform_ints;
        std::unordered_map<std::string, float> uniform_floats;
        std::unordered_map<std::string, Vec2>  uniform_vec2s;
        std::unordered_map<std::string, Vec3>  uniform_vec3s;
        std::unordered_map<std::string, Vec4>  uniform_vec4s;
        std::unordered_map<std::string, Mat4>  uniform_mat4s;
    };

    struct PipelineRecord {
        bool         alive{false};
        PipelineDesc desc;
        void*        mtl_pipeline{nullptr};     // id<MTLRenderPipelineState>
        void*        mtl_depth_state{nullptr};  // id<MTLDepthStencilState>
    };

    struct FramebufferRecord {
        bool alive{false};
        u32  width{0};
        u32  height{0};
        std::vector<TextureFormat> color_formats;
        bool has_depth{false};
        // Owned attachment textures (allocated in create_framebuffer).
        std::vector<void*> color_textures;  // id<MTLTexture>
        void* depth_texture{nullptr};       // id<MTLTexture>
    };

    std::vector<BufferRecord>      buffers_;
    std::vector<TextureRecord>     textures_;
    std::vector<ShaderRecord>      shaders_;
    std::vector<PipelineRecord>    pipelines_;
    std::vector<FramebufferRecord> framebuffers_;
};

} // namespace nexus::rhi
