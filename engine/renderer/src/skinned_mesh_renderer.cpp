#include "nexus/renderer/skinned_mesh_renderer.h"
#include "nexus/core/log.h"

namespace nexus {

// ── Skinning shaders ───────────────────────────────────────────────────────

static const char* SKIN_VERT_SRC = R"(
#version 330 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec4 a_Tangent;
layout (location = 4) in ivec4 a_BoneIndices;
layout (location = 5) in vec4 a_BoneWeights;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat4 u_Bones[128];
uniform int  u_BoneCount;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    // Compute skinned position and normal
    mat4 skin_matrix = mat4(0.0);
    for (int i = 0; i < 4; i++) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (idx >= 0 && idx < u_BoneCount && w > 0.0) {
            skin_matrix += u_Bones[idx] * w;
        }
    }

    // If no bones influence this vertex, use identity
    float total_weight = a_BoneWeights.x + a_BoneWeights.y + a_BoneWeights.z + a_BoneWeights.w;
    if (total_weight < 0.001) {
        skin_matrix = mat4(1.0);
    }

    vec4 skinned_pos = skin_matrix * vec4(a_Position, 1.0);
    vec4 skinned_normal = skin_matrix * vec4(a_Normal, 0.0);

    vec4 world_pos = u_Model * skinned_pos;
    v_WorldPos = world_pos.xyz;
    v_Normal = normalize(mat3(u_Model) * skinned_normal.xyz);
    v_TexCoord = a_TexCoord;

    gl_Position = u_ViewProjection * world_pos;
}
)";

static const char* SKIN_FRAG_SRC = R"(
#version 330 core
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

out vec4 FragColor;

uniform sampler2D u_Diffuse;
uniform vec4 u_Color;
uniform int u_HasTexture;

void main() {
    vec3 normal = normalize(v_Normal);

    // Simple directional light for visualization
    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    float ndl = max(dot(normal, light_dir), 0.0);
    float lighting = 0.3 + 0.7 * ndl; // ambient + diffuse

    vec4 base_color = u_Color;
    if (u_HasTexture != 0) {
        base_color *= texture(u_Diffuse, v_TexCoord);
    }

    FragColor = vec4(base_color.rgb * lighting, base_color.a);
}
)";

// ── SkinnedMeshRenderer implementation ─────────────────────────────────────

void SkinnedMeshRenderer::init(rhi::RHI* rhi) {
    rhi_ = rhi;
    if (!rhi_) return;

    shader_ = rhi_->create_shader(SKIN_VERT_SRC, SKIN_FRAG_SRC);

    rhi::PipelineDesc desc;
    desc.shader = shader_;
    desc.blend = rhi::BlendMode::Alpha;
    desc.depth_test = true;
    desc.depth_write = true;
    desc.cull = rhi::CullMode::Back;
    desc.primitive = rhi::PrimitiveType::Triangles;
    desc.vertex_layout.stride = sizeof(SkinnedVertex);
    desc.vertex_layout.attributes = {
        {0, 3, 0, false},                                        // position
        {1, 3, static_cast<u32>(offsetof(SkinnedVertex, normal)), false},     // normal
        {2, 2, static_cast<u32>(offsetof(SkinnedVertex, texcoord)), false},   // texcoord
        {3, 4, static_cast<u32>(offsetof(SkinnedVertex, tangent)), false},    // tangent
        {4, 4, static_cast<u32>(offsetof(SkinnedVertex, bone_indices)), true},// bone indices (int)
        {5, 4, static_cast<u32>(offsetof(SkinnedVertex, bone_weights)), false},// bone weights
    };
    pipeline_ = rhi_->create_pipeline(desc);

    NX_INFO("SkinnedMeshRenderer initialized");
}

void SkinnedMeshRenderer::shutdown() {
    if (!rhi_) return;
    for (auto& mesh : meshes_) {
        if (mesh.vbo != rhi::INVALID_HANDLE) rhi_->destroy_buffer(mesh.vbo);
        if (mesh.ibo != rhi::INVALID_HANDLE) rhi_->destroy_buffer(mesh.ibo);
    }
    meshes_.clear();
    if (pipeline_ != rhi::INVALID_HANDLE) rhi_->destroy_pipeline(pipeline_);
    if (shader_ != rhi::INVALID_HANDLE) rhi_->destroy_shader(shader_);
    rhi_ = nullptr;
}

u32 SkinnedMeshRenderer::upload_mesh(const std::vector<SkinnedVertex>& vertices,
                                      const std::vector<u32>& indices) {
    if (!rhi_) return 0;

    MeshGPU mesh;
    mesh.vertex_count = static_cast<u32>(vertices.size());
    mesh.index_count = static_cast<u32>(indices.size());

    rhi::BufferDesc vbo_desc;
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Static;
    vbo_desc.size = static_cast<u32>(vertices.size() * sizeof(SkinnedVertex));
    vbo_desc.data = vertices.data();
    mesh.vbo = rhi_->create_buffer(vbo_desc);

    if (!indices.empty()) {
        rhi::BufferDesc ibo_desc;
        ibo_desc.type = rhi::BufferType::Index;
        ibo_desc.usage = rhi::BufferUsage::Static;
        ibo_desc.size = static_cast<u32>(indices.size() * sizeof(u32));
        ibo_desc.data = indices.data();
        mesh.ibo = rhi_->create_buffer(ibo_desc);
    }

    auto handle = static_cast<u32>(meshes_.size());
    meshes_.push_back(mesh);
    return handle;
}

void SkinnedMeshRenderer::begin(const Mat4& view_projection) {
    view_projection_ = view_projection;
    if (rhi_) {
        rhi_->bind_pipeline(pipeline_);
    }
}

void SkinnedMeshRenderer::draw(u32 mesh_handle, const Mat4& model,
                                const Mat4* bone_matrices, u32 bone_count,
                                rhi::TextureHandle diffuse_texture,
                                Vec4 color) {
    if (!rhi_ || mesh_handle >= meshes_.size()) return;

    auto& mesh = meshes_[mesh_handle];

    rhi_->set_uniform_mat4(shader_, "u_ViewProjection", view_projection_);
    rhi_->set_uniform_mat4(shader_, "u_Model", model);
    rhi_->set_uniform_vec4(shader_, "u_Color", color);

    // Upload bone matrices
    u32 count = std::min(bone_count, MAX_BONES);
    rhi_->set_uniform_int(shader_, "u_BoneCount", static_cast<i32>(count));
    for (u32 i = 0; i < count; ++i) {
        std::string name = "u_Bones[" + std::to_string(i) + "]";
        rhi_->set_uniform_mat4(shader_, name.c_str(), bone_matrices[i]);
    }

    // Bind texture
    if (diffuse_texture != rhi::INVALID_HANDLE) {
        rhi_->bind_texture(diffuse_texture, 0);
        rhi_->set_uniform_int(shader_, "u_Diffuse", 0);
        rhi_->set_uniform_int(shader_, "u_HasTexture", 1);
    } else {
        rhi_->set_uniform_int(shader_, "u_HasTexture", 0);
    }

    rhi_->bind_vertex_buffer(mesh.vbo);
    if (mesh.ibo != rhi::INVALID_HANDLE) {
        rhi_->bind_index_buffer(mesh.ibo);
        rhi_->draw_indexed(mesh.index_count, 0);
    } else {
        rhi_->draw(mesh.vertex_count, 0);
    }
}

void SkinnedMeshRenderer::end() {
    // No batching needed — each draw is immediate
}

} // namespace nexus
