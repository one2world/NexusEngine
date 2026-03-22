#include "nexus/renderer/deferred_renderer.h"
#include "nexus/renderer/camera.h"
#include "nexus/core/log.h"
#include <algorithm>

namespace nexus {

// ── Shaders ─────────────────────────────────────────────────────────────────

namespace deferred_shaders {

const char* GEOMETRY_VERTEX = R"(
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

const char* GEOMETRY_FRAGMENT = R"(
#version 330 core
layout (location = 0) out vec4 gAlbedoMetallic;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gRoughnessAO;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec4  u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform int   u_HasAlbedoMap;
uniform sampler2D u_AlbedoMap;

void main() {
    vec4 albedo = u_Albedo;
    if (u_HasAlbedoMap > 0) {
        albedo *= texture(u_AlbedoMap, v_TexCoord);
    }

    gAlbedoMetallic = vec4(albedo.rgb, u_Metallic);
    gNormal = vec4(normalize(v_Normal) * 0.5 + 0.5, 1.0);
    gRoughnessAO = vec4(u_Roughness, u_AO, 0.0, 1.0);
}
)";

const char* LIGHTING_VERTEX = R"(
#version 330 core
layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

const char* LIGHTING_FRAGMENT = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_GAlbedoMetallic;
uniform sampler2D u_GNormal;
uniform sampler2D u_GRoughnessAO;
uniform sampler2D u_GDepth;

uniform vec3  u_CameraPos;
uniform vec3  u_SunDirection;
uniform vec3  u_SunColor;
uniform float u_SunIntensity;
uniform mat4  u_InvViewProjection;

uniform int   u_NumPointLights;
uniform vec3  u_PointLightPos[32];
uniform vec3  u_PointLightColor[32];
uniform float u_PointLightIntensity[32];
uniform float u_PointLightRadius[32];

const float PI = 3.14159265359;

vec3 worldPosFromDepth(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = u_InvViewProjection * clipPos;
    return worldPos.xyz / worldPos.w;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 0.0001);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float g1 = NdotV / (NdotV * (1.0 - k) + k);
    float g2 = NdotL / (NdotL * (1.0 - k) + k);
    return g1 * g2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 computeLight(vec3 N, vec3 V, vec3 L, vec3 lightColor, float lightIntensity,
                   vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 spec = (NDF * G * F) / (4.0 * NdotV * NdotL + 0.0001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    return (kD * albedo / PI + spec) * lightColor * lightIntensity * NdotL;
}

void main() {
    vec4 albedoMetallic = texture(u_GAlbedoMetallic, v_TexCoord);
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;

    vec3 normal = texture(u_GNormal, v_TexCoord).rgb * 2.0 - 1.0;
    normal = normalize(normal);

    vec4 roughnessAO = texture(u_GRoughnessAO, v_TexCoord);
    float roughness = roughnessAO.r;
    float ao = roughnessAO.g;

    float depth = texture(u_GDepth, v_TexCoord).r;
    vec3 worldPos = worldPosFromDepth(v_TexCoord, depth);

    vec3 V = normalize(u_CameraPos - worldPos);
    vec3 N = normal;

    // Directional light
    vec3 Lo = computeLight(N, V, normalize(-u_SunDirection),
                            u_SunColor, u_SunIntensity,
                            albedo, metallic, roughness);

    // Point lights
    for (int i = 0; i < u_NumPointLights; ++i) {
        vec3 lightVec = u_PointLightPos[i] - worldPos;
        float dist = length(lightVec);
        if (dist > u_PointLightRadius[i]) continue;

        vec3 L = lightVec / dist;
        float attenuation = 1.0 / (dist * dist + 1.0);
        float falloff = clamp(1.0 - dist / u_PointLightRadius[i], 0.0, 1.0);
        falloff *= falloff;

        Lo += computeLight(N, V, L,
                            u_PointLightColor[i],
                            u_PointLightIntensity[i] * attenuation * falloff,
                            albedo, metallic, roughness);
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    FragColor = vec4(ambient + Lo, 1.0);
}
)";

} // namespace deferred_shaders

// ── Fullscreen quad ─────────────────────────────────────────────────────────

static const float QUAD_VERTICES[] = {
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
};

// ── DeferredRenderer ────────────────────────────────────────────────────────

DeferredRenderer::DeferredRenderer(DeferredRenderer&& other) noexcept
    : rhi_(other.rhi_), gbuffer_(other.gbuffer_),
      geom_shader_(other.geom_shader_), geom_pipeline_(other.geom_pipeline_),
      light_shader_(other.light_shader_), light_pipeline_(other.light_pipeline_),
      quad_vbo_(other.quad_vbo_),
      view_projection_(other.view_projection_), camera_position_(other.camera_position_) {
    other.rhi_ = nullptr;
    other.geom_shader_ = rhi::INVALID_HANDLE; other.geom_pipeline_ = rhi::INVALID_HANDLE;
    other.light_shader_ = rhi::INVALID_HANDLE; other.light_pipeline_ = rhi::INVALID_HANDLE;
    other.quad_vbo_ = rhi::INVALID_HANDLE;
    other.gbuffer_ = {};
}

DeferredRenderer& DeferredRenderer::operator=(DeferredRenderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        rhi_ = other.rhi_; gbuffer_ = other.gbuffer_;
        geom_shader_ = other.geom_shader_; geom_pipeline_ = other.geom_pipeline_;
        light_shader_ = other.light_shader_; light_pipeline_ = other.light_pipeline_;
        quad_vbo_ = other.quad_vbo_;
        view_projection_ = other.view_projection_; camera_position_ = other.camera_position_;
        other.rhi_ = nullptr;
        other.geom_shader_ = rhi::INVALID_HANDLE; other.geom_pipeline_ = rhi::INVALID_HANDLE;
        other.light_shader_ = rhi::INVALID_HANDLE; other.light_pipeline_ = rhi::INVALID_HANDLE;
        other.quad_vbo_ = rhi::INVALID_HANDLE; other.gbuffer_ = {};
    }
    return *this;
}

