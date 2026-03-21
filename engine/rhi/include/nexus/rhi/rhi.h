#pragma once

#include <nexus/rhi/rhi_types.h>
#include <memory>
#include <string>

namespace nexus::rhi {

enum class Backend : u8 {
    OpenGL,
    Vulkan,
};

class RHI {
public:
    virtual ~RHI() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────
    virtual bool init()    = 0;
    virtual void shutdown() = 0;

    // ── Resource creation / destruction ────────────────────────────────
    virtual BufferHandle create_buffer(const BufferDesc& desc)   = 0;
    virtual void         destroy_buffer(BufferHandle handle)     = 0;
    virtual void         update_buffer(BufferHandle handle,
                                       const void* data,
                                       size_t size,
                                       size_t offset = 0)       = 0;

    virtual TextureHandle create_texture(const TextureDesc& desc) = 0;
    virtual void          destroy_texture(TextureHandle handle)   = 0;

    virtual ShaderHandle create_shader(const std::string& vertex_src,
                                       const std::string& fragment_src) = 0;
    virtual void         destroy_shader(ShaderHandle handle)            = 0;

    virtual PipelineHandle create_pipeline(const PipelineDesc& desc) = 0;
    virtual void           destroy_pipeline(PipelineHandle handle)   = 0;

    virtual FramebufferHandle create_framebuffer(const FramebufferDesc& desc) = 0;
    virtual void              destroy_framebuffer(FramebufferHandle handle)   = 0;

    // ── Render commands ───────────────────────────────────────────────
    virtual void begin_frame() = 0;
    virtual void end_frame()   = 0;

    virtual void set_viewport(i32 x, i32 y, i32 w, i32 h) = 0;
    virtual void set_scissor(i32 x, i32 y, i32 w, i32 h)  = 0;
    virtual void clear(Vec4 color, float depth = 1.0f)     = 0;

    virtual void bind_pipeline(PipelineHandle handle)         = 0;
    virtual void bind_shader(ShaderHandle handle)              = 0;
    virtual void bind_texture(TextureHandle handle, u32 slot = 0) = 0;
    virtual void bind_framebuffer(FramebufferHandle handle)   = 0;
    virtual void unbind_framebuffer()                         = 0;
    virtual void bind_vertex_buffer(BufferHandle handle)      = 0;
    virtual void bind_index_buffer(BufferHandle handle)       = 0;

    // ── State toggles ─────────────────────────────────────────────────
    virtual void set_blend_mode(BlendMode mode) = 0;
    virtual void set_depth_test(bool enabled)   = 0;

    // ── Uniforms ──────────────────────────────────────────────────────
    virtual void set_uniform_int(ShaderHandle shader,
                                 const std::string& name, i32 value)      = 0;
    virtual void set_uniform_int_array(ShaderHandle shader,
                                       const std::string& name,
                                       const i32* values, u32 count)      = 0;
    virtual void set_uniform_float(ShaderHandle shader,
                                   const std::string& name, float value)  = 0;
    virtual void set_uniform_vec2(ShaderHandle shader,
                                  const std::string& name, Vec2 value)    = 0;
    virtual void set_uniform_vec3(ShaderHandle shader,
                                  const std::string& name, Vec3 value)    = 0;
    virtual void set_uniform_vec4(ShaderHandle shader,
                                  const std::string& name, Vec4 value)    = 0;
    virtual void set_uniform_mat4(ShaderHandle shader,
                                  const std::string& name,
                                  const Mat4& value)                      = 0;

    // ── Draw calls ────────────────────────────────────────────────────
    virtual void draw(u32 vertex_count, u32 first_vertex = 0)   = 0;
    virtual void draw_indexed(u32 index_count, u32 first_index = 0) = 0;

    // ── Factory ───────────────────────────────────────────────────────
    /// Create the default RHI backend (OpenGL).
    static std::unique_ptr<RHI> create();

    /// Create a specific RHI backend.
    static std::unique_ptr<RHI> create(Backend backend);
};

} // namespace nexus::rhi
