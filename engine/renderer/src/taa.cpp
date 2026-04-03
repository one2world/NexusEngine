#include "nexus/renderer/taa.h"
#include "nexus/core/log.h"

namespace nexus {

// ── Halton low-discrepancy sequence ────────────────────────────────────────

float TAAEffect::halton(u32 index, u32 base) {
    float f = 1.0f;
    float r = 0.0f;
    u32 i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

Vec2 TAAEffect::compute_jitter(u32 frame) const {
    u32 idx = (frame % JITTER_SEQUENCE_LENGTH) + 1; // avoid 0
    float jx = halton(idx, 2) - 0.5f; // center around 0
    float jy = halton(idx, 3) - 0.5f;
    return Vec2(jx, jy);
}

// ── TAA shader ─────────────────────────────────────────────────────────────

static const char* TAA_VERT_SRC = R"(
#version 450 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;
void main() {
    v_texcoord = a_texcoord;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

static const char* TAA_FRAG_SRC = R"(
#version 450 core
in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_current;       // Current frame (jittered)
uniform sampler2D u_history;       // Previous frame (resolved)
uniform sampler2D u_motion;        // Motion vectors (RG = velocity in UV space)

uniform float u_feedback;          // Blend factor (0.9 typical)
uniform int u_clamp_history;       // Whether to apply neighborhood clamping
uniform vec2 u_texel_size;         // 1.0 / resolution

// Sample 3x3 neighborhood for color clamping
void sample_neighborhood(vec2 uv, out vec3 min_color, out vec3 max_color) {
    min_color = vec3(9999.0);
    max_color = vec3(-9999.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 c = texture(u_current, uv + vec2(x, y) * u_texel_size).rgb;
            min_color = min(min_color, c);
            max_color = max(max_color, c);
        }
    }
}

void main() {
    vec3 current_color = texture(u_current, v_texcoord).rgb;

    // Sample motion vectors to find where this pixel was last frame
    vec2 motion = texture(u_motion, v_texcoord).rg;
    vec2 history_uv = v_texcoord - motion;

    // Reject history if reprojected UV is out of screen
    if (history_uv.x < 0.0 || history_uv.x > 1.0 ||
        history_uv.y < 0.0 || history_uv.y > 1.0) {
        frag_color = vec4(current_color, 1.0);
        return;
    }

    vec3 history_color = texture(u_history, history_uv).rgb;

    // Neighborhood clamping to reduce ghosting
    if (u_clamp_history != 0) {
        vec3 min_c, max_c;
        sample_neighborhood(v_texcoord, min_c, max_c);
        history_color = clamp(history_color, min_c, max_c);
    }

    // Blend current and history
    vec3 result = mix(current_color, history_color, u_feedback);

    frag_color = vec4(result, 1.0);
}
)";

// ── Fullscreen quad data ───────────────────────────────────────────────────

static constexpr float QUAD_VERTICES[] = {
    // position (xy), texcoord (uv)
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
};

// ── TAAEffect implementation ───────────────────────────────────────────────

void TAAEffect::init(rhi::RHI* rhi, u32 width, u32 height) {
    width_ = width;
    height_ = height;

    shader_ = rhi->create_shader(TAA_VERT_SRC, TAA_FRAG_SRC);

    rhi::PipelineDesc pipe_desc;
    pipe_desc.shader = shader_;
    pipe_desc.blend = rhi::BlendMode::None;
    pipe_desc.depth_test = false;
    pipe_desc.depth_write = false;
    pipe_desc.cull = rhi::CullMode::None;
    pipe_desc.primitive = rhi::PrimitiveType::Triangles;
    pipe_desc.vertex_layout.stride = sizeof(float) * 4;
    pipe_desc.vertex_layout.attributes = {
        {0, 2, 0, false},
        {1, 2, sizeof(float) * 2, false},
    };
    pipeline_ = rhi->create_pipeline(pipe_desc);

    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.size = sizeof(QUAD_VERTICES);
    vbo_desc.data = QUAD_VERTICES;
    quad_vbo_ = rhi->create_buffer(vbo_desc);

    // Create double-buffered history textures and framebuffers
    for (u32 i = 0; i < 2; ++i) {
        rhi::TextureDesc tex_desc;
        tex_desc.width = width;
        tex_desc.height = height;
        tex_desc.format = rhi::TextureFormat::RGBA16F;
        tex_desc.generate_mipmaps = false;
        history_tex_[i] = rhi->create_texture(tex_desc);

        rhi::FramebufferDesc fb_desc;
        fb_desc.width = width;
        fb_desc.height = height;
        fb_desc.color_attachments = {rhi::TextureFormat::RGBA16F};
        fb_desc.has_depth = false;
        history_fb_[i] = rhi->create_framebuffer(fb_desc);
    }

    NX_INFO("TAA initialized ({}x{})", width, height);
}

void TAAEffect::shutdown() {
    // Resources cleaned up by RHI shutdown
    shader_ = rhi::INVALID_HANDLE;
    pipeline_ = rhi::INVALID_HANDLE;
    quad_vbo_ = rhi::INVALID_HANDLE;
    for (u32 i = 0; i < 2; ++i) {
        history_fb_[i] = rhi::INVALID_HANDLE;
        history_tex_[i] = rhi::INVALID_HANDLE;
    }
}

void TAAEffect::resize(u32 width, u32 height) {
    width_ = width;
    height_ = height;
    // History buffers would need to be recreated here in a full implementation.
    // For now, TAA quality will degrade until next init.
}

void TAAEffect::advance_frame() {
    jitter_pixels_ = compute_jitter(frame_index_);
    jitter_ndc_ = Vec2(
        jitter_pixels_.x * 2.0f / static_cast<float>(width_ > 0 ? width_ : 1),
        jitter_pixels_.y * 2.0f / static_cast<float>(height_ > 0 ? height_ : 1)
    );
    ++frame_index_;
}

void TAAEffect::apply(rhi::RHI* rhi, rhi::TextureHandle input,
                       rhi::FramebufferHandle dest) {
    u32 current_history = history_index_;
    u32 prev_history = 1 - history_index_;

    rhi->bind_framebuffer(dest);
    rhi->bind_pipeline(pipeline_);

    // Bind textures
    rhi->bind_texture(input, 0);
    rhi->set_uniform_int(shader_, "u_current", 0);

    rhi->bind_texture(history_tex_[prev_history], 1);
    rhi->set_uniform_int(shader_, "u_history", 1);

    // Motion vectors (or black if not set)
    if (motion_vectors_ != rhi::INVALID_HANDLE) {
        rhi->bind_texture(motion_vectors_, 2);
    }
    rhi->set_uniform_int(shader_, "u_motion", 2);

    rhi->set_uniform_float(shader_, "u_feedback", feedback_factor);
    rhi->set_uniform_int(shader_, "u_clamp_history", clamp_history ? 1 : 0);
    rhi->set_uniform_vec2(shader_, "u_texel_size",
                          Vec2(1.0f / static_cast<float>(width_),
                               1.0f / static_cast<float>(height_)));

    rhi->bind_vertex_buffer(quad_vbo_);
    rhi->draw(6, 0);

    // Also render into the current history buffer for next frame
    rhi->bind_framebuffer(history_fb_[current_history]);
    rhi->bind_texture(input, 0);
    rhi->bind_texture(history_tex_[prev_history], 1);
    if (motion_vectors_ != rhi::INVALID_HANDLE) {
        rhi->bind_texture(motion_vectors_, 2);
    }
    rhi->bind_vertex_buffer(quad_vbo_);
    rhi->draw(6, 0);

    // Swap history buffers
    history_index_ = prev_history;
}

} // namespace nexus