void DeferredRenderer::create_gbuffer(u32 width, u32 height) {
    gbuffer_.width = width;
    gbuffer_.height = height;

    auto make_tex = [&](rhi::TextureFormat fmt) {
        rhi::TextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = fmt;
        desc.generate_mipmaps = false;
        return rhi_->create_texture(desc);
    };

    gbuffer_.albedo_metallic = make_tex(rhi::TextureFormat::RGBA16F);
    gbuffer_.normal          = make_tex(rhi::TextureFormat::RGBA16F);
    gbuffer_.roughness_ao    = make_tex(rhi::TextureFormat::RGBA8);
    gbuffer_.depth           = make_tex(rhi::TextureFormat::Depth32F);

    rhi::FramebufferDesc fb_desc;
    fb_desc.width = width;
    fb_desc.height = height;
    fb_desc.color_attachments = {
        rhi::TextureFormat::RGBA16F,  // RT0: albedo+metallic
        rhi::TextureFormat::RGBA16F,  // RT1: normal
        rhi::TextureFormat::RGBA8     // RT2: roughness+AO
    };
    fb_desc.has_depth = true;
    gbuffer_.framebuffer = rhi_->create_framebuffer(fb_desc);
}

void DeferredRenderer::destroy_gbuffer() {
    rhi_->destroy_framebuffer(gbuffer_.framebuffer);
    rhi_->destroy_texture(gbuffer_.albedo_metallic);
    rhi_->destroy_texture(gbuffer_.normal);
    rhi_->destroy_texture(gbuffer_.roughness_ao);
    rhi_->destroy_texture(gbuffer_.depth);
}

void DeferredRenderer::init(rhi::RHI* rhi, u32 width, u32 height) {
    rhi_ = rhi;

    create_gbuffer(width, height);

    // Geometry pass shader + pipeline
    geom_shader_ = rhi->create_shader(deferred_shaders::GEOMETRY_VERTEX,
                                       deferred_shaders::GEOMETRY_FRAGMENT);
    {
        rhi::PipelineDesc desc;
        desc.shader = geom_shader_;
        desc.blend = rhi::BlendMode::None;
        desc.depth_test = true;
        desc.depth_write = true;
        desc.cull = rhi::CullMode::Back;
        desc.primitive = rhi::PrimitiveType::Triangles;
        desc.vertex_layout.stride = sizeof(float) * 8;
        desc.vertex_layout.attributes = {
            {0, 3, 0, false},
            {1, 3, sizeof(float) * 3, false},
            {2, 2, sizeof(float) * 6, false},
        };
        geom_pipeline_ = rhi->create_pipeline(desc);
    }

    // Lighting pass shader + pipeline
    light_shader_ = rhi->create_shader(deferred_shaders::LIGHTING_VERTEX,
                                        deferred_shaders::LIGHTING_FRAGMENT);
    {
        rhi::PipelineDesc desc;
        desc.shader = light_shader_;
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
        light_pipeline_ = rhi->create_pipeline(desc);
    }

    // Quad VBO
    rhi::BufferDesc buf_desc;
    buf_desc.type = rhi::BufferType::Vertex;
    buf_desc.usage = rhi::BufferUsage::Static;
    buf_desc.data = QUAD_VERTICES;
    buf_desc.size = sizeof(QUAD_VERTICES);
    quad_vbo_ = rhi->create_buffer(buf_desc);

    NX_INFO("DeferredRenderer initialized ({}x{})", width, height);
}

