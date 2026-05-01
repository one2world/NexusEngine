#include "nexus/renderer/gpu_particles.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace nexus {

// ── GPU shaders (GL 4.3+ compute path) ─────────────────────────────────────

// Compute shader: advances every slot of the particle pool.  Alive particles
// integrate and may expire; dead slots may be reborn when the per-frame
// emission budget has capacity.  Two atomics (alive_count / emit_cursor) are
// reset from the CPU each frame before dispatch.
static const char* COMPUTE_SIM_SRC = R"(
#version 430 core
layout(local_size_x = 64) in;

struct Particle {
    vec4 pos_life;     // xyz = position, w = lifetime
    vec4 vel_max;      // xyz = velocity, w = max_lifetime
    vec4 color;
    vec4 size_pad;     // x = size, yzw = pad
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};
layout(std430, binding = 1) buffer CounterBuffer {
    uint alive_count;
    uint emit_cursor;
};

uniform float u_DeltaTime;
uniform vec3  u_Position;
uniform vec3  u_Direction;
uniform float u_SpreadCos;
uniform float u_MinSpeed;
uniform float u_MaxSpeed;
uniform float u_MinLifetime;
uniform float u_MaxLifetime;
uniform vec4  u_StartColor;
uniform vec4  u_EndColor;
uniform float u_StartSize;
uniform float u_EndSize;
uniform float u_Gravity;
uniform uint  u_MaxParticles;
uniform uint  u_EmitBudget;
uniform uint  u_FrameSeed;

uint hash(uint x) {
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    return x;
}
float randf(inout uint s) {
    s = hash(s);
    return float(s) / 4294967295.0;
}
float rand_range(inout uint s, float lo, float hi) {
    return lo + randf(s) * (hi - lo);
}
vec3 rand_cone(inout uint s, vec3 dir, float cos_half) {
    float z   = rand_range(s, cos_half, 1.0);
    float phi = rand_range(s, 0.0, 6.2831853);
    float st  = sqrt(max(0.0, 1.0 - z*z));
    vec3 local = vec3(st*cos(phi), st*sin(phi), z);
    vec3 up    = (abs(dir.y) > 0.9) ? vec3(1.0,0.0,0.0) : vec3(0.0,1.0,0.0);
    vec3 right = normalize(cross(dir, up));
    up = cross(right, dir);
    return normalize(right*local.x + up*local.y + dir*local.z);
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_MaxParticles) return;

    Particle p = particles[idx];

    // Integrate alive particles
    if (p.pos_life.w > 0.0) {
        p.pos_life.w   -= u_DeltaTime;
        p.vel_max.y    += u_Gravity * u_DeltaTime;
        p.pos_life.xyz += p.vel_max.xyz * u_DeltaTime;
        if (p.pos_life.w > 0.0) {
            float t = clamp(1.0 - p.pos_life.w / p.vel_max.w, 0.0, 1.0);
            p.color     = mix(u_StartColor, u_EndColor, t);
            p.size_pad.x = mix(u_StartSize, u_EndSize, t);
            atomicAdd(alive_count, 1u);
        }
    }

    // If slot is dead (either pre-existing or just expired), try to emit.
    if (p.pos_life.w <= 0.0) {
        uint ticket = atomicAdd(emit_cursor, 1u);
        if (ticket < u_EmitBudget) {
            uint seed = u_FrameSeed
                      ^ (idx * 747796405u + 2891336453u)
                      ^ (ticket * 2246822519u);
            vec3 dir   = rand_cone(seed, u_Direction, u_SpreadCos);
            float sp   = rand_range(seed, u_MinSpeed, u_MaxSpeed);
            float life = rand_range(seed, u_MinLifetime, u_MaxLifetime);
            p.pos_life   = vec4(u_Position, life);
            p.vel_max    = vec4(dir * sp, life);
            p.color      = u_StartColor;
            p.size_pad.x = u_StartSize;
            atomicAdd(alive_count, 1u);
        }
    }

    particles[idx] = p;
}
)";

// Rendering — reads particle SSBO; draws 6 verts per particle using gl_VertexID.
static const char* RENDER_VERTEX_SRC = R"(
#version 430 core

