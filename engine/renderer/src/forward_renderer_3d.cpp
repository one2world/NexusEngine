// ============================================================================
// forward_renderer_3d.cpp - Multi-light forward rendering pipeline
// ============================================================================

#include <nexus/renderer/forward_renderer_3d.h>
#include <nexus/renderer/shadow_map.h>
#include <nexus/core/log.h>
#include <cmath>
#include <string>

namespace nexus {

// ── Default 3D shaders ──────────────────────────────────────────────────────

static const char* FORWARD_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3  v_FragPos;
out vec3  v_Normal;
out vec2  v_TexCoord;
// View-space linear depth (positive = away from camera).  Used by the
// fragment shader to pick the correct CSM cascade — selection is
// driven by camera depth, not light-space depth, so the cascades
// switch independent of the directional light direction.
out float v_ViewDepth;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_FragPos    = worldPos.xyz;
    v_Normal     = mat3(u_NormalMatrix) * a_Normal;
    v_TexCoord   = a_TexCoord;
    v_ViewDepth  = -(u_View * worldPos).z;   // negate: GL view space is -z forward
    gl_Position  = u_ViewProjection * worldPos;
}
)";

static const char* FORWARD_FRAGMENT_PROLOG = R"(
#version 330 core
in vec3  v_FragPos;
in vec3  v_Normal;
in vec2  v_TexCoord;
in float v_ViewDepth;

out vec4 FragColor;

uniform vec4 u_Color;
uniform sampler2D u_Texture;
uniform vec3 u_CameraPos;

// Directional light
uniform vec3  u_DirLight_Direction;
uniform vec3  u_DirLight_Color;
uniform float u_DirLight_Intensity;
uniform bool  u_DirLight_CastShadows;        // master toggle for CSM sampling

// ── Cascaded shadow map uniforms ──────────────────────────────────
//
// 4 cascades max — matches CascadedShadowMap::MAX_CASCADES.  Each
// cascade has its own depth texture (separate sampler2D rather than
// a samplerArray, since the RHI doesn't expose 2D array textures yet)
// and its own light-space view-projection matrix.  `u_NumCascades`
// is the number actually populated this frame.
//
// `u_SplitDepth[i]` is the *view-space* depth where cascade i ends —
// the fragment shader picks cascade i if v_ViewDepth < SplitDepth[i].
//
// Bias values come from CascadedShadowMap::Config so a host can tune
// them per-scene without recompiling the shader.
const int MAX_CASCADES = 4;
uniform sampler2D u_ShadowMap0;
uniform sampler2D u_ShadowMap1;
uniform sampler2D u_ShadowMap2;
uniform sampler2D u_ShadowMap3;
uniform mat4      u_LightVP[MAX_CASCADES];
uniform float     u_SplitDepth[MAX_CASCADES];
uniform int       u_NumCascades;
uniform float     u_ShadowBias;
uniform float     u_NormalBias;
uniform float     u_ShadowMapTexel;          // 1.0 / shadow resolution
uniform bool      u_ReceiveShadows;          // per-mesh toggle
)";

// PCF / Poisson-disk sampling helpers — pasted between PROLOG and BODY
// at init time.  Defined once in shadow_map.cpp so deferred + future
// shaders share the implementation without copy-paste.
// (See shadow_shaders::SHADOW_SAMPLING_GLSL for the source.)

static const char* FORWARD_FRAGMENT_BODY = R"(

// Point lights
#define MAX_POINT_LIGHTS 8
uniform int u_NumPointLights;
uniform vec3 u_PointLight_Position[MAX_POINT_LIGHTS];
uniform vec3 u_PointLight_Color[MAX_POINT_LIGHTS];
uniform float u_PointLight_Intensity[MAX_POINT_LIGHTS];
uniform float u_PointLight_Radius[MAX_POINT_LIGHTS];

