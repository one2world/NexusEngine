#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <vector>

namespace nexus::rhi {

using BufferHandle      = u32;
using TextureHandle     = u32;
using ShaderHandle      = u32;
using PipelineHandle    = u32;
using FramebufferHandle = u32;

constexpr u32 INVALID_HANDLE = ~u32(0);

enum class BufferType : u8 { Vertex, Index, Uniform, Storage };
enum class BufferUsage : u8 { Static, Dynamic, Stream };

enum class TextureFormat : u8 {
    RGBA8,
    RGB8,
    R8,
    Depth24Stencil8,
    Depth32F,
    RGBA16F,
    RGBA32F
};

enum class TextureFilter : u8 {
    Nearest,
    Linear,
    NearestMipmapLinear,
    LinearMipmapLinear
};

enum class TextureWrap : u8 { Repeat, ClampToEdge, MirroredRepeat };

enum class ShaderType : u8 { Vertex, Fragment, Geometry, Compute };

enum class PrimitiveType : u8 {
    Triangles,
    Lines,
    Points,
    TriangleStrip,
    LineStrip
};

enum class BlendMode : u8 { None, Alpha, Additive, Multiply };
enum class CullMode : u8 { None, Front, Back };

enum class DepthFunc : u8 {
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    Always,
    Never
};

struct BufferDesc {
    BufferType  type;
    BufferUsage usage = BufferUsage::Static;
    const void* data  = nullptr;
    size_t      size  = 0;
};

struct TextureDesc {
    u32           width  = 0;
    u32           height = 0;
    TextureFormat format     = TextureFormat::RGBA8;
    TextureFilter min_filter = TextureFilter::Linear;
    TextureFilter mag_filter = TextureFilter::Linear;
    TextureWrap   wrap_s     = TextureWrap::Repeat;
    TextureWrap   wrap_t     = TextureWrap::Repeat;
    bool          generate_mipmaps = true;
    const void*   data = nullptr;
};

struct VertexAttribute {
    u32  location;
    u32  components; // 1, 2, 3, or 4
    u32  offset;
    bool normalized = false;
};

struct VertexLayout {
    std::vector<VertexAttribute> attributes;
    u32 stride = 0;
};

struct PipelineDesc {
    ShaderHandle  shader = INVALID_HANDLE;
    VertexLayout  vertex_layout;
    BlendMode     blend       = BlendMode::None;
    CullMode      cull        = CullMode::Back;
    DepthFunc     depth       = DepthFunc::Less;
    bool          depth_write = true;
    bool          depth_test  = true;
    PrimitiveType primitive   = PrimitiveType::Triangles;
};

struct FramebufferDesc {
    u32 width  = 0;
    u32 height = 0;
    std::vector<TextureFormat> color_attachments;
    bool has_depth = true;
};

} // namespace nexus::rhi
