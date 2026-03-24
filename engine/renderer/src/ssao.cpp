#include "nexus/renderer/ssao.h"
#include "nexus/core/log.h"
#include <random>
#include <cmath>

namespace nexus {

// ── Shader sources ──────────────────────────────────────────────────────────

static const char* SSAO_VERTEX = R"(
#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

static const char* SSAO_FRAGMENT = R"(
#version 330 core
in vec2 v_TexCoord;
out float FragColor;

uniform sampler2D u_DepthTex;
uniform sampler2D u_NormalTex;
uniform sampler2D u_NoiseTex;

uniform vec3 u_Samples[64];
uniform mat4 u_Projection;
uniform int u_KernelSize;
uniform float u_Radius;
uniform float u_Bias;
uniform float u_Intensity;
uniform vec2 u_NoiseScale;

vec3 reconstructPosition(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inverse(u_Projection) * clipPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    float depth = texture(u_DepthTex, v_TexCoord).r;
    if (depth >= 1.0) { FragColor = 1.0; return; }

    vec3 fragPos = reconstructPosition(v_TexCoord, depth);
    vec3 normal = normalize(texture(u_NormalTex, v_TexCoord).rgb * 2.0 - 1.0);
    vec3 randomVec = normalize(texture(u_NoiseTex, v_TexCoord * u_NoiseScale).xyz * 2.0 - 1.0);

    // Gram-Schmidt to construct TBN
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < u_KernelSize; ++i) {
        vec3 samplePos = fragPos + TBN * u_Samples[i] * u_Radius;

        vec4 offset = u_Projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = texture(u_DepthTex, offset.xy).r;
        vec3 sampleViewPos = reconstructPosition(offset.xy, sampleDepth);

        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / abs(fragPos.z - sampleViewPos.z));
        occlusion += (sampleViewPos.z >= samplePos.z + u_Bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(u_KernelSize)) * u_Intensity;
    FragColor = occlusion;
}
)";

static const char* BLUR_FRAGMENT = R"(
#version 330 core
in vec2 v_TexCoord;
out float FragColor;

uniform sampler2D u_Input;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_Input, 0));
    float result = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(u_Input, v_TexCoord + offset).r;
        }
    }
    FragColor = result / 25.0;
}
)";

// ── Fullscreen quad ─────────────────────────────────────────────────────────

static const float QUAD_VERTICES[] = {
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
};

static rhi::PipelineHandle create_pp_pipeline(rhi::RHI* rhi, rhi::ShaderHandle shader) {
    rhi::PipelineDesc desc{};
    desc.shader = shader;
    desc.blend = rhi::BlendMode::None;
    desc.depth_test = false;
    desc.depth_write = false;
    desc.cull = rhi::CullMode::None;
    desc.primitive = rhi::PrimitiveType::Triangles;
    desc.vertex_layout.stride = sizeof(float) * 4;
    desc.vertex_layout.attributes = {
        {0, 2, 0, false},                    // position
        {1, 2, sizeof(float) * 2, false},    // texcoord
    };
    return rhi->create_pipeline(desc);
}

// ── Implementation ──────────────────────────────────────────────────────────

void SSAOEffect::init(rhi::RHI* rhi, u32 width, u32 height) {
    rhi_ = rhi;
    width_ = width;
    height_ = height;

    generate_kernel();

    ssao_shader_ = rhi->create_shader(SSAO_VERTEX, SSAO_FRAGMENT);
    blur_shader_ = rhi->create_shader(SSAO_VERTEX, BLUR_FRAGMENT);

    ssao_pipeline_ = create_pp_pipeline(rhi, ssao_shader_);
    blur_pipeline_ = create_pp_pipeline(rhi, blur_shader_);

    // Quad VBO
    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.data = QUAD_VERTICES;
    vbo_desc.size = sizeof(QUAD_VERTICES);
    quad_vbo_ = rhi->create_buffer(vbo_desc);

    // SSAO framebuffer
    rhi::FramebufferDesc fb_desc;
    fb_desc.width = width;
    fb_desc.height = height;
    fb_desc.color_attachments = {rhi::TextureFormat::R8};
    fb_desc.has_depth = false;
    ssao_fb_ = rhi->create_framebuffer(fb_desc);
    blur_fb_ = rhi->create_framebuffer(fb_desc);

    // SSAO textures (for binding as input)
    rhi::TextureDesc td{};
    td.width = width;
    td.height = height;
    td.format = rhi::TextureFormat::R8;
    td.generate_mipmaps = false;
    ssao_tex_ = rhi->create_texture(td);
    blur_tex_ = rhi->create_texture(td);

    generate_noise_texture();
}