// Spot lights
#define MAX_SPOT_LIGHTS 4
uniform int u_NumSpotLights;
uniform vec3 u_SpotLight_Position[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLight_Direction[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLight_Color[MAX_SPOT_LIGHTS];
uniform float u_SpotLight_Intensity[MAX_SPOT_LIGHTS];
uniform float u_SpotLight_Range[MAX_SPOT_LIGHTS];
uniform float u_SpotLight_InnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLight_OuterCos[MAX_SPOT_LIGHTS];

// Hemispheric ambient — scene-level fill that every surface receives in
// proportion to its `ambient_response` material parameter.  Sky/ground
// colours come from set_ambient_sky / set_ambient_ground (defaults
// chosen for an overcast preset).
uniform vec3 u_AmbientSky;     // RGB tint applied where normal.y > 0
uniform vec3 u_AmbientGround;  // RGB tint applied where normal.y < 0

// ── Material uniforms ──────────────────────────────────────────────
//
// Every "knob" that used to be hardcoded in the shader (specular
// power, specular weight, diffuse wrap, ambient response) now comes
// from the SurfaceMaterial bound by ForwardRenderer3D::draw_mesh.
// Different materials get different looks without re-compiling shaders.
uniform vec4  u_Material_Albedo;
uniform vec3  u_Material_Specular;
uniform float u_Material_SpecStrength;
uniform float u_Material_Shininess;
uniform float u_Material_DiffuseWrap;
uniform float u_Material_AmbientResponse;
uniform vec3  u_Material_Emissive;
uniform float u_Material_EmissiveStrength;

vec3 hemispheric_ambient(vec3 normal) {
    float t = normal.y * 0.5 + 0.5;
    return mix(u_AmbientGround, u_AmbientSky, t);
}

// Wrap-diffuse — softens Lambert's discontinuous max(0, n·l) cliff
// into a smooth shoulder when the surface's material asks for it.
//   wrap = 0    → vanilla Lambert (physically correct, hard terminator)
//   wrap = 1    → Half-Lambert (no terminator, very flat)
// Each material picks its own wrap value via SurfaceMaterial.diffuse_wrap.
float wrap_diffuse(vec3 normal, vec3 lightDir, float wrap) {
    float ndl = dot(normal, lightDir);
    return max((ndl + wrap) / (1.0 + wrap), 0.0);
}

// Per-light Blinn-Phong with material-driven shininess + specular weight.
// `lightColor`, `lightIntensity`, and any per-light attenuation are baked
// into the caller so this stays purely local: surface response × light
// energy = contribution.
vec3 lit_brdf(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor) {
    float diff = wrap_diffuse(normal, lightDir, u_Material_DiffuseWrap);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = (dot(normal, lightDir) > 0.0)
        ? pow(max(dot(normal, halfwayDir), 0.0), u_Material_Shininess)
        : 0.0;
    vec3 diffuse  = diff * lightColor;
    vec3 specular = spec * u_Material_SpecStrength * u_Material_Specular * lightColor;
    return diffuse + specular;
}

// ── CSM cascade selection + shadow factor ────────────────────────────
//
// Cascade is chosen from the fragment's view-space depth.  Depth > the
// last cascade's far split returns the last cascade so distant
// fragments still get *some* shadow rather than a hard cutoff.
int select_cascade(float view_depth) {
    for (int i = 0; i < u_NumCascades - 1; ++i) {
        if (view_depth < u_SplitDepth[i]) return i;
    }
    return u_NumCascades - 1;
}

float sample_cascade_pcf(int cascade, vec3 projCoords, float bias) {
    // GLSL 330 forbids dynamic indexing of a sampler array, so dispatch
    // by cascade.  4 cascades = 4 branches; the GPU coalesces predictably.
    if (cascade == 0) return sampleShadowPCF(u_ShadowMap0, projCoords, bias, u_ShadowMapTexel);
    if (cascade == 1) return sampleShadowPCF(u_ShadowMap1, projCoords, bias, u_ShadowMapTexel);
    if (cascade == 2) return sampleShadowPCF(u_ShadowMap2, projCoords, bias, u_ShadowMapTexel);
    return                    sampleShadowPCF(u_ShadowMap3, projCoords, bias, u_ShadowMapTexel);
}

// Compute shadow attenuation for the directional light.  Returns 1.0
// (fully lit) when:
//   - the fragment opts out via u_ReceiveShadows = false
//   - the directional light has cast_shadows = false
//   - the fragment's view depth is beyond the last cascade's coverage
//   - the fragment's projected light-space position is outside the map
// Otherwise returns the PCF factor in [0, 1] where 0 = fully shadowed.
float directional_shadow_factor(vec3 normal, vec3 lightDir) {
    if (!u_ReceiveShadows || !u_DirLight_CastShadows) return 1.0;
    if (u_NumCascades <= 0) return 1.0;

    int cascade = select_cascade(v_ViewDepth);

    // Normal-offset bias: push the receiver fragment slightly along its
    // normal before sampling, so geometry surfaces don't self-shadow at
    // grazing angles.  Scaled by the cascade's texel size in world
    // units (approximated by light-space frustum size / resolution).
    vec3 biased_world = v_FragPos + normal * u_NormalBias;

    vec4 light_clip = u_LightVP[cascade] * vec4(biased_world, 1.0);
    vec3 proj = light_clip.xyz / light_clip.w;
    proj = proj * 0.5 + 0.5;  // [-1, 1] → [0, 1]

    // Outside the cascade's coverage → fall through to no-shadow.  The
    // last cascade should always cover; intermediate ones may not when
    // the camera frustum nips the edge of the cascade's bounds.
    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z < 0.0 || proj.z > 1.0) {
        return 1.0;
    }

    // Slope-scale bias: more bias on grazing surfaces where Lambertian
    // depth varies fastest.  Clamped to a floor so flat surfaces still
    // receive a tiny constant bias for floating-point safety.
    float ndotl = max(dot(normal, lightDir), 0.0);
    float bias  = max(u_ShadowBias * (1.0 - ndotl), u_ShadowBias * 0.1);

    return sample_cascade_pcf(cascade, proj, bias);
}

vec3 calcDirectionalLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-u_DirLight_Direction);
    vec3 lit      = lit_brdf(normal, lightDir, viewDir, u_DirLight_Color);
    float shadow  = directional_shadow_factor(normal, lightDir);
    return lit * shadow * u_DirLight_Intensity;
}