void DeferredRenderer::shutdown() {
    if (!rhi_) return;
    destroy_gbuffer();
    if (geom_pipeline_ != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(geom_pipeline_);
    if (geom_shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(geom_shader_);
    if (light_pipeline_ != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(light_pipeline_);
    if (light_shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(light_shader_);
    if (quad_vbo_ != rhi::INVALID_HANDLE) rhi_->destroy_buffer(quad_vbo_);
    geom_pipeline_ = rhi::INVALID_HANDLE; geom_shader_ = rhi::INVALID_HANDLE;
    light_pipeline_ = rhi::INVALID_HANDLE; light_shader_ = rhi::INVALID_HANDLE;
    quad_vbo_ = rhi::INVALID_HANDLE;
    rhi_ = nullptr;
}

void DeferredRenderer::resize(u32 width, u32 height) {
    destroy_gbuffer();
    create_gbuffer(width, height);
}

void DeferredRenderer::begin_geometry_pass(const Camera3D& camera) {
    view_projection_ = camera.get_view_projection();
    camera_position_ = camera.position;

    rhi_->bind_framebuffer(gbuffer_.framebuffer);
    rhi_->set_viewport(0, 0, static_cast<i32>(gbuffer_.width),
                       static_cast<i32>(gbuffer_.height));
    rhi_->clear(Vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

void DeferredRenderer::submit_geometry(rhi::BufferHandle vbo,
                                         rhi::BufferHandle ibo,
                                         u32 index_count,
                                         const Mat4& transform,
                                         const PBRMaterial& material) {
    rhi_->bind_shader(geom_shader_);
    rhi_->bind_pipeline(geom_pipeline_);

    rhi_->set_uniform_mat4(geom_shader_, "u_ViewProjection", view_projection_);
    rhi_->set_uniform_mat4(geom_shader_, "u_Model", transform);
    Mat4 normal_matrix = glm::transpose(glm::inverse(transform));
    rhi_->set_uniform_mat4(geom_shader_, "u_NormalMatrix", normal_matrix);

    rhi_->set_uniform_vec4(geom_shader_, "u_Albedo", material.albedo);
    rhi_->set_uniform_float(geom_shader_, "u_Metallic", material.metallic);
    rhi_->set_uniform_float(geom_shader_, "u_Roughness", material.roughness);
    rhi_->set_uniform_float(geom_shader_, "u_AO", material.ao_strength);

    int has_albedo = (material.albedo_map != rhi::INVALID_HANDLE) ? 1 : 0;
    rhi_->set_uniform_int(geom_shader_, "u_HasAlbedoMap", has_albedo);
    if (has_albedo) {
        rhi_->bind_texture(material.albedo_map, 0);
        rhi_->set_uniform_int(geom_shader_, "u_AlbedoMap", 0);
    }

    rhi_->bind_vertex_buffer(vbo);
    rhi_->bind_index_buffer(ibo);
    rhi_->draw_indexed(index_count);
}

void DeferredRenderer::end_geometry_pass() {
    rhi_->unbind_framebuffer();
}

void DeferredRenderer::lighting_pass(const Camera3D& camera,
                                       Vec3 sun_direction, Vec3 sun_color,
                                       float sun_intensity,
                                       const std::vector<PointLight>& point_lights,
                                       rhi::FramebufferHandle dest) {
    rhi_->bind_framebuffer(dest);
    rhi_->bind_shader(light_shader_);
    rhi_->bind_pipeline(light_pipeline_);

    // Bind G-buffer textures
    rhi_->bind_texture(gbuffer_.albedo_metallic, 0);
    rhi_->set_uniform_int(light_shader_, "u_GAlbedoMetallic", 0);
    rhi_->bind_texture(gbuffer_.normal, 1);
    rhi_->set_uniform_int(light_shader_, "u_GNormal", 1);
    rhi_->bind_texture(gbuffer_.roughness_ao, 2);
    rhi_->set_uniform_int(light_shader_, "u_GRoughnessAO", 2);
    rhi_->bind_texture(gbuffer_.depth, 3);
    rhi_->set_uniform_int(light_shader_, "u_GDepth", 3);

    // Camera
    rhi_->set_uniform_vec3(light_shader_, "u_CameraPos", camera.position);
    Mat4 inv_vp = glm::inverse(camera.get_view_projection());
    rhi_->set_uniform_mat4(light_shader_, "u_InvViewProjection", inv_vp);

    // Sun
    rhi_->set_uniform_vec3(light_shader_, "u_SunDirection", sun_direction);
    rhi_->set_uniform_vec3(light_shader_, "u_SunColor", sun_color);
    rhi_->set_uniform_float(light_shader_, "u_SunIntensity", sun_intensity);

    // Point lights
    u32 num_lights = static_cast<u32>(std::min(
        static_cast<size_t>(MAX_POINT_LIGHTS), point_lights.size()));
    rhi_->set_uniform_int(light_shader_, "u_NumPointLights",
                           static_cast<i32>(num_lights));

    for (u32 i = 0; i < num_lights; ++i) {
        std::string idx = std::to_string(i);
        rhi_->set_uniform_vec3(light_shader_, "u_PointLightPos[" + idx + "]",
                                point_lights[i].position);
        rhi_->set_uniform_vec3(light_shader_, "u_PointLightColor[" + idx + "]",
                                point_lights[i].color);
        rhi_->set_uniform_float(light_shader_, "u_PointLightIntensity[" + idx + "]",
                                 point_lights[i].intensity);
        rhi_->set_uniform_float(light_shader_, "u_PointLightRadius[" + idx + "]",
                                 point_lights[i].radius);
    }

    rhi_->bind_vertex_buffer(quad_vbo_);
    rhi_->draw(6);
}

} // namespace nexus
