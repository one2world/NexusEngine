#include "nexus/renderer/pbr_renderer.h"
#include "nexus/renderer/camera.h"
#include "nexus/core/log.h"

namespace nexus {

// ── PBR Shaders ─────────────────────────────────────────────────────────────

namespace pbr_shaders {

const char* VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal = mat3(u_NormalMatrix) * a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * worldPos;
}
)";

const char* FRAGMENT_SHADER = R"(
#version 330 core
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

out vec4 FragColor;

// Material uniforms
uniform vec4  u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3  u_Emissive;
uniform float u_EmissiveStrength;
uniform float u_AO;
uniform float u_AlphaCutoff;

// Lighting
uniform vec3  u_SunDirection;
uniform vec3  u_SunColor;
uniform float u_SunIntensity;
uniform vec3  u_CameraPos;
uniform float u_Exposure;

// Texture flags
uniform int   u_HasAlbedoMap;
uniform sampler2D u_AlbedoMap;
uniform int   u_HasNormalMap;
uniform sampler2D u_NormalMap;
uniform float u_NormalStrength;
uniform int   u_HasMetallicRoughnessMap;
uniform sampler2D u_MetallicRoughnessMap;
uniform int   u_HasAOMap;
uniform sampler2D u_AOMap;
uniform int   u_HasEmissiveMap;
uniform sampler2D u_EmissiveMap;

// IBL
uniform int         u_HasIBL;
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredMap;
uniform sampler2D   u_BrdfLUT;
uniform float       u_IBLIntensity;

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// GGX/Trowbridge-Reitz NDF
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

// Schlick-GGX geometry function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for IBL
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 albedo = u_Albedo;
    if (u_HasAlbedoMap > 0) {
        albedo *= texture(u_AlbedoMap, v_TexCoord);
    }

    // Alpha cutoff (discard fragments below threshold)
    if (albedo.a < u_AlphaCutoff) {
        discard;
    }

    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessMap > 0) {
        vec4 mr = texture(u_MetallicRoughnessMap, v_TexCoord);
        roughness *= mr.g;  // glTF: green = roughness
        metallic *= mr.b;   // glTF: blue = metallic
    }

    float ao = u_AO;
    if (u_HasAOMap > 0) {
        ao *= texture(u_AOMap, v_TexCoord).r;
    }

    vec3 N = normalize(v_Normal);
    if (u_HasNormalMap > 0) {
        // Derive TBN from screen-space derivatives
        vec3 dPdx = dFdx(v_WorldPos);
        vec3 dPdy = dFdy(v_WorldPos);
        vec2 dUVdx = dFdx(v_TexCoord);
        vec2 dUVdy = dFdy(v_TexCoord);
        float det = dUVdx.x * dUVdy.y - dUVdx.y * dUVdy.x;
        vec3 T = normalize((dPdx * dUVdy.y - dPdy * dUVdx.y) / max(abs(det), 0.0001));
        vec3 B = normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        vec3 tangentNormal = texture(u_NormalMap, v_TexCoord).rgb * 2.0 - 1.0;
        tangentNormal.xy *= u_NormalStrength;
        N = normalize(TBN * tangentNormal);
    }

    vec3 V = normalize(u_CameraPos - v_WorldPos);
    vec3 L = normalize(-u_SunDirection);
    vec3 H = normalize(V + L);

    // F0 for dielectrics is 0.04, for metals it's the albedo color
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);

    vec3 Lo = (kD * albedo.rgb / PI + specular) * u_SunColor * u_SunIntensity * NdotL;

    // Ambient — use IBL when available, otherwise constant
    vec3 ambient;
    if (u_HasIBL > 0) {
        float NdotV_ibl = max(dot(N, V), 0.0);
        vec3 F_ibl = fresnelSchlickRoughness(NdotV_ibl, F0, roughness);
        vec3 kS_ibl = F_ibl;
        vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

        // Diffuse IBL from irradiance map
        vec3 irradiance = texture(u_IrradianceMap, N).rgb;
        vec3 diffuse_ibl = irradiance * albedo.rgb;

        // Specular IBL from prefiltered environment map
        vec3 R = reflect(-V, N);
        vec3 prefilteredColor = textureLod(u_PrefilteredMap, R,
                                           roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(u_BrdfLUT, vec2(NdotV_ibl, roughness)).rg;
        vec3 specular_ibl = prefilteredColor * (F_ibl * brdf.x + brdf.y);

        ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao * u_IBLIntensity;
    } else {
        ambient = vec3(0.03) * albedo.rgb * ao;
    }

    // Emissive
    vec3 emissive = u_Emissive * u_EmissiveStrength;
    if (u_HasEmissiveMap > 0) {
        emissive *= texture(u_EmissiveMap, v_TexCoord).rgb;
    }

    vec3 color = ambient + Lo + emissive;

    // Exposure tone mapping
    color = vec3(1.0) - exp(-color * u_Exposure);

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, albedo.a);
}
)";

} // namespace pbr_shaders

