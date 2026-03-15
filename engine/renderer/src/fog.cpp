#include "nexus/renderer/fog.h"
#include "nexus/core/log.h"
#include <cmath>

namespace nexus {

// ── FogSettings ─────────────────────────────────────────────────────────────

float FogSettings::compute_factor(float distance, float world_y) const {
    switch (mode) {
        case Mode::None:
            return 1.0f;

        case Mode::Linear: {
            float f = (far_distance - distance) / (far_distance - near_distance);
            return math::clamp(f, 0.0f, 1.0f);
        }

        case Mode::Exponential: {
            float f = std::exp(-density * distance);
            return math::clamp(f, 0.0f, 1.0f);
        }

        case Mode::ExponentialSquared: {
            float d = density * distance;
            float f = std::exp(-d * d);
            return math::clamp(f, 0.0f, 1.0f);
        }

        case Mode::HeightBased: {
            // Distance-based exponential fog combined with height falloff
            float dist_fog = std::exp(-density * distance);

            // Height attenuation: fog is densest at base_height, fades above
            float height_factor = 1.0f;
            if (world_y > base_height) {
                float relative_height = (world_y - base_height) / (max_height - base_height);
                height_factor = std::exp(-height_falloff * relative_height);
                height_factor = math::clamp(height_factor, 0.0f, 1.0f);
            }

            float f = 1.0f - (1.0f - dist_fog) * height_factor;
            return math::clamp(f, 0.0f, 1.0f);
        }
    }
    return 1.0f;
}

// ── FogPass ─────────────────────────────────────────────────────────────────

namespace fog_shaders {

const char* VERTEX = R"(
#version 330 core
layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

const char* FRAGMENT = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneColor;
uniform sampler2D u_DepthTexture;

uniform int   u_FogMode;       // 0=None, 1=Linear, 2=Exp, 3=Exp2, 4=HeightBased
uniform vec3  u_FogColor;
uniform float u_FogDensity;
uniform float u_FogNear;
uniform float u_FogFar;
uniform float u_HeightFalloff;
uniform float u_BaseHeight;
uniform float u_MaxHeight;

uniform mat4  u_InvViewProjection;
uniform vec3  u_CameraPos;
uniform float u_NearClip;
uniform float u_FarClip;

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_NearClip * u_FarClip) / (u_FarClip + u_NearClip - z * (u_FarClip - u_NearClip));
}

vec3 worldPosFromDepth(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = u_InvViewProjection * clipPos;
    return worldPos.xyz / worldPos.w;
}

void main() {
    vec3 sceneColor = texture(u_SceneColor, v_TexCoord).rgb;

    if (u_FogMode == 0) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    float depth = texture(u_DepthTexture, v_TexCoord).r;
    vec3 worldPos = worldPosFromDepth(v_TexCoord, depth);
    float dist = length(worldPos - u_CameraPos);

    float fogFactor = 1.0;

    if (u_FogMode == 1) {
        // Linear
        fogFactor = clamp((u_FogFar - dist) / (u_FogFar - u_FogNear), 0.0, 1.0);
    } else if (u_FogMode == 2) {
        // Exponential
        fogFactor = clamp(exp(-u_FogDensity * dist), 0.0, 1.0);
    } else if (u_FogMode == 3) {
        // Exponential squared
        float d = u_FogDensity * dist;
        fogFactor = clamp(exp(-d * d), 0.0, 1.0);
    } else if (u_FogMode == 4) {
        // Height-based
        float distFog = exp(-u_FogDensity * dist);
        float heightFactor = 1.0;
        if (worldPos.y > u_BaseHeight) {
            float rel = (worldPos.y - u_BaseHeight) / (u_MaxHeight - u_BaseHeight);
            heightFactor = clamp(exp(-u_HeightFalloff * rel), 0.0, 1.0);
        }
        fogFactor = clamp(1.0 - (1.0 - distFog) * heightFactor, 0.0, 1.0);
    }

    vec3 finalColor = mix(u_FogColor, sceneColor, fogFactor);
    FragColor = vec4(finalColor, 1.0);
}
)";

} // namespace fog_shaders

static const float QUAD_VERTICES[] = {
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
};

void FogPass::init(rhi::RHI* rhi) {
    shader_ = rhi->create_shader(fog_shaders::VERTEX, fog_shaders::FRAGMENT);

    rhi::PipelineDesc desc;
    desc.shader = shader_;
    desc.blend = rhi::BlendMode::None;
    desc.depth_test = false;
    desc.depth_write = false;
    desc.cull = rhi::CullMode::None;
    desc.primitive = rhi::PrimitiveType::Triangles;
    desc.vertex_layout.stride = sizeof(float) * 4;
    desc.vertex_layout.attributes = {
        {0, 2, 0, false},
        {1, 2, sizeof(float) * 2, false},
    };
    pipeline_ = rhi->create_pipeline(desc);

    rhi::BufferDesc buf_desc;
    buf_desc.type = rhi::BufferType::Vertex;
    buf_desc.usage = rhi::BufferUsage::Static;
    buf_desc.data = QUAD_VERTICES;
    buf_desc.size = sizeof(QUAD_VERTICES);
    quad_vbo_ = rhi->create_buffer(buf_desc);
}

void FogPass::shutdown() {}

void FogPass::apply(rhi::RHI* rhi,
                     rhi::TextureHandle scene_color,
                     rhi::TextureHandle depth_texture,
                     rhi::FramebufferHandle dest,
                     const FogSettings& settings,
                     const Mat4& inverse_view_projection,
                     Vec3 camera_position,
                     float near_clip,
                     float far_clip) {
    rhi->bind_framebuffer(dest);
    rhi->bind_shader(shader_);
    rhi->bind_pipeline(pipeline_);

    rhi->bind_texture(scene_color, 0);
    rhi->set_uniform_int(shader_, "u_SceneColor", 0);
    rhi->bind_texture(depth_texture, 1);
    rhi->set_uniform_int(shader_, "u_DepthTexture", 1);

    rhi->set_uniform_int(shader_, "u_FogMode", static_cast<i32>(settings.mode));
    rhi->set_uniform_vec3(shader_, "u_FogColor", settings.color);
    rhi->set_uniform_float(shader_, "u_FogDensity", settings.density);
    rhi->set_uniform_float(shader_, "u_FogNear", settings.near_distance);
    rhi->set_uniform_float(shader_, "u_FogFar", settings.far_distance);
    rhi->set_uniform_float(shader_, "u_HeightFalloff", settings.height_falloff);
    rhi->set_uniform_float(shader_, "u_BaseHeight", settings.base_height);
    rhi->set_uniform_float(shader_, "u_MaxHeight", settings.max_height);

    rhi->set_uniform_mat4(shader_, "u_InvViewProjection", inverse_view_projection);
    rhi->set_uniform_vec3(shader_, "u_CameraPos", camera_position);
    rhi->set_uniform_float(shader_, "u_NearClip", near_clip);
    rhi->set_uniform_float(shader_, "u_FarClip", far_clip);

    rhi->bind_vertex_buffer(quad_vbo_);
    rhi->draw(6);
}

} // namespace nexus