struct Particle {
    vec4 pos_life;
    vec4 vel_max;
    vec4 color;
    vec4 size_pad;
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec2 v_TexCoord;
out vec4 v_Color;

const vec2 QUAD[6] = vec2[](
    vec2(-1.0,-1.0), vec2( 1.0,-1.0), vec2( 1.0, 1.0),
    vec2(-1.0,-1.0), vec2( 1.0, 1.0), vec2(-1.0, 1.0)
);

void main() {
    uint pid = uint(gl_VertexID) / 6u;
    uint qv  = uint(gl_VertexID) % 6u;
    vec2 off = QUAD[qv];
    Particle p = particles[pid];

    if (p.pos_life.w <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);   // off-screen; discarded
        v_Color = vec4(0.0);
        v_TexCoord = vec2(0.0);
        return;
    }

    float sz = p.size_pad.x;
    vec3 world = p.pos_life.xyz
               + u_CameraRight * off.x * sz
               + u_CameraUp    * off.y * sz;
    gl_Position = u_ViewProjection * vec4(world, 1.0);
    v_TexCoord = off * 0.5 + 0.5;
    v_Color = p.color;
}
)";

static const char* RENDER_FRAGMENT_SRC = R"(
#version 430 core
in vec2 v_TexCoord;
in vec4 v_Color;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform bool u_HasTexture;

void main() {
    vec4 tex = u_HasTexture ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    float d  = length(v_TexCoord - vec2(0.5));
    float a  = smoothstep(0.5, 0.4, d);
    FragColor = v_Color * tex * vec4(1.0, 1.0, 1.0, a);
    if (FragColor.a < 0.01) discard;
}
)";

// ── Legacy CPU-only shaders (fallback when compute unavailable) ────────────

static const char* CPU_VERTEX_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in float a_Size;
layout(location = 3) in vec2 a_QuadVertex;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    vec3 world = a_Position
        + u_CameraRight * a_QuadVertex.x * a_Size
        + u_CameraUp    * a_QuadVertex.y * a_Size;
    gl_Position = u_ViewProjection * vec4(world, 1.0);
    v_TexCoord = a_QuadVertex * 0.5 + 0.5;
    v_Color = a_Color;
}
)";

static const char* CPU_FRAGMENT_SRC = R"(
#version 330 core
in vec2 v_TexCoord;
in vec4 v_Color;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform bool u_HasTexture;

void main() {
    vec4 tex = u_HasTexture ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    float d  = length(v_TexCoord - vec2(0.5));
    float a  = smoothstep(0.5, 0.4, d);
    FragColor = v_Color * tex * vec4(1.0, 1.0, 1.0, a);
    if (FragColor.a < 0.01) discard;
}
)";

// ── RNG helpers (CPU fallback only) ────────────────────────────────────────

float GPUParticleSystem::rand_float() {
    rng_state_ ^= rng_state_ << 13;
    rng_state_ ^= rng_state_ >> 17;
    rng_state_ ^= rng_state_ << 5;
    return static_cast<float>(rng_state_) / static_cast<float>(0xFFFFFFFFu);
}

float GPUParticleSystem::rand_range(float min, float max) {
    return min + rand_float() * (max - min);
}