void SSAOEffect::shutdown() {}

void SSAOEffect::resize(u32 width, u32 height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
}

void SSAOEffect::apply(rhi::RHI* rhi, rhi::TextureHandle input,
                        rhi::FramebufferHandle dest) {
    u32 ks = std::min(kernel_size, MAX_KERNEL_SIZE);

    // Pass 1: SSAO
    rhi->bind_framebuffer(ssao_fb_);
    rhi->set_viewport(0, 0, static_cast<i32>(width_), static_cast<i32>(height_));
    rhi->clear(Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    rhi->bind_pipeline(ssao_pipeline_);
    rhi->bind_texture(depth_tex_, 0);
    rhi->bind_texture(normal_tex_, 1);
    rhi->bind_texture(noise_tex_, 2);

    rhi->set_uniform_int(ssao_shader_, "u_DepthTex", 0);
    rhi->set_uniform_int(ssao_shader_, "u_NormalTex", 1);
    rhi->set_uniform_int(ssao_shader_, "u_NoiseTex", 2);
    rhi->set_uniform_int(ssao_shader_, "u_KernelSize", static_cast<i32>(ks));
    rhi->set_uniform_float(ssao_shader_, "u_Radius", radius);
    rhi->set_uniform_float(ssao_shader_, "u_Bias", bias);
    rhi->set_uniform_float(ssao_shader_, "u_Intensity", intensity);
    rhi->set_uniform_vec2(ssao_shader_, "u_NoiseScale",
                           Vec2(static_cast<float>(width_) / 4.0f,
                                static_cast<float>(height_) / 4.0f));
    rhi->set_uniform_mat4(ssao_shader_, "u_Projection", projection_);

    for (u32 i = 0; i < ks; ++i) {
        std::string uname = "u_Samples[" + std::to_string(i) + "]";
        rhi->set_uniform_vec3(ssao_shader_, uname, kernel_[i]);
    }

    rhi->bind_vertex_buffer(quad_vbo_);
    rhi->draw(6, 0);

    // Pass 2: Blur
    rhi->bind_framebuffer(blur_fb_);
    rhi->clear(Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    rhi->bind_pipeline(blur_pipeline_);
    rhi->bind_texture(ssao_tex_, 0);
    rhi->set_uniform_int(blur_shader_, "u_Input", 0);
    rhi->bind_vertex_buffer(quad_vbo_);
    rhi->draw(6, 0);

    // The blurred AO result is in blur_tex_ for deferred lighting to sample
    rhi->bind_framebuffer(dest);
}

void SSAOEffect::generate_kernel() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (u32 i = 0; i < MAX_KERNEL_SIZE; ++i) {
        Vec3 s(
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f,
            dist(rng)
        );
        s = glm::normalize(s);
        s *= dist(rng);

        float scale = static_cast<float>(i) / static_cast<float>(MAX_KERNEL_SIZE);
        scale = math::lerp(0.1f, 1.0f, scale * scale);
        s *= scale;

        kernel_[i] = s;
    }
}

void SSAOEffect::generate_noise_texture() {
    if (!rhi_) return;

    std::mt19937 rng(0);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::array<u8, 4 * 4 * 4> noise_data;
    for (u32 i = 0; i < 16; ++i) {
        float x = dist(rng) * 2.0f - 1.0f;
        float y = dist(rng) * 2.0f - 1.0f;
        noise_data[i * 4 + 0] = static_cast<u8>((x * 0.5f + 0.5f) * 255.0f);
        noise_data[i * 4 + 1] = static_cast<u8>((y * 0.5f + 0.5f) * 255.0f);
        noise_data[i * 4 + 2] = 128;
        noise_data[i * 4 + 3] = 255;
    }

    rhi::TextureDesc td{};
    td.width = 4;
    td.height = 4;
    td.format = rhi::TextureFormat::RGBA8;
    td.wrap_s = rhi::TextureWrap::Repeat;
    td.wrap_t = rhi::TextureWrap::Repeat;
    td.generate_mipmaps = false;
    td.data = noise_data.data();
    noise_tex_ = rhi_->create_texture(td);
}

} // namespace nexus