vec3 calcPointLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightVec  = u_PointLight_Position[i] - fragPos;
    float distance = length(lightVec);
    vec3 lightDir  = normalize(lightVec);

    // Inverse-square style attenuation (clamped by radius).
    float r = u_PointLight_Radius[i];
    float attenuation = 1.0 / (1.0 + (distance / r) * (distance / r));

    return lit_brdf(normal, lightDir, viewDir, u_PointLight_Color[i])
         * attenuation * u_PointLight_Intensity[i];
}

vec3 calcSpotLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightVec  = u_SpotLight_Position[i] - fragPos;
    float distance = length(lightVec);
    vec3 lightDir  = normalize(lightVec);

    float r = u_SpotLight_Range[i];
    float attenuation = 1.0 / (1.0 + (distance / r) * (distance / r));

    float theta = dot(lightDir, normalize(-u_SpotLight_Direction[i]));
    float epsilon = u_SpotLight_InnerCos[i] - u_SpotLight_OuterCos[i];
    float spotIntensity = clamp((theta - u_SpotLight_OuterCos[i]) /
                                  max(epsilon, 0.001), 0.0, 1.0);

    return lit_brdf(normal, lightDir, viewDir, u_SpotLight_Color[i])
         * attenuation * spotIntensity * u_SpotLight_Intensity[i];
}

void main() {
    vec3 normal  = normalize(v_Normal);
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);

    // 1. Ambient fill — scaled by how much the material accepts.
    vec3 light_sum = hemispheric_ambient(normal) * u_Material_AmbientResponse;

    // 2. Per-light contributions — each light's BRDF response.
    light_sum += calcDirectionalLight(normal, viewDir);
    for (int i = 0; i < u_NumPointLights; ++i) {
        light_sum += calcPointLight(i, normal, v_FragPos, viewDir);
    }
    for (int i = 0; i < u_NumSpotLights; ++i) {
        light_sum += calcSpotLight(i, normal, v_FragPos, viewDir);
    }

    // 3. Albedo modulation — material albedo × per-instance tint × texture.
    //    Tint is the legacy MeshRendererComponent.tint (still useful as
    //    an instance-level multiplier on top of the asset's albedo).
    vec4 tex      = texture(u_Texture, v_TexCoord);
    vec4 baseRgba = u_Material_Albedo * u_Color * tex;
    vec3 lit      = light_sum * baseRgba.rgb;

    // 4. Emissive bypasses lighting entirely.
    vec3 emissive = u_Material_Emissive * u_Material_EmissiveStrength;

    FragColor = vec4(lit + emissive, baseRgba.a);
}
)";

// ── Lifecycle ───────────────────────────────────────────────────────────────

void ForwardRenderer3D::init(rhi::RHI* rhi) {
    rhi_ = rhi;

    // Assemble the fragment shader: prolog (declarations + uniforms),
    // shadow sampling helpers (shared with deferred / point-light
    // shadow paths via shadow_shaders::SHADOW_SAMPLING_GLSL), then the
    // body (light models + main()).  Concatenation lets us keep the
    // shadow helpers in a single source of truth without copy-paste.
    const std::string fragment_source =
        std::string(FORWARD_FRAGMENT_PROLOG) +
        shadow_shaders::SHADOW_SAMPLING_GLSL +
        FORWARD_FRAGMENT_BODY;

    shader_ = rhi_->create_shader(FORWARD_VERTEX_SHADER, fragment_source.c_str());
    if (shader_ == rhi::INVALID_HANDLE) {
        NX_ERROR("ForwardRenderer3D: Failed to compile shaders");
        return;
    }

    // Create 1x1 white texture
    u32 white_pixel = 0xFFFFFFFF;
    rhi::TextureDesc white_desc;
    white_desc.width  = 1;
    white_desc.height = 1;
    white_desc.format = rhi::TextureFormat::RGBA8;
    white_desc.min_filter = rhi::TextureFilter::Nearest;
    white_desc.mag_filter = rhi::TextureFilter::Nearest;
    white_desc.generate_mipmaps = false;
    white_desc.data = &white_pixel;
    white_texture_ = rhi_->create_texture(white_desc);

    // 1×1 depth=1.0 texture used to fill cascade sampler slots that
    // aren't backed by a real shadow map this frame.  Sampling it
    // returns 1.0 (max depth), which the PCF code interprets as
    // "fragment is closer than any blocker" → fully lit.  This keeps
    // the shader path uniform whether shadows are on or off.
    f32 white_depth_pixel = 1.0f;
    rhi::TextureDesc dd;
    dd.width = 1;
    dd.height = 1;
    dd.format = rhi::TextureFormat::Depth32F;
    dd.min_filter = rhi::TextureFilter::Nearest;
    dd.mag_filter = rhi::TextureFilter::Nearest;
    dd.generate_mipmaps = false;
    dd.data = &white_depth_pixel;
    white_depth_texture_ = rhi_->create_texture(dd);

    NX_INFO("ForwardRenderer3D initialized");
}

