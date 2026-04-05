#pragma once

#include "nexus/assets/asset_loader.h"
#include <string>
#include <vector>

namespace nexus::assets {

// ============================================================================
// GltfImporter — full glTF 2.0 JSON + GLB importer
//
// Supports: multi-primitive meshes, PBR materials (metallic-roughness),
//           skeleton joints/weights, animation channels (T/R/S).
// ============================================================================

/// Parsed glTF buffer view (a slice of a buffer).
struct GltfBufferView {
    u32 buffer{0};
    u32 byte_offset{0};
    u32 byte_length{0};
    u32 byte_stride{0};
};

/// Parsed glTF accessor (typed view into a buffer view).
struct GltfAccessor {
    u32 buffer_view{0};
    u32 byte_offset{0};
    u32 count{0};
    u32 component_type{0};  // 5120=byte, 5121=ubyte, 5122=short, 5123=ushort, 5125=uint, 5126=float
    std::string type;        // "SCALAR", "VEC2", "VEC3", "VEC4", "MAT4"
    float min_vals[4]{};
    float max_vals[4]{};
};

/// Parsed glTF primitive.
struct GltfPrimitive {
    i32 position_accessor{-1};
    i32 normal_accessor{-1};
    i32 texcoord_accessor{-1};
    i32 tangent_accessor{-1};
    i32 joints_accessor{-1};
    i32 weights_accessor{-1};
    i32 indices_accessor{-1};
    i32 material{-1};
};

/// Parsed glTF mesh.
struct GltfMesh {
    std::string name;
    std::vector<GltfPrimitive> primitives;
};

/// Parsed glTF material.
struct GltfMaterial {
    std::string name;
    float base_color[4]{1, 1, 1, 1};
    float metallic{0.0f};
    float roughness{1.0f};
    i32 base_color_texture{-1};
    i32 normal_texture{-1};
    i32 metallic_roughness_texture{-1};
    bool double_sided{false};
};

/// Parsed glTF node.
struct GltfNode {
    std::string name;
    i32 mesh{-1};
    i32 skin{-1};
    std::vector<u32> children;
    float translation[3]{0, 0, 0};
    float rotation[4]{0, 0, 0, 1};  // xyzw quaternion
    float scale[3]{1, 1, 1};
};

/// Parsed glTF skin (skeleton).
struct GltfSkin {
    std::string name;
    std::vector<u32> joints;
    i32 inverse_bind_matrices{-1};  // accessor index
    i32 skeleton_root{-1};
};

/// Parsed glTF animation channel target.
struct GltfAnimChannel {
    u32 node{0};
    std::string path;        // "translation", "rotation", "scale"
    i32 sampler{-1};
};

/// Parsed glTF animation sampler.
struct GltfAnimSampler {
    i32 input{-1};           // accessor for timestamps
    i32 output{-1};          // accessor for values
    std::string interpolation;  // "LINEAR", "STEP", "CUBICSPLINE"
};

/// Parsed glTF animation.
struct GltfAnimation {
    std::string name;
    std::vector<GltfAnimChannel> channels;
    std::vector<GltfAnimSampler> samplers;
};

/// Complete parsed glTF scene.
struct GltfScene {
    std::vector<std::vector<u8>> buffers;
    std::vector<GltfBufferView> buffer_views;
    std::vector<GltfAccessor> accessors;
    std::vector<GltfMesh> meshes;
    std::vector<GltfMaterial> materials;
    std::vector<GltfNode> nodes;
    std::vector<GltfSkin> skins;
    std::vector<GltfAnimation> animations;
    std::vector<std::string> images;  // image URIs/paths
};

/// Parse a glTF JSON string (+ optional GLB binary buffer) into a GltfScene.
bool parse_gltf(const std::string& json_str,
                const std::vector<u8>& glb_bin,
                const std::string& base_dir,
                GltfScene& out);

/// Read accessor data as floats (handles component type conversion).
std::vector<float> read_accessor_floats(const GltfScene& scene, u32 accessor_index);

/// Read accessor data as u32 indices.
std::vector<u32> read_accessor_indices(const GltfScene& scene, u32 accessor_index);

/// Convert a GltfMesh primitive into engine MeshData.
std::shared_ptr<MeshData> gltf_primitive_to_mesh(const GltfScene& scene,
                                                   const GltfPrimitive& prim,
                                                   const std::string& name);

/// Convert a GltfMaterial into engine MaterialData.
std::shared_ptr<MaterialData> gltf_material_to_material(const GltfScene& scene,
                                                          const GltfMaterial& mat);

/// Convert a GltfAnimation into engine AnimationData.
std::shared_ptr<AnimationData> gltf_animation_to_anim(const GltfScene& scene,
                                                        const GltfAnimation& anim);

} // namespace nexus::assets
