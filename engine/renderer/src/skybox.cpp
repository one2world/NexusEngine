#include "nexus/renderer/skybox.h"
#include "nexus/core/log.h"
#include <cmath>

namespace nexus {

// ── Shared cube vertex data (inward-facing for skybox) ─────────────────────

// 36 vertices for a unit cube (positions only, no indices needed)
static constexpr float CUBE_VERTICES[] = {
    // Back face
    -1.0f,  1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,    1.0f,  1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
    // Front face
    -1.0f, -1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,    1.0f, -1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,
    // Left face
    -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,
    // Right face
     1.0f,  1.0f, -1.0f,    1.0f, -1.0f, -1.0f,    1.0f,  1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,    1.0f, -1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
    // Bottom face
    -1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,    1.0f, -1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,
    // Top face
    -1.0f,  1.0f,  1.0f,   -1.0f,  1.0f, -1.0f,    1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,    1.0f,  1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,
};

// ── SkyboxRenderer ─────────────────────────────────────────────────────────

static const char* SKYBOX_VERT_SRC = R"(
#version 450 core
layout(location = 0) in vec3 a_position;
out vec3 v_texcoord;
uniform mat4 u_view_projection;
void main() {
    v_texcoord = a_position;
    vec4 pos = u_view_projection * vec4(a_position, 1.0);
    // Set z = w so depth is always maximum (behind everything)
    gl_Position = pos.xyww;
}
)";

static const char* SKYBOX_FRAG_SRC = R"(
#version 450 core
in vec3 v_texcoord;
out vec4 frag_color;
uniform samplerCube u_cubemap;
uniform float u_exposure;
void main() {
    vec3 color = texture(u_cubemap, v_texcoord).rgb;
    color *= u_exposure;
    frag_color = vec4(color, 1.0);
}
)";

void SkyboxRenderer::init(rhi::RHI* rhi) {
    rhi_ = rhi;
    if (!rhi_) return;

    shader_ = rhi_->create_shader(SKYBOX_VERT_SRC, SKYBOX_FRAG_SRC);
    {
        rhi::PipelineDesc desc;
        desc.shader = shader_;
        desc.depth_test = true;
        desc.depth_write = false;  // skybox writes at max depth
        desc.depth = rhi::DepthFunc::LessEqual;
        desc.cull = rhi::CullMode::None;
        desc.vertex_layout.stride = sizeof(float) * 3;
        desc.vertex_layout.attributes = {{0, 3, 0, false}};
        pipeline_ = rhi_->create_pipeline(desc);
    }

    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.size = sizeof(CUBE_VERTICES);
    vbo_desc.data = CUBE_VERTICES;
    cube_vbo_ = rhi_->create_buffer(vbo_desc);

    NX_INFO("SkyboxRenderer initialized");
}