ForwardRenderer3D::ForwardRenderer3D(ForwardRenderer3D&& other) noexcept
    : rhi_(other.rhi_), shader_(other.shader_), white_texture_(other.white_texture_),
      dir_light_(other.dir_light_), point_lights_(std::move(other.point_lights_)),
      view_projection_(other.view_projection_), camera_position_(other.camera_position_) {
    other.rhi_ = nullptr;
    other.shader_ = rhi::INVALID_HANDLE;
    other.white_texture_ = rhi::INVALID_HANDLE;
}

ForwardRenderer3D& ForwardRenderer3D::operator=(ForwardRenderer3D&& other) noexcept {
    if (this != &other) {
        shutdown();
        rhi_ = other.rhi_; shader_ = other.shader_; white_texture_ = other.white_texture_;
        dir_light_ = other.dir_light_; point_lights_ = std::move(other.point_lights_);
        view_projection_ = other.view_projection_; camera_position_ = other.camera_position_;
        other.rhi_ = nullptr; other.shader_ = rhi::INVALID_HANDLE; other.white_texture_ = rhi::INVALID_HANDLE;
    }
    return *this;
}

void ForwardRenderer3D::shutdown() {
    if (!rhi_) return;
    if (shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(shader_);
    if (white_texture_ != rhi::INVALID_HANDLE) rhi_->destroy_texture(white_texture_);
    if (white_depth_texture_ != rhi::INVALID_HANDLE) {
        rhi_->destroy_texture(white_depth_texture_);
    }
    shader_              = rhi::INVALID_HANDLE;
    white_texture_       = rhi::INVALID_HANDLE;
    white_depth_texture_ = rhi::INVALID_HANDLE;
    rhi_ = nullptr;
    in_frame_ = false;
}

// Bind shadow textures to fixed sampler slots and push cascade matrices /
// split depths / bias / texel size to the main shader.  Slots 1..4 are
// reserved for cascades; slot 0 is the diffuse texture (set per-draw).
// When shadow_map_ is null or has fewer cascades than MAX_CASCADES we
// fill the unused slots with white_depth_texture_ so the GPU never
// reads from an unbound sampler — that's UB and hides bugs behind
// silent driver behaviour.
void ForwardRenderer3D::push_shadow_uniforms() {
    if (!rhi_ || shader_ == rhi::INVALID_HANDLE) return;

    constexpr u32 kMaxCascades = CascadedShadowMap::MAX_CASCADES;
    const char* sampler_names[kMaxCascades] = {
        "u_ShadowMap0", "u_ShadowMap1", "u_ShadowMap2", "u_ShadowMap3"
    };

    if (shadow_map_) {
        const auto& cfg     = shadow_map_->config();
        const u32   ncasc   = shadow_map_->num_cascades();
        const auto& cascades = shadow_map_->cascades();

        // Per-cascade samplers + matrices + split depths.
        for (u32 i = 0; i < kMaxCascades; ++i) {
            rhi::TextureHandle tex = (i < ncasc)
                ? shadow_map_->depth_texture(i)
                : white_depth_texture_;
            const i32 unit = static_cast<i32>(i + 1);  // slot 0 = diffuse
            rhi_->bind_texture(tex, static_cast<u32>(unit));
            rhi_->set_uniform_int(shader_, sampler_names[i], unit);

            const std::string vp_name    =
                "u_LightVP["    + std::to_string(i) + "]";
            const std::string split_name =
                "u_SplitDepth[" + std::to_string(i) + "]";
            if (i < ncasc) {
                rhi_->set_uniform_mat4 (shader_, vp_name,    cascades[i].light_view_projection);
                rhi_->set_uniform_float(shader_, split_name, cascades[i].split_depth);
            } else {
                rhi_->set_uniform_mat4 (shader_, vp_name,    Mat4(1.0f));
                rhi_->set_uniform_float(shader_, split_name, std::numeric_limits<float>::max());
            }
        }
        rhi_->set_uniform_int  (shader_, "u_NumCascades",    static_cast<i32>(ncasc));
        rhi_->set_uniform_float(shader_, "u_ShadowBias",     cfg.bias);
        rhi_->set_uniform_float(shader_, "u_NormalBias",     cfg.normal_bias);
        rhi_->set_uniform_float(shader_, "u_ShadowMapTexel", 1.0f / static_cast<float>(cfg.resolution));
    } else {
        // No shadow map: bind the white-depth fallback to every cascade
        // slot and clamp NumCascades=0.  The shader's
        // directional_shadow_factor returns 1.0 for NumCascades<=0.
        for (u32 i = 0; i < kMaxCascades; ++i) {
            const i32 unit = static_cast<i32>(i + 1);
            rhi_->bind_texture(white_depth_texture_, static_cast<u32>(unit));
            rhi_->set_uniform_int(shader_, sampler_names[i], unit);
        }
        rhi_->set_uniform_int  (shader_, "u_NumCascades",    0);
        rhi_->set_uniform_float(shader_, "u_ShadowBias",     0.0f);
        rhi_->set_uniform_float(shader_, "u_NormalBias",     0.0f);
        rhi_->set_uniform_float(shader_, "u_ShadowMapTexel", 1.0f);
    }
}

// ── Frustum culling ─────────────────────────────────────────────────────────

void ForwardRenderer3D::extract_frustum_planes() {
    // Gribb/Hartmann method: extract planes from view-projection matrix
    const Mat4& m = view_projection_;
    // Left
    frustum_planes_[0] = Vec4(m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0]);
    // Right
    frustum_planes_[1] = Vec4(m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0]);
    // Bottom
    frustum_planes_[2] = Vec4(m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1]);
    // Top
    frustum_planes_[3] = Vec4(m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1]);
    // Near
    frustum_planes_[4] = Vec4(m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2]);
    // Far
    frustum_planes_[5] = Vec4(m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2]);

    // Normalize planes
    for (auto& plane : frustum_planes_) {
        float len = glm::length(Vec3(plane));
        if (len > 0.0f) plane /= len;
    }
}

