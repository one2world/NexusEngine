#include "nexus/renderer/mobile_renderer.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>

namespace nexus {

namespace mobile_shaders {

const char* MOBILE_VERT = R"(#version 300 es
precision highp float;

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_NormalMatrix;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal = normalize(mat3(u_NormalMatrix) * a_Normal);
    v_TexCoord = a_TexCoord;
    gl_Position = u_Projection * u_View * worldPos;
}
)";

const char* MOBILE_FRAG = R"(#version 300 es
precision mediump float;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec4 u_BaseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_CameraPos;
uniform vec3 u_AmbientColor;
uniform float u_AmbientIntensity;

// Up to 4 lights
uniform int u_LightCount;
uniform vec3 u_LightPositions[4];
uniform vec3 u_LightColors[4];
uniform float u_LightIntensities[4];
uniform float u_LightRadii[4];
uniform int u_LightIsDirectional[4];
uniform vec3 u_LightDirections[4];

uniform sampler2D u_AlbedoTex;
uniform int u_HasAlbedoTex;

out vec4 FragColor;

void main() {
    vec3 albedo = u_BaseColor.rgb;
    if (u_HasAlbedoTex > 0) {
        albedo *= texture(u_AlbedoTex, v_TexCoord).rgb;
    }

    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraPos - v_WorldPos);

    // Ambient
    vec3 ambient = u_AmbientColor * u_AmbientIntensity * albedo;
    vec3 result = ambient;

    // Per-light simplified PBR
    for (int i = 0; i < u_LightCount && i < 4; ++i) {
        vec3 L;
        float attenuation = 1.0;

        if (u_LightIsDirectional[i] > 0) {
            L = normalize(-u_LightDirections[i]);
        } else {
            vec3 toLight = u_LightPositions[i] - v_WorldPos;
            float dist = length(toLight);
            L = toLight / max(dist, 0.001);
            float r = u_LightRadii[i];
            attenuation = max(1.0 - (dist * dist) / (r * r), 0.0);
        }

        float NdotL = max(dot(N, L), 0.0);
        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);

        // Blinn-Phong approximation for mobile
        float shininess = mix(8.0, 256.0, 1.0 - u_Roughness);
        float spec = pow(NdotH, shininess) * (1.0 - u_Roughness);

        vec3 diffuse = albedo * NdotL;
        vec3 specular = vec3(spec) * mix(vec3(0.04), albedo, u_Metallic);

        result += (diffuse + specular) * u_LightColors[i] * u_LightIntensities[i] * attenuation;
    }

    FragColor = vec4(result, u_BaseColor.a);
}
)";

const char* MOBILE_SHADOW_VERT = R"(#version 300 es
precision highp float;
layout (location = 0) in vec3 a_Position;
uniform mat4 u_LightVP;
uniform mat4 u_Model;
void main() {
    gl_Position = u_LightVP * u_Model * vec4(a_Position, 1.0);
}
)";

const char* MOBILE_SHADOW_FRAG = R"(#version 300 es
precision mediump float;
out vec4 FragColor;
void main() {
    FragColor = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
}
)";

const char* MOBILE_SKYBOX_VERT = R"(#version 300 es
precision highp float;
layout (location = 0) in vec3 a_Position;
uniform mat4 u_ViewProjection;
out vec3 v_TexCoord;
void main() {
    v_TexCoord = a_Position;
    vec4 pos = u_ViewProjection * vec4(a_Position, 1.0);
    gl_Position = pos.xyww;
}
)";

const char* MOBILE_SKYBOX_FRAG = R"(#version 300 es
precision mediump float;
in vec3 v_TexCoord;
uniform samplerCube u_Skybox;
out vec4 FragColor;
void main() {
    FragColor = texture(u_Skybox, v_TexCoord);
}
)";

} // namespace mobile_shaders

void MobileRenderer::init(rhi::RHI* rhi, const MobileRendererConfig& config) {
    rhi_ = rhi;
    config_ = config;
    apply_quality_preset();

    main_shader_ = rhi_->create_shader(mobile_shaders::MOBILE_VERT,
                                         mobile_shaders::MOBILE_FRAG);
    if (main_shader_ == rhi::INVALID_HANDLE) {
        NX_ERROR("MobileRenderer: failed to compile main shader");
    }

    NX_INFO("MobileRenderer: initialized ({}x{}, quality={})",
            config_.render_width, config_.render_height,
            static_cast<int>(config_.quality));
}

void MobileRenderer::shutdown() {
    if (rhi_ && main_shader_ != rhi::INVALID_HANDLE) {
        rhi_->destroy_shader(main_shader_);
        main_shader_ = rhi::INVALID_HANDLE;
    }
    if (rhi_ && main_pipeline_ != rhi::INVALID_HANDLE) {
        rhi_->destroy_pipeline(main_pipeline_);
        main_pipeline_ = rhi::INVALID_HANDLE;
    }
    rhi_ = nullptr;
}

