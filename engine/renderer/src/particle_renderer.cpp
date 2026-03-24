#include "nexus/renderer/particle_renderer.h"
#include "nexus/animation/particle_system.h"
#include <algorithm>
#include <cmath>

namespace nexus {

// ── Shaders ─────────────────────────────────────────────────────────────────

static const char* PARTICLE_VERTEX = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Size;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    vec3 worldPos = a_Position
        + u_CameraRight * a_TexCoord.x * a_Size
        + u_CameraUp * a_TexCoord.y * a_Size;
    gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
    v_TexCoord = a_TexCoord * 0.5 + 0.5; // map [-1,1] to [0,1]
    v_Color = a_Color;
}
)";

static const char* PARTICLE_FRAGMENT = R"(
#version 330 core
in vec2 v_TexCoord;
in vec4 v_Color;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform int u_HasTexture;

void main() {
    vec4 texColor = u_HasTexture != 0 ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    FragColor = texColor * v_Color;
    if (FragColor.a < 0.01) discard;
}
)";

// ── Implementation ──────────────────────────────────────────────────────────

void ParticleRenderer::init(rhi::RHI* rhi) {
    rhi_ = rhi;

    shader_ = rhi->create_shader(PARTICLE_VERTEX, PARTICLE_FRAGMENT);

    // Alpha blend pipeline
    rhi::PipelineDesc pd{};
    pd.shader = shader_;
    pd.blend = rhi::BlendMode::Alpha;
    pd.depth_test = true;
    pd.depth_write = false;
    pd.cull = rhi::CullMode::None;
    pd.primitive = rhi::PrimitiveType::Triangles;
    pd.vertex_layout.stride = sizeof(ParticleVertex);
    pd.vertex_layout.attributes = {
        {0, 3, offsetof(ParticleVertex, position), false},
        {1, 2, offsetof(ParticleVertex, uv), false},
        {2, 4, offsetof(ParticleVertex, color), false},
        {3, 1, offsetof(ParticleVertex, size), false},
    };
    pipeline_alpha_ = rhi->create_pipeline(pd);

    // Additive blend pipeline
    pd.blend = rhi::BlendMode::Additive;
    pipeline_additive_ = rhi->create_pipeline(pd);

    // Dynamic VBO for particle vertices (6 vertices per particle: 2 triangles)
    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Dynamic;
    vbo_desc.data = nullptr;
    vbo_desc.size = MAX_PARTICLES * 6 * sizeof(ParticleVertex);
    vbo_ = rhi->create_buffer(vbo_desc);
}

void ParticleRenderer::shutdown() {
    // Resources managed by RHI
}

void ParticleRenderer::render(const Camera3D& camera,
                               const std::vector<anim::Particle>& particles) {
    if (particles.empty() || !rhi_) return;

    Mat4 view = camera.get_view_matrix();
    Mat4 proj = camera.get_projection_matrix();
    Mat4 vp = proj * view;

    // Extract camera right/up from view matrix
    Vec3 cam_right = Vec3(view[0][0], view[1][0], view[2][0]);
    Vec3 cam_up    = Vec3(view[0][1], view[1][1], view[2][1]);

    // Build vertex data (billboard quads)
    u32 count = std::min(static_cast<u32>(particles.size()), MAX_PARTICLES);
    std::vector<ParticleVertex> vertices;
    vertices.reserve(count * 6);

    for (u32 i = 0; i < count; ++i) {
        const auto& p = particles[i];
        if (!p.alive) continue;

        // Two triangles forming a quad centered on the particle position
        // UV coordinates are in [-1,1] for the vertex shader billboard offset
        ParticleVertex tl{p.position, {-1.0f,  1.0f}, p.color, p.size};
        ParticleVertex tr{p.position, { 1.0f,  1.0f}, p.color, p.size};
        ParticleVertex br{p.position, { 1.0f, -1.0f}, p.color, p.size};
        ParticleVertex bl{p.position, {-1.0f, -1.0f}, p.color, p.size};

        vertices.push_back(tl);
        vertices.push_back(bl);
        vertices.push_back(tr);
        vertices.push_back(tr);
        vertices.push_back(bl);
        vertices.push_back(br);
    }

    if (vertices.empty()) return;

    // Upload vertex data
    rhi_->update_buffer(vbo_, vertices.data(),
                         vertices.size() * sizeof(ParticleVertex));

    // Bind pipeline
    if (blend_mode == BlendMode::Additive) {
        rhi_->bind_pipeline(pipeline_additive_);
    } else {
        rhi_->bind_pipeline(pipeline_alpha_);
    }

    // Set uniforms
    rhi_->set_uniform_mat4(shader_, "u_ViewProjection", vp);
    rhi_->set_uniform_vec3(shader_, "u_CameraRight", cam_right);
    rhi_->set_uniform_vec3(shader_, "u_CameraUp", cam_up);

    if (texture_ != rhi::INVALID_HANDLE) {
        rhi_->bind_texture(texture_, 0);
        rhi_->set_uniform_int(shader_, "u_Texture", 0);
        rhi_->set_uniform_int(shader_, "u_HasTexture", 1);
    } else {
        rhi_->set_uniform_int(shader_, "u_HasTexture", 0);
    }

    rhi_->bind_vertex_buffer(vbo_);
    rhi_->draw(static_cast<u32>(vertices.size()), 0);
}

} // namespace nexus