bool ForwardRenderer3D::is_visible(Vec3 center, float radius) const {
    for (const auto& plane : frustum_planes_) {
        float dist = glm::dot(Vec3(plane), center) + plane.w;
        if (dist < -radius) return false;
    }
    return true;
}

// ── Frame scope ─────────────────────────────────────────────────────────────

void ForwardRenderer3D::begin_frame(const Camera3D& camera) {
    if (!rhi_ || shader_ == rhi::INVALID_HANDLE) return;

    current_camera_ = camera;
    view_projection_ = camera.get_view_projection();
    view_matrix_     = camera.get_view_matrix();
    camera_position_ = camera.position;
    point_lights_.clear();
    spot_lights_.clear();
    in_frame_ = true;

    extract_frustum_planes();

    rhi_->set_depth_test(true);
    rhi_->bind_shader(shader_);
    rhi_->set_uniform_mat4(shader_, "u_ViewProjection", view_projection_);
    rhi_->set_uniform_mat4(shader_, "u_View",           view_matrix_);
    rhi_->set_uniform_vec3(shader_, "u_CameraPos",      camera_position_);

    // Hemispheric ambient defaults — picked to look like an overcast
    // sky without overwhelming the directional light.  Hosts that want
    // a different mood (sunrise warmth, night-time blue) override
    // these via set_ambient_sky / set_ambient_ground.
    rhi_->set_uniform_vec3(shader_, "u_AmbientSky",    ambient_sky_);
    rhi_->set_uniform_vec3(shader_, "u_AmbientGround", ambient_ground_);

    // Default shadow uniforms — the directional light's cast_shadows
    // flag is pushed by set_directional_light().  When no shadow_map_
    // is initialised these stay at "off" so set_directional_light
    // can short-circuit the sampling path entirely.
    rhi_->set_uniform_int(shader_, "u_NumCascades", 0);
    push_shadow_uniforms();  // binds whatever the renderer currently has
}

void ForwardRenderer3D::end_frame() {
    in_frame_ = false;
}

void ForwardRenderer3D::set_directional_light(const DirectionalLight& light) {
    dir_light_ = light;
    rhi_->set_uniform_vec3 (shader_, "u_DirLight_Direction", light.direction);
    rhi_->set_uniform_vec3 (shader_, "u_DirLight_Color",     light.color);
    rhi_->set_uniform_float(shader_, "u_DirLight_Intensity", light.intensity);
    // Master shadow toggle for the directional light.  Even with shadows
    // enabled at the renderer level, a host can flip this off per-light
    // to sample no shadow map and skip the shadow factor multiply.
    rhi_->set_uniform_int  (shader_, "u_DirLight_CastShadows",
                              light.cast_shadows ? 1 : 0);
}

void ForwardRenderer3D::add_point_light(const PointLight& light) {
    if (point_lights_.size() >= MAX_POINT_LIGHTS) return;
    u32 idx = static_cast<u32>(point_lights_.size());
    point_lights_.push_back(light);

    std::string prefix = "u_PointLight_Position[" + std::to_string(idx) + "]";
    rhi_->set_uniform_vec3(shader_, prefix, light.position);

    prefix = "u_PointLight_Color[" + std::to_string(idx) + "]";
    rhi_->set_uniform_vec3(shader_, prefix, light.color);

    prefix = "u_PointLight_Intensity[" + std::to_string(idx) + "]";
    rhi_->set_uniform_float(shader_, prefix, light.intensity);

    prefix = "u_PointLight_Radius[" + std::to_string(idx) + "]";
    rhi_->set_uniform_float(shader_, prefix, light.radius);

    rhi_->set_uniform_int(shader_, "u_NumPointLights",
                          static_cast<i32>(point_lights_.size()));
}