void MobileRenderer::apply_quality_preset() {
    switch (config_.quality) {
    case MobileQuality::Low:
        config_.enable_shadows = false;
        config_.enable_bloom = false;
        config_.shadow_resolution = 256;
        config_.max_lights = 2;
        config_.resolution_scale = 0.75f;
        break;
    case MobileQuality::Medium:
        config_.enable_shadows = true;
        config_.enable_bloom = false;
        config_.shadow_resolution = 512;
        config_.max_lights = 4;
        config_.resolution_scale = 1.0f;
        break;
    case MobileQuality::High:
        config_.enable_shadows = true;
        config_.enable_bloom = true;
        config_.shadow_resolution = 1024;
        config_.max_lights = 4;
        config_.resolution_scale = 1.0f;
        break;
    }
}

void MobileRenderer::begin_frame(const Mat4& view, const Mat4& projection) {
    view_ = view;
    projection_ = projection;
    render_queue_.clear();
    lights_.clear();
    draw_calls_ = 0;
    triangle_count_ = 0;
}

void MobileRenderer::submit(const RenderItem& item) {
    render_queue_.push_back(item);
}

void MobileRenderer::set_ambient(Vec3 color, f32 intensity) {
    ambient_color_ = color;
    ambient_intensity_ = intensity;
}

void MobileRenderer::add_light(const LightData& light) {
    if (lights_.size() < config_.max_lights) {
        lights_.push_back(light);
    }
}

void MobileRenderer::end_frame() {
    if (!rhi_ || main_shader_ == rhi::INVALID_HANDLE) return;

    rhi_->bind_shader(main_shader_);

    rhi_->set_uniform_mat4(main_shader_, "u_View", view_);
    rhi_->set_uniform_mat4(main_shader_, "u_Projection", projection_);
    rhi_->set_uniform_vec3(main_shader_, "u_AmbientColor", ambient_color_);
    rhi_->set_uniform_float(main_shader_, "u_AmbientIntensity", ambient_intensity_);

    // Camera position from inverse view matrix
    Mat4 inv_view = glm::inverse(view_);
    Vec3 cam_pos = Vec3(inv_view[3]);
    rhi_->set_uniform_vec3(main_shader_, "u_CameraPos", cam_pos);

    // Upload light data
    auto light_count = static_cast<i32>(lights_.size());
    rhi_->set_uniform_int(main_shader_, "u_LightCount", light_count);

    for (u32 i = 0; i < static_cast<u32>(lights_.size()); ++i) {
        const auto& l = lights_[i];
        std::string idx = std::to_string(i);
        rhi_->set_uniform_vec3(main_shader_, ("u_LightPositions[" + idx + "]").c_str(), l.position);
        rhi_->set_uniform_vec3(main_shader_, ("u_LightColors[" + idx + "]").c_str(), l.color);
        rhi_->set_uniform_float(main_shader_, ("u_LightIntensities[" + idx + "]").c_str(), l.intensity);
        rhi_->set_uniform_float(main_shader_, ("u_LightRadii[" + idx + "]").c_str(), l.radius);
        rhi_->set_uniform_int(main_shader_, ("u_LightIsDirectional[" + idx + "]").c_str(),
                              l.is_directional ? 1 : 0);
        rhi_->set_uniform_vec3(main_shader_, ("u_LightDirections[" + idx + "]").c_str(), l.direction);
    }

    // Render all items
    for (const auto& item : render_queue_) {
        rhi_->set_uniform_mat4(main_shader_, "u_Model", item.model);
        Mat4 normal_mat4 = glm::transpose(glm::inverse(item.model));
        rhi_->set_uniform_mat4(main_shader_, "u_NormalMatrix", normal_mat4);

        rhi_->set_uniform_vec4(main_shader_, "u_BaseColor", item.base_color);
        rhi_->set_uniform_float(main_shader_, "u_Metallic", item.metallic);
        rhi_->set_uniform_float(main_shader_, "u_Roughness", item.roughness);

        i32 has_tex = (item.albedo_texture != rhi::INVALID_HANDLE) ? 1 : 0;
        rhi_->set_uniform_int(main_shader_, "u_HasAlbedoTex", has_tex);
        if (has_tex) {
            rhi_->bind_texture(item.albedo_texture, 0);
        }

        rhi_->bind_vertex_buffer(item.vbo);
        rhi_->bind_index_buffer(item.ibo);
        rhi_->draw_indexed(item.index_count);

        ++draw_calls_;
        triangle_count_ += item.index_count / 3;
    }
}

} // namespace nexus