Vec3 GPUParticleSystem::rand_cone(Vec3 direction, float angle) {
    float half_angle = glm::radians(angle * 0.5f);
    float cos_angle  = std::cos(half_angle);

    float z = rand_range(cos_angle, 1.0f);
    float phi = rand_range(0.0f, 2.0f * 3.14159265f);
    float st = std::sqrt(std::max(0.0f, 1.0f - z * z));

    Vec3 local(st * std::cos(phi), st * std::sin(phi), z);
    Vec3 up    = (std::abs(direction.y) > 0.9f) ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
    Vec3 right = glm::normalize(glm::cross(direction, up));
    up = glm::cross(right, direction);
    return glm::normalize(right * local.x + up * local.y + direction * local.z);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void GPUParticleSystem::init(rhi::RHI* rhi, const GPUParticleEmitterConfig& config) {
    rhi_ = rhi;
    config_ = config;
    alive_count_ = 0;
    emission_accumulator_ = 0.0f;
    frame_seed_ = 0x12345u;

    uses_gpu_compute_ = rhi_ && rhi_->supports_compute();

    if (uses_gpu_compute_) {
        // Compile compute and render shaders
        compute_shader_ = rhi_->create_compute_shader(COMPUTE_SIM_SRC);
        render_shader_  = rhi_->create_shader(RENDER_VERTEX_SRC, RENDER_FRAGMENT_SRC);

        if (compute_shader_ == rhi::INVALID_HANDLE ||
            render_shader_  == rhi::INVALID_HANDLE) {
            NX_WARN("GPUParticleSystem: compute/render shader compile failed — "
                    "falling back to CPU path");
            uses_gpu_compute_ = false;
        }
    }

    if (uses_gpu_compute_) {
        // Particle SSBO: zero-initialised so all slots start dead.
        std::vector<u8> zeros(config_.max_particles * sizeof(GPUParticle), 0);
        rhi::BufferDesc pdesc{};
        pdesc.type  = rhi::BufferType::Storage;
        pdesc.usage = rhi::BufferUsage::Dynamic;
        pdesc.data  = zeros.data();
        pdesc.size  = zeros.size();
        particle_ssbo_ = rhi_->create_buffer(pdesc);

        // Counter SSBO: alive_count + emit_cursor
        u32 counters[2] = { 0u, 0u };
        rhi::BufferDesc cdesc{};
        cdesc.type  = rhi::BufferType::Storage;
        cdesc.usage = rhi::BufferUsage::Dynamic;
        cdesc.data  = counters;
        cdesc.size  = sizeof(counters);
        counter_ssbo_ = rhi_->create_buffer(cdesc);

        // Rendering pipeline — no vertex attributes; VS uses gl_VertexID.
        rhi::PipelineDesc pdl{};
        pdl.shader      = render_shader_;
        pdl.blend       = config_.additive_blend ? rhi::BlendMode::Additive
                                                  : rhi::BlendMode::Alpha;
        pdl.depth_test  = true;
        pdl.depth_write = false;
        pdl.cull        = rhi::CullMode::None;
        pdl.primitive   = rhi::PrimitiveType::Triangles;
        pdl.vertex_layout.stride = 0;
        pipeline_ = rhi_->create_pipeline(pdl);

        NX_INFO("GPUParticleSystem: GPU compute path active "
                "(max {} particles, {} MiB VRAM)",
                config_.max_particles,
                (config_.max_particles * sizeof(GPUParticle)) / (1024*1024));
        return;
    }

    // CPU fallback path (unchanged for parity)
    cpu_particles_.reserve(config_.max_particles);
    render_shader_ = rhi_->create_shader(CPU_VERTEX_SRC, CPU_FRAGMENT_SRC);

    rhi::PipelineDesc pdl{};
    pdl.shader      = render_shader_;
    pdl.blend       = config_.additive_blend ? rhi::BlendMode::Additive
                                              : rhi::BlendMode::Alpha;
    pdl.depth_test  = true;
    pdl.depth_write = false;
    pdl.cull        = rhi::CullMode::None;
    pdl.primitive   = rhi::PrimitiveType::Triangles;
    pdl.vertex_layout.stride = sizeof(float) * 10;
    pdl.vertex_layout.attributes = {
        {0, 3, 0,                false},
        {1, 4, sizeof(float)*3,  false},
        {2, 1, sizeof(float)*7,  false},
        {3, 2, sizeof(float)*8,  false},
    };
    pipeline_ = rhi_->create_pipeline(pdl);

    rhi::BufferDesc vbo_desc{};
    vbo_desc.type  = rhi::BufferType::Vertex;
    vbo_desc.size  = config_.max_particles * 6 * sizeof(float) * 10;
    vbo_desc.usage = rhi::BufferUsage::Dynamic;
    vbo_desc.data  = nullptr;
    cpu_vbo_ = rhi_->create_buffer(vbo_desc);

    NX_INFO("GPUParticleSystem: CPU fallback path (compute unavailable) — "
            "max {} particles", config_.max_particles);
}

void GPUParticleSystem::shutdown() {
    if (!rhi_) return;
    if (pipeline_       != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(pipeline_);
    if (render_shader_  != rhi::INVALID_HANDLE) rhi_->destroy_shader(render_shader_);
    if (compute_shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(compute_shader_);
    if (particle_ssbo_  != rhi::INVALID_HANDLE) rhi_->destroy_buffer(particle_ssbo_);
    if (counter_ssbo_   != rhi::INVALID_HANDLE) rhi_->destroy_buffer(counter_ssbo_);
    if (quad_vbo_       != rhi::INVALID_HANDLE) rhi_->destroy_buffer(quad_vbo_);
    if (cpu_vbo_        != rhi::INVALID_HANDLE) rhi_->destroy_buffer(cpu_vbo_);

    pipeline_ = render_shader_ = compute_shader_ = rhi::INVALID_HANDLE;
    particle_ssbo_ = counter_ssbo_ = quad_vbo_ = cpu_vbo_ = rhi::INVALID_HANDLE;
    cpu_particles_.clear();
    alive_count_ = 0;
    rhi_ = nullptr;
}

// ── Update (dispatch compute or CPU fallback) ──────────────────────────────

void GPUParticleSystem::update(float dt) {
    if (uses_gpu_compute_) {
        update_gpu(dt);
    } else {
        update_cpu_fallback(dt);
    }
}

void GPUParticleSystem::update_gpu(float dt) {
    if (!rhi_ || compute_shader_ == rhi::INVALID_HANDLE) return;

    // Compute this frame's emission budget
    emission_accumulator_ += config_.emission_rate * dt;
    u32 emit_budget = static_cast<u32>(emission_accumulator_);
    emission_accumulator_ -= static_cast<float>(emit_budget);
    if (emit_budget > config_.max_particles) emit_budget = config_.max_particles;

    // Reset atomic counters on GPU (alive_count=0, emit_cursor=0)
    u32 zero_counters[2] = { 0u, 0u };
    rhi_->update_buffer(counter_ssbo_, zero_counters, sizeof(zero_counters), 0);

    // Bind buffers & shader, push uniforms, dispatch
    rhi_->bind_shader(compute_shader_);
    rhi_->bind_storage_buffer(particle_ssbo_, 0);
    rhi_->bind_storage_buffer(counter_ssbo_,  1);

    float cos_half = std::cos(glm::radians(config_.spread_angle * 0.5f));

    rhi_->set_uniform_float(compute_shader_, "u_DeltaTime", dt);
    rhi_->set_uniform_vec3 (compute_shader_, "u_Position", config_.position);
    rhi_->set_uniform_vec3 (compute_shader_, "u_Direction", glm::normalize(config_.direction));
    rhi_->set_uniform_float(compute_shader_, "u_SpreadCos", cos_half);
    rhi_->set_uniform_float(compute_shader_, "u_MinSpeed", config_.min_speed);
    rhi_->set_uniform_float(compute_shader_, "u_MaxSpeed", config_.max_speed);
    rhi_->set_uniform_float(compute_shader_, "u_MinLifetime", config_.min_lifetime);
    rhi_->set_uniform_float(compute_shader_, "u_MaxLifetime", config_.max_lifetime);
    rhi_->set_uniform_vec4 (compute_shader_, "u_StartColor", config_.start_color);
    rhi_->set_uniform_vec4 (compute_shader_, "u_EndColor",   config_.end_color);
    rhi_->set_uniform_float(compute_shader_, "u_StartSize", config_.start_size);
    rhi_->set_uniform_float(compute_shader_, "u_EndSize",   config_.end_size);
    rhi_->set_uniform_float(compute_shader_, "u_Gravity",   config_.gravity);
    rhi_->set_uniform_uint (compute_shader_, "u_MaxParticles", config_.max_particles);
    rhi_->set_uniform_uint (compute_shader_, "u_EmitBudget",  emit_budget);

    // Per-frame seed — xorshift to decorrelate frames
    frame_seed_ ^= frame_seed_ << 13;
    frame_seed_ ^= frame_seed_ >> 17;
    frame_seed_ ^= frame_seed_ << 5;
    if (frame_seed_ == 0) frame_seed_ = 0x9E3779B9u;
    rhi_->set_uniform_uint(compute_shader_, "u_FrameSeed", frame_seed_);

    constexpr u32 LOCAL_SIZE_X = 64;
    u32 groups = (config_.max_particles + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
    rhi_->dispatch_compute(groups, 1, 1);
    rhi_->memory_barrier();

    // Read back alive count (synchronous — for telemetry & alive_count() API).
    u32 counters[2] = { 0u, 0u };
    rhi_->read_buffer(counter_ssbo_, counters, sizeof(counters), 0);
    alive_count_ = counters[0];
}

void GPUParticleSystem::update_cpu_fallback(float dt) {
    // Emit
    emission_accumulator_ += config_.emission_rate * dt;
    u32 to_emit = static_cast<u32>(emission_accumulator_);
    emission_accumulator_ -= static_cast<float>(to_emit);

    for (u32 i = 0; i < to_emit && alive_count_ < config_.max_particles; ++i) {
        GPUParticle p{};
        p.position = config_.position;
        float speed = rand_range(config_.min_speed, config_.max_speed);
        Vec3 dir = rand_cone(config_.direction, config_.spread_angle);
        p.velocity = dir * speed;
        p.max_lifetime = rand_range(config_.min_lifetime, config_.max_lifetime);
        p.lifetime = p.max_lifetime;
        p.color = config_.start_color;
        p.size = config_.start_size;
        if (alive_count_ < cpu_particles_.size()) {
            cpu_particles_[alive_count_] = p;
        } else {
            cpu_particles_.push_back(p);
        }
        ++alive_count_;
    }

    // Simulate
    for (u32 i = 0; i < alive_count_; ++i) {
        auto& p = cpu_particles_[i];
        p.lifetime    -= dt;
        p.velocity.y  += config_.gravity * dt;
        p.position    += p.velocity * dt;
        float t = glm::clamp(1.0f - (p.lifetime / p.max_lifetime), 0.0f, 1.0f);
        p.color = glm::mix(config_.start_color, config_.end_color, t);
        p.size  = glm::mix(config_.start_size,  config_.end_size,  t);
    }

    // Compact
    u32 i = 0;
    while (i < alive_count_) {
        if (cpu_particles_[i].lifetime <= 0.0f) {
            --alive_count_;
            if (i < alive_count_) cpu_particles_[i] = cpu_particles_[alive_count_];
        } else {
            ++i;
        }
    }
}

// ── Render ─────────────────────────────────────────────────────────────────

void GPUParticleSystem::render(const Mat4& view, const Mat4& projection,
                                Vec3 camera_right, Vec3 camera_up) {
    if (!rhi_) return;

    const Mat4 vp = projection * view;

    if (uses_gpu_compute_) {
        if (render_shader_ == rhi::INVALID_HANDLE) return;
        rhi_->bind_pipeline(pipeline_);
        rhi_->bind_shader(render_shader_);
        rhi_->bind_storage_buffer(particle_ssbo_, 0);

        rhi_->set_uniform_mat4(render_shader_, "u_ViewProjection", vp);
        rhi_->set_uniform_vec3(render_shader_, "u_CameraRight",    camera_right);
        rhi_->set_uniform_vec3(render_shader_, "u_CameraUp",       camera_up);

        bool has_tex = (texture_ != rhi::INVALID_HANDLE);
        rhi_->set_uniform_int(render_shader_, "u_HasTexture", has_tex ? 1 : 0);
        if (has_tex) {
            rhi_->bind_texture(texture_, 0);
            rhi_->set_uniform_int(render_shader_, "u_Texture", 0);
        }

        // 6 vertices per slot; dead slots are culled off-screen in the VS.
        rhi_->draw(config_.max_particles * 6);
        return;
    }

    // CPU fallback: build vertex buffer & draw
    if (alive_count_ == 0) return;

    static const Vec2 quad[6] = {
        {-1,-1}, {1,-1}, {1, 1},
        {-1,-1}, {1, 1}, {-1,1}
    };

    std::vector<float> verts;
    verts.reserve(alive_count_ * 6 * 10);
    for (u32 i = 0; i < alive_count_; ++i) {
        const auto& p = cpu_particles_[i];
        for (int q = 0; q < 6; ++q) {
            verts.push_back(p.position.x);
            verts.push_back(p.position.y);
            verts.push_back(p.position.z);
            verts.push_back(p.color.r);
            verts.push_back(p.color.g);
            verts.push_back(p.color.b);
            verts.push_back(p.color.a);
            verts.push_back(p.size);
            verts.push_back(quad[q].x);
            verts.push_back(quad[q].y);
        }
    }

    rhi_->update_buffer(cpu_vbo_, verts.data(), verts.size() * sizeof(float), 0);

    rhi_->bind_pipeline(pipeline_);
    rhi_->bind_shader(render_shader_);
    rhi_->set_uniform_mat4(render_shader_, "u_ViewProjection", vp);
    rhi_->set_uniform_vec3(render_shader_, "u_CameraRight",    camera_right);
    rhi_->set_uniform_vec3(render_shader_, "u_CameraUp",       camera_up);

    bool has_tex = (texture_ != rhi::INVALID_HANDLE);
    rhi_->set_uniform_int(render_shader_, "u_HasTexture", has_tex ? 1 : 0);
    if (has_tex) {
        rhi_->bind_texture(texture_, 0);
        rhi_->set_uniform_int(render_shader_, "u_Texture", 0);
    }

    rhi_->bind_vertex_buffer(cpu_vbo_);
    rhi_->draw(alive_count_ * 6);
}

// (read_back_alive_count declared but folded into update_gpu for now)
void GPUParticleSystem::read_back_alive_count() {
    if (!uses_gpu_compute_ || !rhi_ || counter_ssbo_ == rhi::INVALID_HANDLE) return;
    u32 counters[2] = { 0u, 0u };
    rhi_->read_buffer(counter_ssbo_, counters, sizeof(counters), 0);
    alive_count_ = counters[0];
}

} // namespace nexus