void SkyboxRenderer::shutdown() {
    if (!rhi_) return;
    if (cube_vbo_ != rhi::INVALID_HANDLE) rhi_->destroy_buffer(cube_vbo_);
    if (pipeline_ != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(pipeline_);
    if (shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(shader_);
    rhi_ = nullptr;
}

void SkyboxRenderer::render(const Mat4& view, const Mat4& projection) {
    if (!rhi_ || cubemap_ == rhi::INVALID_HANDLE) return;

    // Strip translation from view matrix so skybox stays centered on camera
    Mat4 view_no_translate = Mat4(Mat3(view));

    // Apply rotation around Y axis
    if (rotation_ != 0.0f) {
        view_no_translate = glm::rotate(view_no_translate, rotation_, Vec3(0.0f, 1.0f, 0.0f));
    }

    Mat4 vp = projection * view_no_translate;

    rhi_->bind_pipeline(pipeline_);
    rhi_->set_uniform_mat4(shader_, "u_view_projection", vp);
    rhi_->set_uniform_float(shader_, "u_exposure", exposure_);
    rhi_->bind_texture(cubemap_, 0);
    rhi_->set_uniform_int(shader_, "u_cubemap", 0);

    rhi_->bind_vertex_buffer(cube_vbo_);
    rhi_->draw(36, 0);
}

// ── ProceduralSky ──────────────────────────────────────────────────────────

static const char* PROCEDURAL_SKY_VERT_SRC = R"(
#version 450 core
layout(location = 0) in vec3 a_position;
out vec3 v_direction;
uniform mat4 u_view_projection;
void main() {
    v_direction = a_position;
    vec4 pos = u_view_projection * vec4(a_position, 1.0);
    gl_Position = pos.xyww;
}
)";

// Simplified Preetham sky model (more portable than Hosek-Wilkie)
static const char* PROCEDURAL_SKY_FRAG_SRC = R"(
#version 450 core
in vec3 v_direction;
out vec4 frag_color;

uniform vec3 u_sun_direction;
uniform float u_turbidity;
uniform vec3 u_ground_albedo;
uniform float u_exposure;

// Perez function: F(theta, gamma) = (1 + A*exp(B/cos(theta))) * (1 + C*exp(D*gamma) + E*cos^2(gamma))
vec3 perez(float cos_theta, float gamma, float cos_gamma,
           vec3 A, vec3 B, vec3 C, vec3 D, vec3 E) {
    return (1.0 + A * exp(B / max(cos_theta, 0.01))) *
           (1.0 + C * exp(D * gamma) + E * cos_gamma * cos_gamma);
}

void main() {
    vec3 dir = normalize(v_direction);

    // Below horizon: ground color
    if (dir.y < 0.0) {
        frag_color = vec4(u_ground_albedo * 0.3, 1.0);
        return;
    }

    float cos_theta = max(dir.y, 0.001);
    float cos_gamma = dot(dir, u_sun_direction);
    float gamma_angle = acos(clamp(cos_gamma, -1.0, 1.0));

    float cos_theta_s = max(u_sun_direction.y, 0.001);

    // Turbidity-dependent Perez coefficients (simplified)
    float T = u_turbidity;
    vec3 A = vec3(-0.0193 * T - 0.2592);
    vec3 B = vec3(-0.0665 * T + 0.0008);
    vec3 C = vec3(-0.0004 * T + 0.2125);
    vec3 D = vec3(-0.0641 * T - 0.8989);
    vec3 E = vec3(-0.0033 * T + 0.0452);

    vec3 F_theta_gamma = perez(cos_theta, gamma_angle, cos_gamma, A, B, C, D, E);
    vec3 F_0_theta_s   = perez(cos_theta_s, 0.0, 1.0, A, B, C, D, E);

    // Zenith luminance/chrominance (simplified)
    float chi = (4.0/9.0 - T/120.0) * (3.14159 - 2.0 * acos(cos_theta_s));
    float Yz = (4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192;
    Yz = max(Yz, 0.0);

    vec3 zenith_color = vec3(0.3, 0.5, 0.9) * Yz;

    vec3 sky_color = zenith_color * F_theta_gamma / max(F_0_theta_s, vec3(0.001));

    // Add sun disk
    if (cos_gamma > 0.9997) {
        sky_color += vec3(100.0);
    } else if (cos_gamma > 0.9990) {
        float sun_t = (cos_gamma - 0.9990) / 0.0007;
        sky_color += vec3(100.0) * sun_t * sun_t;
    }

    sky_color *= u_exposure;

    frag_color = vec4(sky_color, 1.0);
}
)";

void ProceduralSky::init(rhi::RHI* rhi) {
    rhi_ = rhi;
    if (!rhi_) return;

    shader_ = rhi_->create_shader(PROCEDURAL_SKY_VERT_SRC, PROCEDURAL_SKY_FRAG_SRC);
    {
        rhi::PipelineDesc desc;
        desc.shader = shader_;
        desc.depth_test = true;
        desc.depth_write = false;  // skybox writes at max depth
        desc.depth = rhi::DepthFunc::LessEqual;
        desc.cull = rhi::CullMode::None;
        desc.vertex_layout.stride = sizeof(float) * 3;
        desc.vertex_layout.attributes = {{0, 3, 0, false}};
        pipeline_ = rhi_->create_pipeline(desc);
    }

    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.size = sizeof(CUBE_VERTICES);
    vbo_desc.data = CUBE_VERTICES;
    cube_vbo_ = rhi_->create_buffer(vbo_desc);

    NX_INFO("ProceduralSky initialized");
}

void ProceduralSky::shutdown() {
    if (!rhi_) return;
    if (cube_vbo_ != rhi::INVALID_HANDLE) rhi_->destroy_buffer(cube_vbo_);
    if (pipeline_ != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(pipeline_);
    if (shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(shader_);
    rhi_ = nullptr;
}

void ProceduralSky::render(const Mat4& view, const Mat4& projection) {
    if (!rhi_) return;

    Mat4 view_no_translate = Mat4(Mat3(view));
    Mat4 vp = projection * view_no_translate;

    rhi_->bind_pipeline(pipeline_);
    rhi_->set_uniform_mat4(shader_, "u_view_projection", vp);
    rhi_->set_uniform_vec3(shader_, "u_sun_direction", sun_direction_);
    rhi_->set_uniform_float(shader_, "u_turbidity", turbidity_);
    rhi_->set_uniform_vec3(shader_, "u_ground_albedo", ground_albedo_);
    rhi_->set_uniform_float(shader_, "u_exposure", exposure_);

    rhi_->bind_vertex_buffer(cube_vbo_);
    rhi_->draw(36, 0);
}

// ── ReflectionProbeManager ─────────────────────────────────────────────────

void ReflectionProbeManager::init(rhi::RHI* rhi) {
    rhi_ = rhi;
}

void ReflectionProbeManager::shutdown() {
    for (auto& probe : probes_) {
        if (probe.cubemap != rhi::INVALID_HANDLE && rhi_) {
            rhi_->destroy_texture(probe.cubemap);
        }
    }
    probes_.clear();
    if (capture_fb_ != rhi::INVALID_HANDLE && rhi_) {
        rhi_->destroy_framebuffer(capture_fb_);
    }
    rhi_ = nullptr;
}

u32 ReflectionProbeManager::add_probe(const ReflectionProbe& probe) {
    auto index = static_cast<u32>(probes_.size());
    probes_.push_back(probe);
    return index;
}

void ReflectionProbeManager::remove_probe(u32 index) {
    if (index < probes_.size()) {
        if (probes_[index].cubemap != rhi::INVALID_HANDLE && rhi_) {
            rhi_->destroy_texture(probes_[index].cubemap);
        }
        probes_.erase(probes_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

const ReflectionProbe* ReflectionProbeManager::find_probe(Vec3 world_pos) const {
    const ReflectionProbe* best = nullptr;
    float best_dist = std::numeric_limits<float>::max();

    for (auto& probe : probes_) {
        // Check if point is inside probe's AABB
        Vec3 local = world_pos - probe.position;
        if (local.x >= probe.box_min.x && local.x <= probe.box_max.x &&
            local.y >= probe.box_min.y && local.y <= probe.box_max.y &&
            local.z >= probe.box_min.z && local.z <= probe.box_max.z) {
            float dist = glm::length(local);
            if (dist < best_dist) {
                best_dist = dist;
                best = &probe;
            }
        }
    }

    // Fallback: return closest probe even if not inside AABB
    if (!best && !probes_.empty()) {
        for (auto& probe : probes_) {
            float dist = glm::length(world_pos - probe.position);
            if (dist < best_dist) {
                best_dist = dist;
                best = &probe;
            }
        }
    }

    return best;
}

void ReflectionProbeManager::bake_probe(u32 index, RenderCallback render_scene) {
    if (!rhi_ || index >= probes_.size()) return;

    auto& probe = probes_[index];
    u32 res = probe.resolution;

    // Create cubemap texture if needed
    if (probe.cubemap == rhi::INVALID_HANDLE) {
        rhi::TextureDesc desc;
        desc.width = res;
        desc.height = res;
        desc.format = rhi::TextureFormat::RGBA16F;
        desc.generate_mipmaps = true;
        probe.cubemap = rhi_->create_texture(desc);
    }

    // Render each face
    Vec3 pos = probe.position;
    struct FaceInfo { Vec3 target; Vec3 up; };
    static constexpr std::array<FaceInfo, 6> faces = {{
        {{1, 0, 0}, {0, -1, 0}},   // +X
        {{-1, 0, 0}, {0, -1, 0}},  // -X
        {{0, 1, 0}, {0, 0, 1}},    // +Y
        {{0, -1, 0}, {0, 0, -1}},  // -Y
        {{0, 0, 1}, {0, -1, 0}},   // +Z
        {{0, 0, -1}, {0, -1, 0}},  // -Z
    }};

    Mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

    for (u32 face = 0; face < 6; ++face) {
        Mat4 view = glm::lookAt(pos, pos + faces[face].target, faces[face].up);
        Mat4 vp = projection * view;
        render_scene(vp, face);
    }

    probe.needs_rebake = false;
    NX_INFO("Reflection probe {} baked at ({}, {}, {})", index, pos.x, pos.y, pos.z);
}

} // namespace nexus