void ForwardRenderer3D::add_spot_light(const SpotLight& light) {
    if (spot_lights_.size() >= MAX_SPOT_LIGHTS) return;
    u32 idx = static_cast<u32>(spot_lights_.size());
    spot_lights_.push_back(light);

    std::string si = std::to_string(idx);
    rhi_->set_uniform_vec3(shader_, "u_SpotLight_Position[" + si + "]", light.position);
    rhi_->set_uniform_vec3(shader_, "u_SpotLight_Direction[" + si + "]", light.direction);
    rhi_->set_uniform_vec3(shader_, "u_SpotLight_Color[" + si + "]", light.color);
    rhi_->set_uniform_float(shader_, "u_SpotLight_Intensity[" + si + "]", light.intensity);
    rhi_->set_uniform_float(shader_, "u_SpotLight_Range[" + si + "]", light.range);
    rhi_->set_uniform_float(shader_, "u_SpotLight_InnerCos[" + si + "]", light.inner_cos);
    rhi_->set_uniform_float(shader_, "u_SpotLight_OuterCos[" + si + "]", light.outer_cos);
    rhi_->set_uniform_int(shader_, "u_NumSpotLights", static_cast<i32>(spot_lights_.size()));
}

// ── Mesh management ─────────────────────────────────────────────────────────