// ── PBRRenderer ─────────────────────────────────────────────────────────────

PBRRenderer::PBRRenderer(PBRRenderer&& other) noexcept
    : rhi_(other.rhi_), shader_(other.shader_), pipeline_(other.pipeline_),
      view_projection_(other.view_projection_), camera_position_(other.camera_position_),
      sun_direction_(other.sun_direction_), sun_color_(other.sun_color_),
      sun_intensity_(other.sun_intensity_), ibl_(other.ibl_), exposure_(other.exposure_) {
    other.rhi_ = nullptr;
    other.shader_ = rhi::INVALID_HANDLE;
    other.pipeline_ = rhi::INVALID_HANDLE;
}

PBRRenderer& PBRRenderer::operator=(PBRRenderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        rhi_ = other.rhi_; shader_ = other.shader_; pipeline_ = other.pipeline_;
        view_projection_ = other.view_projection_; camera_position_ = other.camera_position_;
        sun_direction_ = other.sun_direction_; sun_color_ = other.sun_color_;
        sun_intensity_ = other.sun_intensity_; ibl_ = other.ibl_; exposure_ = other.exposure_;
        other.rhi_ = nullptr; other.shader_ = rhi::INVALID_HANDLE; other.pipeline_ = rhi::INVALID_HANDLE;
    }
    return *this;
}

void PBRRenderer::init(rhi::RHI* rhi) {
    rhi_ = rhi;

    shader_ = rhi_->create_shader(pbr_shaders::VERTEX_SHADER,
                                   pbr_shaders::FRAGMENT_SHADER);
    if (shader_ == rhi::INVALID_HANDLE) {
        NX_ERROR("PBRRenderer: Failed to compile PBR shader");
        return;
    }

    rhi::PipelineDesc desc;
    desc.shader = shader_;
    desc.blend = rhi::BlendMode::None;
    desc.depth_test = true;
    desc.depth_write = true;
    desc.cull = rhi::CullMode::Back;
    desc.primitive = rhi::PrimitiveType::Triangles;
    desc.vertex_layout.stride = sizeof(float) * 8; // pos(3) + normal(3) + uv(2)
    desc.vertex_layout.attributes = {
        {0, 3, 0, false},                   // position
        {1, 3, sizeof(float) * 3, false},    // normal
        {2, 2, sizeof(float) * 6, false},    // texcoord
    };
    pipeline_ = rhi_->create_pipeline(desc);

    NX_INFO("PBRRenderer initialized");
}

