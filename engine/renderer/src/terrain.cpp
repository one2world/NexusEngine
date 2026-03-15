#include "nexus/renderer/terrain.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <cmath>

namespace nexus {

// ── Heightmap ───────────────────────────────────────────────────────────────

void Heightmap::create(u32 width, u32 height, float default_height) {
    width_ = width;
    height_ = height;
    data_.assign(static_cast<size_t>(width) * height, default_height);
}

void Heightmap::create_from_data(u32 width, u32 height, const float* data) {
    width_ = width;
    height_ = height;
    size_t count = static_cast<size_t>(width) * height;
    data_.assign(data, data + count);
}

float Heightmap::get(u32 x, u32 z) const {
    if (x >= width_ || z >= height_) return 0.0f;
    return data_[static_cast<size_t>(z) * width_ + x];
}

void Heightmap::set(u32 x, u32 z, float value) {
    if (x >= width_ || z >= height_) return;
    data_[static_cast<size_t>(z) * width_ + x] = value;
}

float Heightmap::sample(float u, float v) const {
    if (width_ == 0 || height_ == 0) return 0.0f;

    float fx = u * static_cast<float>(width_ - 1);
    float fz = v * static_cast<float>(height_ - 1);

    u32 x0 = static_cast<u32>(std::floor(fx));
    u32 z0 = static_cast<u32>(std::floor(fz));
    u32 x1 = std::min(x0 + 1, width_ - 1);
    u32 z1 = std::min(z0 + 1, height_ - 1);

    float tx = fx - static_cast<float>(x0);
    float tz = fz - static_cast<float>(z0);

    float h00 = get(x0, z0);
    float h10 = get(x1, z0);
    float h01 = get(x0, z1);
    float h11 = get(x1, z1);

    float h0 = math::lerp(h00, h10, tx);
    float h1 = math::lerp(h01, h11, tx);
    return math::lerp(h0, h1, tz);
}

Vec3 Heightmap::normal_at(u32 x, u32 z, float cell_size) const {
    float hL = get(x > 0 ? x - 1 : x, z);
    float hR = get(x + 1 < width_ ? x + 1 : x, z);
    float hD = get(x, z > 0 ? z - 1 : z);
    float hU = get(x, z + 1 < height_ ? z + 1 : z);

    Vec3 n(hL - hR, 2.0f * cell_size, hD - hU);
    return glm::normalize(n);
}

// ── TerrainRenderer ─────────────────────────────────────────────────────────

namespace terrain_shaders {

const char* VERTEX = R"(
#version 330 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

uniform mat4 u_ViewProjection;

void main() {
    v_WorldPos = a_Position;
    v_Normal = a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

const char* FRAGMENT = R"(
#version 330 core
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

out vec4 FragColor;

uniform vec3 u_SunDirection;
uniform vec3 u_SunColor;

void main() {
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(-u_SunDirection);
    float NdotL = max(dot(N, L), 0.0);

    // Simple terrain shading based on height and normal
    vec3 grassColor = vec3(0.2, 0.5, 0.1);
    vec3 rockColor = vec3(0.5, 0.4, 0.3);
    vec3 snowColor = vec3(0.9, 0.9, 0.95);

    float slope = 1.0 - N.y;
    float height = v_WorldPos.y;

    vec3 baseColor = mix(grassColor, rockColor, smoothstep(0.3, 0.7, slope));
    baseColor = mix(baseColor, snowColor, smoothstep(30.0, 50.0, height));

    vec3 ambient = vec3(0.1) * baseColor;
    vec3 diffuse = baseColor * u_SunColor * NdotL;

    FragColor = vec4(ambient + diffuse, 1.0);
}
)";

} // namespace terrain_shaders

void TerrainRenderer::init(rhi::RHI* rhi, const Config& config) {
    rhi_ = rhi;
    config_ = config;

    shader_ = rhi->create_shader(terrain_shaders::VERTEX,
                                  terrain_shaders::FRAGMENT);

    rhi::PipelineDesc desc;
    desc.shader = shader_;
    desc.blend = rhi::BlendMode::None;
    desc.depth_test = true;
    desc.depth_write = true;
    desc.cull = rhi::CullMode::Back;
    desc.primitive = rhi::PrimitiveType::Triangles;
    desc.vertex_layout.stride = sizeof(float) * 8; // pos(3) + normal(3) + uv(2)
    desc.vertex_layout.attributes = {
        {0, 3, 0, false},
        {1, 3, sizeof(float) * 3, false},
        {2, 2, sizeof(float) * 6, false},
    };
    pipeline_ = rhi->create_pipeline(desc);

    NX_INFO("TerrainRenderer initialized");
}

void TerrainRenderer::shutdown() {
    if (!rhi_) return;
    for (auto& chunk : chunks_) {
        rhi_->destroy_buffer(chunk.vbo);
        rhi_->destroy_buffer(chunk.ibo);
    }
    chunks_.clear();
    rhi_->destroy_pipeline(pipeline_);
    rhi_->destroy_shader(shader_);
    rhi_ = nullptr;
}

void TerrainRenderer::load_heightmap(const Heightmap& heightmap) {
    heightmap_ = heightmap;
    // Destroy old chunks
    for (auto& chunk : chunks_) {
        rhi_->destroy_buffer(chunk.vbo);
        rhi_->destroy_buffer(chunk.ibo);
    }
    chunks_.clear();
    generate_chunks();
}

void TerrainRenderer::set_splat_layers(const std::vector<SplatLayer>& layers) {
    splat_layers_ = layers;
}

float TerrainRenderer::height_at(float world_x, float world_z) const {
    if (heightmap_.width() == 0 || heightmap_.height() == 0) return 0.0f;

    float u = world_x / (static_cast<float>(heightmap_.width() - 1) * config_.cell_size);
    float v = world_z / (static_cast<float>(heightmap_.height() - 1) * config_.cell_size);

    u = math::clamp(u, 0.0f, 1.0f);
    v = math::clamp(v, 0.0f, 1.0f);

    return heightmap_.sample(u, v) * config_.height_scale;
}

Vec3 TerrainRenderer::normal_at(float world_x, float world_z) const {
    if (heightmap_.width() == 0 || heightmap_.height() == 0) return Vec3(0, 1, 0);

    u32 x = static_cast<u32>(world_x / config_.cell_size);
    u32 z = static_cast<u32>(world_z / config_.cell_size);
    x = std::min(x, heightmap_.width() - 1);
    z = std::min(z, heightmap_.height() - 1);

    return heightmap_.normal_at(x, z, config_.cell_size * config_.height_scale);
}

void TerrainRenderer::generate_chunks() {
    u32 hm_w = heightmap_.width();
    u32 hm_h = heightmap_.height();
    if (hm_w < 2 || hm_h < 2) return;

    u32 chunk_sz = config_.chunk_size;

    for (u32 cz = 0; cz < hm_h - 1; cz += chunk_sz) {
        for (u32 cx = 0; cx < hm_w - 1; cx += chunk_sz) {
            chunks_.push_back(generate_chunk(cx, cz, 0));
        }
    }

    NX_INFO("Terrain: {} chunks generated", chunks_.size());
}

TerrainChunk TerrainRenderer::generate_chunk(u32 start_x, u32 start_z, u32 lod) {
    u32 step = 1u << lod;
    u32 end_x = std::min(start_x + config_.chunk_size, heightmap_.width() - 1);
    u32 end_z = std::min(start_z + config_.chunk_size, heightmap_.height() - 1);

    std::vector<float> vertices;
    std::vector<u32> indices;

    u32 cols = 0, rows = 0;
    for (u32 z = start_z; z <= end_z; z += step) {
        cols = 0;
        for (u32 x = start_x; x <= end_x; x += step) {
            float wx = static_cast<float>(x) * config_.cell_size;
            float wz = static_cast<float>(z) * config_.cell_size;
            float wy = heightmap_.get(x, z) * config_.height_scale;

            Vec3 n = heightmap_.normal_at(x, z, config_.cell_size * config_.height_scale);

            float u = static_cast<float>(x) / static_cast<float>(heightmap_.width() - 1);
            float v = static_cast<float>(z) / static_cast<float>(heightmap_.height() - 1);

            vertices.push_back(wx);
            vertices.push_back(wy);
            vertices.push_back(wz);
            vertices.push_back(n.x);
            vertices.push_back(n.y);
            vertices.push_back(n.z);
            vertices.push_back(u);
            vertices.push_back(v);

            ++cols;
        }
        ++rows;
    }

    // Generate triangle indices
    for (u32 r = 0; r < rows - 1; ++r) {
        for (u32 c = 0; c < cols - 1; ++c) {
            u32 tl = r * cols + c;
            u32 tr = tl + 1;
            u32 bl = (r + 1) * cols + c;
            u32 br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    TerrainChunk chunk;
    chunk.lod = lod;
    chunk.index_count = static_cast<u32>(indices.size());

    float center_x = (static_cast<float>(start_x) + static_cast<float>(end_x)) * 0.5f * config_.cell_size;
    float center_z = (static_cast<float>(start_z) + static_cast<float>(end_z)) * 0.5f * config_.cell_size;
    chunk.center = Vec3(center_x, 0.0f, center_z);
    chunk.size = static_cast<float>(config_.chunk_size) * config_.cell_size;

    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.data = vertices.data();
    vbo_desc.size = vertices.size() * sizeof(float);
    chunk.vbo = rhi_->create_buffer(vbo_desc);

    rhi::BufferDesc ibo_desc;
    ibo_desc.type = rhi::BufferType::Index;
    ibo_desc.usage = rhi::BufferUsage::Static;
    ibo_desc.data = indices.data();
    ibo_desc.size = indices.size() * sizeof(u32);
    chunk.ibo = rhi_->create_buffer(ibo_desc);

    return chunk;
}

void TerrainRenderer::render(rhi::RHI* rhi, const Mat4& view_projection,
                               Vec3 camera_pos) {
    if (chunks_.empty()) return;

    rhi->bind_shader(shader_);
    rhi->bind_pipeline(pipeline_);
    rhi->set_uniform_mat4(shader_, "u_ViewProjection", view_projection);
    rhi->set_uniform_vec3(shader_, "u_SunDirection", Vec3(0.0f, -1.0f, -0.5f));
    rhi->set_uniform_vec3(shader_, "u_SunColor", Vec3(1.0f));

    (void)camera_pos; // Would be used for LOD selection in a full implementation

    for (const auto& chunk : chunks_) {
        rhi->bind_vertex_buffer(chunk.vbo);
        rhi->bind_index_buffer(chunk.ibo);
        rhi->draw_indexed(chunk.index_count);
    }
}

} // namespace nexus