void ForwardRenderer3D::upload_mesh(Mesh& mesh) {
    // Local-space AABB — derived once from CPU vertices so frustum culling
    // can use a real bounding volume per mesh instead of a one-size-fits-all
    // unit-cube assumption.  Critical for large meshes (planes, terrain,
    // pre-scaled imports) whose authored extent dwarfs the pivot.
    if (!mesh.vertices.empty()) {
        Vec3 lo = mesh.vertices[0].position;
        Vec3 hi = lo;
        for (const auto& v : mesh.vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
        mesh.local_aabb_min = lo;
        mesh.local_aabb_max = hi;
    }

    // VBO
    rhi::BufferDesc vbo_desc;
    vbo_desc.type  = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.size  = mesh.vertices.size() * sizeof(MeshVertex);
    vbo_desc.data  = mesh.vertices.data();
    mesh.vbo = rhi_->create_buffer(vbo_desc);

    // IBO
    rhi::BufferDesc ibo_desc;
    ibo_desc.type  = rhi::BufferType::Index;
    ibo_desc.usage = rhi::BufferUsage::Static;
    ibo_desc.size  = mesh.indices.size() * sizeof(u32);
    ibo_desc.data  = mesh.indices.data();
    mesh.ibo = rhi_->create_buffer(ibo_desc);

    // Pipeline / VAO
    rhi::PipelineDesc pipe_desc;
    pipe_desc.shader     = shader_;
    pipe_desc.blend      = rhi::BlendMode::None;
    pipe_desc.depth_test = true;
    pipe_desc.cull       = rhi::CullMode::Back;
    pipe_desc.primitive  = rhi::PrimitiveType::Triangles;

    pipe_desc.vertex_layout.stride = sizeof(MeshVertex);
    pipe_desc.vertex_layout.attributes = {
        {0, 3, offsetof(MeshVertex, position), false},
        {1, 3, offsetof(MeshVertex, normal),   false},
        {2, 2, offsetof(MeshVertex, texcoord), false},
    };

    mesh.pipeline = rhi_->create_pipeline(pipe_desc);
}

void ForwardRenderer3D::destroy_mesh(Mesh& mesh) {
    if (!rhi_) return;
    if (mesh.pipeline != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(mesh.pipeline);
    if (mesh.ibo != rhi::INVALID_HANDLE) rhi_->destroy_buffer(mesh.ibo);
    if (mesh.vbo != rhi::INVALID_HANDLE) rhi_->destroy_buffer(mesh.vbo);
    mesh.pipeline = rhi::INVALID_HANDLE;
    mesh.ibo = rhi::INVALID_HANDLE;
    mesh.vbo = rhi::INVALID_HANDLE;
}

// Push every material parameter as a uniform.  Called once per draw with
// the resolved SurfaceMaterial — keeps push_material out of the hot inner
// loop while still letting each draw use its own material.
static void push_material_uniforms(rhi::RHI* rhi,
                                    rhi::ShaderHandle shader,
                                    const SurfaceMaterial& m) {
    rhi->set_uniform_vec4 (shader, "u_Material_Albedo",            m.albedo);
    rhi->set_uniform_vec3 (shader, "u_Material_Specular",          m.specular_color);
    rhi->set_uniform_float(shader, "u_Material_SpecStrength",      m.specular_strength);
    rhi->set_uniform_float(shader, "u_Material_Shininess",         m.shininess);
    rhi->set_uniform_float(shader, "u_Material_DiffuseWrap",       m.diffuse_wrap);
    rhi->set_uniform_float(shader, "u_Material_AmbientResponse",   m.ambient_response);
    rhi->set_uniform_vec3 (shader, "u_Material_Emissive",          m.emissive);
    rhi->set_uniform_float(shader, "u_Material_EmissiveStrength",  m.emissive_strength);
}

void ForwardRenderer3D::upload_material(u32 id, const SurfaceMaterial& m) {
    // material_id 0 always means "use default"; reject so callers can't
    // accidentally redirect every default-tinted entity to a custom mat.
    if (id == 0) return;
    material_table_[id] = m;
}

const SurfaceMaterial& ForwardRenderer3D::get_material(u32 id) const {
    if (id == 0) return default_material_;
    auto it = material_table_.find(id);
    return it != material_table_.end() ? it->second : default_material_;
}

u32 ForwardRenderer3D::material_count() const {
    return static_cast<u32>(material_table_.size());
}

void ForwardRenderer3D::draw_mesh(const Mesh& mesh, const Mat4& transform,
                                  u32 material_id, bool receive_shadows,
                                  Vec4 tint, rhi::TextureHandle texture) {
    if (!in_frame_) return;

    // Frustum culling — derive the world-space bounding sphere from the
    // mesh's actual local AABB.  Transforming all eight corners then taking
    // the AABB → sphere conversion handles arbitrary rotation, non-uniform
    // scale, and meshes whose pivot is offset from their geometric center
    // (the previous heuristic dropped large meshes — planes, terrain — as
    // soon as the pivot left the frustum even though the geometry stayed
    // visible).  An empty local AABB (min == max) means upload_mesh never
    // ran for this mesh; in that case skip culling rather than guess.
    if (mesh.local_aabb_min != mesh.local_aabb_max) {
        const Vec3& lo = mesh.local_aabb_min;
        const Vec3& hi = mesh.local_aabb_max;
        const Vec3 corners[8] = {
            {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z},
            {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z},
            {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
            {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z},
        };
        Vec3 wlo = Vec3(transform * Vec4(corners[0], 1.0f));
        Vec3 whi = wlo;
        for (u32 i = 1; i < 8; ++i) {
            Vec3 w = Vec3(transform * Vec4(corners[i], 1.0f));
            wlo = glm::min(wlo, w);
            whi = glm::max(whi, w);
        }
        Vec3 center = (wlo + whi) * 0.5f;
        float radius = glm::length(whi - center);
        if (!is_visible(center, radius)) return;
    }

    rhi_->bind_shader(shader_);
    rhi_->set_uniform_mat4(shader_, "u_Model", transform);

    // Normal matrix = transpose(inverse(model))
    Mat4 normal_matrix = glm::transpose(glm::inverse(transform));
    rhi_->set_uniform_mat4(shader_, "u_NormalMatrix", normal_matrix);

    // Resolve material → push every per-surface uniform.  The shader's
    // BRDF reads `u_Material_*` for shininess / specular weight /
    // diffuse wrap / ambient response — none of those values come from
    // the renderer or the shader source any more.
    push_material_uniforms(rhi_, shader_, get_material(material_id));

    // Per-mesh shadow-receive toggle propagated from the
    // MeshRendererComponent.  Off ⇒ fragment shader skips the cascade
    // sample and treats the surface as fully lit, useful for fully
    // emissive geometry (sky domes, particle billboards) that
    // shouldn't darken under another object's shadow.
    rhi_->set_uniform_int(shader_, "u_ReceiveShadows", receive_shadows ? 1 : 0);

    // Per-instance tint multiplied on top of the material's albedo
    // (legacy MeshRendererComponent.tint usage, kept for cheap
    // recolouring without authoring a new material asset).
    rhi_->set_uniform_vec4(shader_, "u_Color", tint);

    rhi::TextureHandle tex = (texture != rhi::INVALID_HANDLE) ? texture : white_texture_;
    rhi_->bind_texture(tex, 0);
    rhi_->set_uniform_int(shader_, "u_Texture", 0);

    rhi_->bind_pipeline(mesh.pipeline);
    rhi_->bind_vertex_buffer(mesh.vbo);
    rhi_->bind_index_buffer(mesh.ibo);
    rhi_->draw_indexed(static_cast<u32>(mesh.indices.size()));
}

// ── Shadow mapping ──────────────────────────────────────────────────────

void ForwardRenderer3D::enable_shadows(const CascadedShadowMap::Config& config) {
    if (!rhi_) return;
    shadow_map_ = std::make_unique<CascadedShadowMap>();
    shadow_map_->init(rhi_, config);
    NX_INFO("ForwardRenderer3D: Cascaded shadow mapping enabled ({} cascades, {}px)",
            config.num_cascades, config.resolution);
}

void ForwardRenderer3D::disable_shadows() {
    if (shadow_map_) {
        shadow_map_->shutdown();
        shadow_map_.reset();
    }
}

void ForwardRenderer3D::render_shadow_pass(Vec3 light_direction,
                                            ShadowGeometryCallback submit_geometry) {
    if (!shadow_map_ || !in_frame_) return;

    // Update cascade splits based on current camera
    shadow_map_->update(current_camera_, light_direction);

    // Render each cascade
    for (u32 c = 0; c < shadow_map_->num_cascades(); ++c) {
        shadow_map_->begin_pass(c);
        submit_geometry(c);
        shadow_map_->end_pass();
    }

    // Re-bind the main shader after shadow pass
    rhi_->bind_shader(shader_);
    rhi_->set_uniform_mat4(shader_, "u_ViewProjection", view_projection_);
    rhi_->set_uniform_vec3(shader_, "u_CameraPos", camera_position_);
}

// ── Primitive mesh generators ───────────────────────────────────────────────

Mesh create_cube_mesh() {
    Mesh mesh;

    // 24 vertices (4 per face, for correct normals)
    mesh.vertices = {
        // Front face (+Z)
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
        // Back face (-Z)
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
        // Top face (+Y)
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
        // Bottom face (-Y)
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
        // Right face (+X)
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        // Left face (-X)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    };

    mesh.indices = {
         0,  1,  2,   2,  3,  0,  // front
         4,  5,  6,   6,  7,  4,  // back
         8,  9, 10,  10, 11,  8,  // top
        12, 13, 14,  14, 15, 12,  // bottom
        16, 17, 18,  18, 19, 16,  // right
        20, 21, 22,  22, 23, 20,  // left
    };

    return mesh;
}

Mesh create_plane_mesh(float size, u32 subdivisions) {
    Mesh mesh;

    float half = size * 0.5f;
    float step = size / static_cast<float>(subdivisions);

    for (u32 z = 0; z <= subdivisions; ++z) {
        for (u32 x = 0; x <= subdivisions; ++x) {
            float px = -half + static_cast<float>(x) * step;
            float pz = -half + static_cast<float>(z) * step;
            float u = static_cast<float>(x) / static_cast<float>(subdivisions);
            float v = static_cast<float>(z) / static_cast<float>(subdivisions);

            mesh.vertices.push_back({
                {px, 0.0f, pz},
                {0.0f, 1.0f, 0.0f},
                {u, v}
            });
        }
    }

    for (u32 z = 0; z < subdivisions; ++z) {
        for (u32 x = 0; x < subdivisions; ++x) {
            u32 tl = z * (subdivisions + 1) + x;
            u32 tr = tl + 1;
            u32 bl = (z + 1) * (subdivisions + 1) + x;
            u32 br = bl + 1;

            mesh.indices.push_back(tl);
            mesh.indices.push_back(bl);
            mesh.indices.push_back(tr);
            mesh.indices.push_back(tr);
            mesh.indices.push_back(bl);
            mesh.indices.push_back(br);
        }
    }

    return mesh;
}

Mesh create_sphere_mesh(float radius, u32 rings, u32 sectors) {
    Mesh mesh;

    for (u32 r = 0; r <= rings; ++r) {
        float phi = math::PI * static_cast<float>(r) / static_cast<float>(rings);
        for (u32 s = 0; s <= sectors; ++s) {
            float theta = math::TWO_PI * static_cast<float>(s) / static_cast<float>(sectors);

            Vec3 pos;
            pos.x = radius * std::sin(phi) * std::cos(theta);
            pos.y = radius * std::cos(phi);
            pos.z = radius * std::sin(phi) * std::sin(theta);

            Vec3 normal = glm::normalize(pos);
            Vec2 uv;
            uv.x = static_cast<float>(s) / static_cast<float>(sectors);
            uv.y = static_cast<float>(r) / static_cast<float>(rings);

            mesh.vertices.push_back({pos, normal, uv});
        }
    }

    // Triangle winding: outward-facing under glFrontFace(GL_CCW) +
    // CullMode::Back.  cur(theta_s, phi_r), cur+1(theta_s+1, phi_r),
    // next(theta_s, phi_r+1).  The cross-product (cur+1 − cur) ×
    // (next − cur) at the equator points outward (+x for theta=0),
    // which is exactly the convention we need.  Earlier order
    // (cur, next, cur+1) had the wrong winding and the GPU culled
    // the front-facing surface, exposing the opposite hemisphere.
    for (u32 r = 0; r < rings; ++r) {
        for (u32 s = 0; s < sectors; ++s) {
            u32 cur = r * (sectors + 1) + s;
            u32 next = cur + sectors + 1;

            mesh.indices.push_back(cur);
            mesh.indices.push_back(cur + 1);
            mesh.indices.push_back(next);

            mesh.indices.push_back(cur + 1);
            mesh.indices.push_back(next + 1);
            mesh.indices.push_back(next);
        }
    }

    return mesh;
}

} // namespace nexus