void PBRRenderer::shutdown() {
    if (!rhi_) return;
    if (pipeline_ != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(pipeline_);
    if (shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(shader_);
    pipeline_ = rhi::INVALID_HANDLE;
    shader_ = rhi::INVALID_HANDLE;
    rhi_ = nullptr;
}

void PBRRenderer::begin(const Camera3D& camera) {
    view_projection_ = camera.get_view_projection();
    camera_position_ = camera.position;
}

void PBRRenderer::end() {
    // Flush if batching in the future
}

void PBRRenderer::set_environment(const IBLData& ibl) {
    ibl_ = ibl;
}

void PBRRenderer::set_sun(Vec3 direction, Vec3 color, float intensity) {
    sun_direction_ = direction;
    sun_color_ = color;
    sun_intensity_ = intensity;
}

void PBRRenderer::draw(rhi::BufferHandle vbo, rhi::BufferHandle ibo,
                        u32 index_count, const Mat4& transform,
                        const PBRMaterial& material) {
    if (shader_ == rhi::INVALID_HANDLE) return;

    rhi_->bind_shader(shader_);
    rhi_->bind_pipeline(pipeline_);

    // Per-material render state: transparency and double-sided
    if (material.transparent) {
        rhi_->set_blend_mode(rhi::BlendMode::Alpha);
        rhi_->set_depth_write(false);
    } else {
        rhi_->set_blend_mode(rhi::BlendMode::None);
        rhi_->set_depth_write(true);
    }
    rhi_->set_cull_mode(material.double_sided ? rhi::CullMode::None : rhi::CullMode::Back);

    // Transforms
    rhi_->set_uniform_mat4(shader_, "u_ViewProjection", view_projection_);
    rhi_->set_uniform_mat4(shader_, "u_Model", transform);
    Mat4 normal_matrix = glm::transpose(glm::inverse(transform));
    rhi_->set_uniform_mat4(shader_, "u_NormalMatrix", normal_matrix);

    // Material
    rhi_->set_uniform_vec4(shader_, "u_Albedo", material.albedo);
    rhi_->set_uniform_float(shader_, "u_Metallic", material.metallic);
    rhi_->set_uniform_float(shader_, "u_Roughness", material.roughness);
    rhi_->set_uniform_vec3(shader_, "u_Emissive", material.emissive);
    rhi_->set_uniform_float(shader_, "u_EmissiveStrength", material.emissive_strength);
    rhi_->set_uniform_float(shader_, "u_AO", material.ao_strength);
    rhi_->set_uniform_float(shader_, "u_AlphaCutoff", material.transparent ? 0.0f : material.alpha_cutoff);

    // Textures — Albedo (slot 0)
    int has_albedo = (material.albedo_map != rhi::INVALID_HANDLE) ? 1 : 0;
    rhi_->set_uniform_int(shader_, "u_HasAlbedoMap", has_albedo);
    if (has_albedo) {
        rhi_->bind_texture(material.albedo_map, 0);
        rhi_->set_uniform_int(shader_, "u_AlbedoMap", 0);
    }

    // Normal map (slot 1)
    int has_normal = (material.normal_map != rhi::INVALID_HANDLE) ? 1 : 0;
    rhi_->set_uniform_int(shader_, "u_HasNormalMap", has_normal);
    rhi_->set_uniform_float(shader_, "u_NormalStrength", material.normal_strength);
    if (has_normal) {
        rhi_->bind_texture(material.normal_map, 1);
        rhi_->set_uniform_int(shader_, "u_NormalMap", 1);
    }

    // Metallic-roughness map (slot 2)
    int has_mr = (material.metallic_roughness_map != rhi::INVALID_HANDLE) ? 1 : 0;
    rhi_->set_uniform_int(shader_, "u_HasMetallicRoughnessMap", has_mr);
    if (has_mr) {
        rhi_->bind_texture(material.metallic_roughness_map, 2);
        rhi_->set_uniform_int(shader_, "u_MetallicRoughnessMap", 2);
    }

    // AO map (slot 3)
    int has_ao = (material.ao_map != rhi::INVALID_HANDLE) ? 1 : 0;
    rhi_->set_uniform_int(shader_, "u_HasAOMap", has_ao);
    if (has_ao) {
        rhi_->bind_texture(material.ao_map, 3);
        rhi_->set_uniform_int(shader_, "u_AOMap", 3);
    }

    // Emissive map (slot 7)
    int has_emissive = (material.emissive_map != rhi::INVALID_HANDLE) ? 1 : 0;
    rhi_->set_uniform_int(shader_, "u_HasEmissiveMap", has_emissive);
    if (has_emissive) {
        rhi_->bind_texture(material.emissive_map, 7);
        rhi_->set_uniform_int(shader_, "u_EmissiveMap", 7);
    }

    // Lighting
    rhi_->set_uniform_vec3(shader_, "u_SunDirection", sun_direction_);
    rhi_->set_uniform_vec3(shader_, "u_SunColor", sun_color_);
    rhi_->set_uniform_float(shader_, "u_SunIntensity", sun_intensity_);
    rhi_->set_uniform_vec3(shader_, "u_CameraPos", camera_position_);
    rhi_->set_uniform_float(shader_, "u_Exposure", exposure_);

    // IBL environment
    bool has_ibl = (ibl_.irradiance_map != rhi::INVALID_HANDLE &&
                    ibl_.prefiltered_map != rhi::INVALID_HANDLE &&
                    ibl_.brdf_lut != rhi::INVALID_HANDLE);
    rhi_->set_uniform_int(shader_, "u_HasIBL", has_ibl ? 1 : 0);
    if (has_ibl) {
        rhi_->bind_texture(ibl_.irradiance_map, 4);
        rhi_->set_uniform_int(shader_, "u_IrradianceMap", 4);
        rhi_->bind_texture(ibl_.prefiltered_map, 5);
        rhi_->set_uniform_int(shader_, "u_PrefilteredMap", 5);
        rhi_->bind_texture(ibl_.brdf_lut, 6);
        rhi_->set_uniform_int(shader_, "u_BrdfLUT", 6);
        rhi_->set_uniform_float(shader_, "u_IBLIntensity", ibl_.intensity);
    }

    // Draw
    rhi_->bind_vertex_buffer(vbo);
    rhi_->bind_index_buffer(ibo);
    rhi_->draw_indexed(index_count);
}

} // namespace nexus
