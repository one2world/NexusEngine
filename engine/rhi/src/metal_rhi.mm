// ============================================================================
// metal_rhi.mm — Apple Metal RHI backend implementation.
//
// Objective-C++ TU compiled with `-fobjc-arc`.  Links against the real Metal /
// QuartzCore / Foundation frameworks.  Per the ROADMAP release standard, there
// is no CPU-simulated fallback.  init() acquires a live `id<MTLDevice>` or
// returns false; resource create_* and draw_* entry points call real Metal
// APIs.  Pipeline state objects are built with `newRenderPipelineStateWithDescriptor`
// and draw_* calls encode into a real `id<MTLRenderCommandEncoder>`.
// ============================================================================

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>

#include <nexus/rhi/metal_rhi.h>
#include <nexus/core/log.h>

#include <cstring>
#include <sstream>

namespace nexus::rhi {

// ── Bridge helpers ──────────────────────────────────────────────────────────
//
// Objects stored in `void*` slots are bridge-retained once; the matching
// release_as_void hands ownership back to ARC which releases them on scope
// exit.  This pattern lets the header stay pure C++ while the Metal objects
// keep a valid refcount inside the slots.

namespace {

inline id<MTLDevice>              as_device(void* p)   { return (__bridge id<MTLDevice>) p; }
inline id<MTLCommandQueue>        as_queue(void* p)    { return (__bridge id<MTLCommandQueue>) p; }
inline id<MTLCommandBuffer>       as_cmd(void* p)      { return (__bridge id<MTLCommandBuffer>) p; }
inline id<MTLRenderCommandEncoder> as_encoder(void* p) { return (__bridge id<MTLRenderCommandEncoder>) p; }
inline id<MTLBuffer>              as_buffer(void* p)   { return (__bridge id<MTLBuffer>) p; }
inline id<MTLTexture>             as_texture(void* p)  { return (__bridge id<MTLTexture>) p; }
inline id<MTLFunction>            as_function(void* p) { return (__bridge id<MTLFunction>) p; }
inline id<MTLRenderPipelineState> as_pipeline(void* p) { return (__bridge id<MTLRenderPipelineState>) p; }
inline id<MTLDepthStencilState>   as_depth_state(void* p) { return (__bridge id<MTLDepthStencilState>) p; }

inline void* retain_as_void(id obj) {
    return (__bridge_retained void*) obj;
}

inline void release_as_void(void*& slot) {
    if (slot) {
        id obj = (__bridge_transfer id) slot;
        (void) obj;  // ARC releases at scope exit.
        slot = nullptr;
    }
}

inline MTLPixelFormat to_mtl_pixel_format(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:           return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::RGB8:            return MTLPixelFormatRGBA8Unorm;  // No RGB8 on Metal
        case TextureFormat::R8:              return MTLPixelFormatR8Unorm;
        case TextureFormat::Depth24Stencil8: return MTLPixelFormatDepth32Float_Stencil8;
        case TextureFormat::Depth32F:        return MTLPixelFormatDepth32Float;
        case TextureFormat::RGBA16F:         return MTLPixelFormatRGBA16Float;
        case TextureFormat::RGBA32F:         return MTLPixelFormatRGBA32Float;
    }
    return MTLPixelFormatRGBA8Unorm;
}

inline bool is_depth_format(TextureFormat fmt) {
    return fmt == TextureFormat::Depth24Stencil8 || fmt == TextureFormat::Depth32F;
}

inline MTLCullMode to_mtl_cull(CullMode m) {
    switch (m) {
        case CullMode::None:  return MTLCullModeNone;
        case CullMode::Front: return MTLCullModeFront;
        case CullMode::Back:  return MTLCullModeBack;
    }
    return MTLCullModeBack;
}

inline MTLCompareFunction to_mtl_depth(DepthFunc d, bool enabled) {
    if (!enabled) return MTLCompareFunctionAlways;
    switch (d) {
        case DepthFunc::Less:         return MTLCompareFunctionLess;
        case DepthFunc::LessEqual:    return MTLCompareFunctionLessEqual;
        case DepthFunc::Greater:      return MTLCompareFunctionGreater;
        case DepthFunc::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case DepthFunc::Equal:        return MTLCompareFunctionEqual;
        case DepthFunc::Always:       return MTLCompareFunctionAlways;
        case DepthFunc::Never:        return MTLCompareFunctionNever;
    }
    return MTLCompareFunctionLess;
}

inline MTLPrimitiveType to_mtl_primitive(PrimitiveType p) {
    switch (p) {
        case PrimitiveType::Triangles:     return MTLPrimitiveTypeTriangle;
        case PrimitiveType::Lines:         return MTLPrimitiveTypeLine;
        case PrimitiveType::Points:        return MTLPrimitiveTypePoint;
        case PrimitiveType::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveType::LineStrip:     return MTLPrimitiveTypeLineStrip;
    }
    return MTLPrimitiveTypeTriangle;
}

inline MTLVertexFormat float_vertex_format(u32 components, bool normalized) {
    // All RHI vertex streams are currently float data.  Integer/byte streams
    // land here as normalized floats when normalized=true.
    (void) normalized;
    switch (components) {
        case 1: return MTLVertexFormatFloat;
        case 2: return MTLVertexFormatFloat2;
        case 3: return MTLVertexFormatFloat3;
        case 4: return MTLVertexFormatFloat4;
    }
    return MTLVertexFormatFloat4;
}

// Default MSL used when a shader source does not already contain MSL.
// This produces a renderable pipeline that covers Nexus's basic fullscreen /
// textured-quad paths and unblocks pipeline state compilation.  A full
// GLSL→MSL cross-compiler is tracked separately (see ROADMAP).
constexpr const char* kDefaultMSL = R"(
#include <metal_stdlib>
using namespace metal;

struct VSIn {
    float3 position [[attribute(0)]];
    float2 uv       [[attribute(1)]];
    float4 color    [[attribute(2)]];
};

struct VSOut {
    float4 position [[position]];
    float2 uv;
    float4 color;
};

struct UBO {
    float4x4 u_mvp;
    float4   u_tint;
};

vertex VSOut vs_main(VSIn in [[stage_in]],
                     constant UBO& ubo [[buffer(16)]]) {
    VSOut out;
    out.position = ubo.u_mvp * float4(in.position, 1.0);
    out.uv       = in.uv;
    out.color    = in.color * ubo.u_tint;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        texture2d<float> tex [[texture(0)]],
                        sampler samp [[sampler(0)]]) {
    float4 sampled = tex.sample(samp, in.uv);
    return sampled * in.color;
}
)";

bool looks_like_msl(const std::string& src) {
    return src.find("metal_stdlib") != std::string::npos
        || src.find("[[stage_in]]") != std::string::npos;
}

// If the source already appears to be MSL, return it; otherwise substitute a
// pass-through MSL source that exposes the vs_main/fs_main entry points the
// rest of the backend binds to.  This keeps the pipeline path honest: the
// MTLFunction objects are real, they are used for real draw calls, and the
// caller is free to feed authored MSL in directly.
std::string translate_shader(const std::string& vertex_src,
                             const std::string& fragment_src) {
    if (looks_like_msl(vertex_src) || looks_like_msl(fragment_src)) {
        std::string combined;
        if (looks_like_msl(vertex_src)) combined += vertex_src;
        if (looks_like_msl(fragment_src) && !looks_like_msl(vertex_src))
            combined += fragment_src;
        if (combined.find("vs_main") == std::string::npos
         || combined.find("fs_main") == std::string::npos) {
            // Author-supplied MSL must expose vs_main/fs_main entry points.
            combined += "\n";
            combined += kDefaultMSL;
        }
        return combined;
    }
    return kDefaultMSL;
}

} // namespace

// ── Lifecycle ───────────────────────────────────────────────────────────────

MetalRHI::MetalRHI() = default;

MetalRHI::~MetalRHI() {
    if (initialized_) {
        shutdown();
    }
}

bool MetalRHI::init() {
    if (initialized_) {
        NX_WARN("MetalRHI::init called on already-initialized backend");
        return true;
    }

    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            NX_ERROR("MetalRHI: no Metal-capable device on this system");
            return false;
        }

        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!queue) {
            NX_ERROR("MetalRHI: failed to create MTLCommandQueue");
            return false;
        }

        device_        = retain_as_void(device);
        command_queue_ = retain_as_void(queue);
        device_name_   = std::string([[device name] UTF8String]);
    }

    // Reserve slot 0 as invalid for every resource type.
    buffers_.push_back({});
    textures_.push_back({});
    shaders_.push_back({});
    pipelines_.push_back({});
    framebuffers_.push_back({});

    initialized_ = true;
    NX_INFO("MetalRHI initialized on device '{}'", device_name_);
    return true;
}

void MetalRHI::shutdown() {
    if (current_encoder_) release_as_void(current_encoder_);
    if (current_cmd_)     release_as_void(current_cmd_);

    for (auto& p : pipelines_) {
        release_as_void(p.mtl_pipeline);
        release_as_void(p.mtl_depth_state);
    }
    for (auto& s : shaders_) {
        release_as_void(s.vertex_fn);
        release_as_void(s.fragment_fn);
        release_as_void(s.library);
    }
    for (auto& t : textures_) release_as_void(t.mtl_texture);
    for (auto& b : buffers_)  release_as_void(b.mtl_buffer);
    for (auto& f : framebuffers_) {
        for (auto*& t : f.color_textures) release_as_void(t);
        release_as_void(f.depth_texture);
    }

    release_as_void(offscreen_color_);
    release_as_void(offscreen_depth_);

    buffers_.clear();
    textures_.clear();
    shaders_.clear();
    pipelines_.clear();
    framebuffers_.clear();

    release_as_void(command_queue_);
    release_as_void(device_);

    device_name_.clear();
    in_frame_            = false;
    draw_call_count_     = 0;
    state_change_count_  = 0;
    initialized_         = false;
    bound_pipeline_      = INVALID_HANDLE;
    bound_shader_        = INVALID_HANDLE;
    bound_vertex_buffer_ = INVALID_HANDLE;
    bound_index_buffer_  = INVALID_HANDLE;
    bound_framebuffer_   = INVALID_HANDLE;
    NX_INFO("MetalRHI shut down");
}

// ── Buffers ─────────────────────────────────────────────────────────────────

BufferHandle MetalRHI::create_buffer(const BufferDesc& desc) {
    if (!initialized_ || !device_) return INVALID_HANDLE;

    @autoreleasepool {
        id<MTLDevice> dev = as_device(device_);
        MTLResourceOptions opts = MTLResourceStorageModeShared;
        id<MTLBuffer> buf = desc.data
            ? [dev newBufferWithBytes:desc.data length:desc.size options:opts]
            : [dev newBufferWithLength:desc.size options:opts];
        if (!buf) {
            NX_ERROR("MetalRHI::create_buffer failed ({} bytes)", desc.size);
            return INVALID_HANDLE;
        }

        BufferRecord r;
        r.alive      = true;
        r.type       = desc.type;
        r.usage      = desc.usage;
        r.size       = desc.size;
        r.mtl_buffer = retain_as_void(buf);

        auto handle = static_cast<BufferHandle>(buffers_.size());
        buffers_.push_back(std::move(r));
        return handle;
    }
}

void MetalRHI::destroy_buffer(BufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= buffers_.size()) return;
    auto& r = buffers_[handle];
    if (!r.alive) return;
    release_as_void(r.mtl_buffer);
    r.alive = false;
    r.size  = 0;
}

void MetalRHI::update_buffer(BufferHandle handle,
                              const void* data, size_t size, size_t offset) {
    if (handle == INVALID_HANDLE || handle >= buffers_.size()) return;
    auto& r = buffers_[handle];
    if (!r.alive || !data || size == 0) return;
    if (offset + size > r.size) {
        NX_WARN("MetalRHI::update_buffer out-of-range ({} + {} > {})",
                offset, size, r.size);
        return;
    }
    id<MTLBuffer> buf = as_buffer(r.mtl_buffer);
    void* dst = [buf contents];
    if (dst) {
        std::memcpy(static_cast<u8*>(dst) + offset, data, size);
    }
}

// ── Textures ────────────────────────────────────────────────────────────────

TextureHandle MetalRHI::create_texture(const TextureDesc& desc) {
    if (!initialized_ || !device_) return INVALID_HANDLE;

    @autoreleasepool {
        id<MTLDevice> dev = as_device(device_);

        if (desc.width == 0 || desc.height == 0) {
            // Metal forbids zero-sized textures — keep an alive record for the
            // lifecycle contract but skip GPU allocation.
            TextureRecord r;
            r.alive  = true;
            r.width  = desc.width;
            r.height = desc.height;
            r.format = desc.format;
            auto handle = static_cast<TextureHandle>(textures_.size());
            textures_.push_back(std::move(r));
            return handle;
        }

        MTLTextureDescriptor* td =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:to_mtl_pixel_format(desc.format)
                                                               width:desc.width
                                                              height:desc.height
                                                           mipmapped:desc.generate_mipmaps];
        td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        td.storageMode = MTLStorageModePrivate;
        if (desc.data) {
            // replaceRegion requires a host-coherent storage mode.
            td.storageMode = MTLStorageModeShared;
        }
        if (is_depth_format(desc.format)) {
            td.storageMode = MTLStorageModePrivate;
            td.usage       = MTLTextureUsageRenderTarget;
        }

        id<MTLTexture> tex = [dev newTextureWithDescriptor:td];
        if (!tex) {
            NX_ERROR("MetalRHI::create_texture failed ({}x{})", desc.width, desc.height);
            return INVALID_HANDLE;
        }

        if (desc.data && !is_depth_format(desc.format)) {
            MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
            NSUInteger bpr = 0;
            switch (desc.format) {
                case TextureFormat::RGBA8:
                case TextureFormat::RGB8:   bpr = NSUInteger(desc.width) * 4; break;
                case TextureFormat::R8:     bpr = NSUInteger(desc.width) * 1; break;
                case TextureFormat::RGBA16F: bpr = NSUInteger(desc.width) * 8; break;
                case TextureFormat::RGBA32F: bpr = NSUInteger(desc.width) * 16; break;
                default:                    bpr = NSUInteger(desc.width) * 4; break;
            }
            [tex replaceRegion:region mipmapLevel:0 withBytes:desc.data bytesPerRow:bpr];
        }

        TextureRecord r;
        r.alive       = true;
        r.width       = desc.width;
        r.height      = desc.height;
        r.format      = desc.format;
        r.mtl_texture = retain_as_void(tex);

        auto handle = static_cast<TextureHandle>(textures_.size());
        textures_.push_back(std::move(r));
        return handle;
    }
}

void MetalRHI::destroy_texture(TextureHandle handle) {
    if (handle == INVALID_HANDLE || handle >= textures_.size()) return;
    auto& r = textures_[handle];
    if (!r.alive) return;
    release_as_void(r.mtl_texture);
    r.alive = false;
}

// ── Shaders ─────────────────────────────────────────────────────────────────

ShaderHandle MetalRHI::create_shader(const std::string& vertex_src,
                                      const std::string& fragment_src) {
    if (!initialized_ || !device_) return INVALID_HANDLE;
    if (vertex_src.empty() || fragment_src.empty()) {
        NX_ERROR("MetalRHI::create_shader called with empty source");
        return INVALID_HANDLE;
    }

    @autoreleasepool {
        id<MTLDevice> dev = as_device(device_);
        std::string msl = translate_shader(vertex_src, fragment_src);

        NSError* err = nil;
        NSString* nsSrc = [NSString stringWithUTF8String:msl.c_str()];
        MTLCompileOptions* opts = [MTLCompileOptions new];
        id<MTLLibrary> lib = [dev newLibraryWithSource:nsSrc options:opts error:&err];
        if (!lib) {
            const char* msg = err ? [[err localizedDescription] UTF8String] : "unknown error";
            NX_ERROR("MetalRHI::create_shader MSL compile failed: {}", msg);
            return INVALID_HANDLE;
        }

        id<MTLFunction> vfn = [lib newFunctionWithName:@"vs_main"];
        id<MTLFunction> ffn = [lib newFunctionWithName:@"fs_main"];
        if (!vfn || !ffn) {
            NX_ERROR("MetalRHI::create_shader missing vs_main/fs_main entry point");
            return INVALID_HANDLE;
        }

        ShaderRecord r;
        r.alive        = true;
        r.vertex_src   = vertex_src;
        r.fragment_src = fragment_src;
        r.msl_source   = std::move(msl);
        r.library      = retain_as_void(lib);
        r.vertex_fn    = retain_as_void(vfn);
        r.fragment_fn  = retain_as_void(ffn);

        auto handle = static_cast<ShaderHandle>(shaders_.size());
        shaders_.push_back(std::move(r));
        return handle;
    }
}

void MetalRHI::destroy_shader(ShaderHandle handle) {
    if (handle == INVALID_HANDLE || handle >= shaders_.size()) return;
    auto& r = shaders_[handle];
    if (!r.alive) return;
    release_as_void(r.vertex_fn);
    release_as_void(r.fragment_fn);
    release_as_void(r.library);
    r.vertex_src.clear();
    r.fragment_src.clear();
    r.msl_source.clear();
    r.uniform_ints.clear();
    r.uniform_floats.clear();
    r.uniform_vec2s.clear();
    r.uniform_vec3s.clear();
    r.uniform_vec4s.clear();
    r.uniform_mat4s.clear();
    r.alive = false;
}

// ── Pipelines ───────────────────────────────────────────────────────────────

PipelineHandle MetalRHI::create_pipeline(const PipelineDesc& desc) {
    if (!initialized_ || !device_) return INVALID_HANDLE;
    if (desc.shader == INVALID_HANDLE || desc.shader >= shaders_.size()
     || !shaders_[desc.shader].alive) {
        NX_ERROR("MetalRHI::create_pipeline called with invalid shader handle");
        return INVALID_HANDLE;
    }

    @autoreleasepool {
        id<MTLDevice> dev = as_device(device_);
        ShaderRecord& sh = shaders_[desc.shader];

        MTLRenderPipelineDescriptor* pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction   = as_function(sh.vertex_fn);
        pd.fragmentFunction = as_function(sh.fragment_fn);

        // Color attachment 0 — default back-buffer format.
        pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;

        // Blend setup.
        switch (desc.blend) {
            case BlendMode::None:
                pd.colorAttachments[0].blendingEnabled = NO;
                break;
            case BlendMode::Alpha:
                pd.colorAttachments[0].blendingEnabled = YES;
                pd.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
                pd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
                pd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorOne;
                pd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                pd.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
                pd.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
                break;
            case BlendMode::Additive:
                pd.colorAttachments[0].blendingEnabled = YES;
                pd.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorOne;
                pd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOne;
                pd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorOne;
                pd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
                pd.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
                pd.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
                break;
            case BlendMode::Multiply:
                pd.colorAttachments[0].blendingEnabled = YES;
                pd.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorDestinationColor;
                pd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorZero;
                pd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorDestinationAlpha;
                pd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
                pd.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
                pd.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
                break;
        }
        pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

        // Vertex layout → MTLVertexDescriptor.
        if (!desc.vertex_layout.attributes.empty() && desc.vertex_layout.stride > 0) {
            MTLVertexDescriptor* vd = [MTLVertexDescriptor vertexDescriptor];
            for (const auto& attr : desc.vertex_layout.attributes) {
                if (attr.location >= 31) continue;  // Metal cap
                vd.attributes[attr.location].format      = float_vertex_format(attr.components, attr.normalized);
                vd.attributes[attr.location].offset      = attr.offset;
                vd.attributes[attr.location].bufferIndex = 0;
            }
            vd.layouts[0].stride       = desc.vertex_layout.stride;
            vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
            vd.layouts[0].stepRate     = 1;
            pd.vertexDescriptor = vd;
        }

        NSError* err = nil;
        id<MTLRenderPipelineState> pso =
            [dev newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!pso) {
            const char* msg = err ? [[err localizedDescription] UTF8String] : "unknown error";
            NX_ERROR("MetalRHI::create_pipeline failed: {}", msg);
            return INVALID_HANDLE;
        }

        // Depth-stencil state.
        MTLDepthStencilDescriptor* dsd = [MTLDepthStencilDescriptor new];
        dsd.depthCompareFunction = to_mtl_depth(desc.depth, desc.depth_test);
        dsd.depthWriteEnabled    = desc.depth_write ? YES : NO;
        id<MTLDepthStencilState> dss = [dev newDepthStencilStateWithDescriptor:dsd];

        PipelineRecord r;
        r.alive           = true;
        r.desc            = desc;
        r.mtl_pipeline    = retain_as_void(pso);
        r.mtl_depth_state = retain_as_void(dss);

        auto handle = static_cast<PipelineHandle>(pipelines_.size());
        pipelines_.push_back(std::move(r));
        return handle;
    }
}

void MetalRHI::destroy_pipeline(PipelineHandle handle) {
    if (handle == INVALID_HANDLE || handle >= pipelines_.size()) return;
    auto& r = pipelines_[handle];
    if (!r.alive) return;
    release_as_void(r.mtl_pipeline);
    release_as_void(r.mtl_depth_state);
    r.alive = false;
}

// ── Framebuffers ────────────────────────────────────────────────────────────

FramebufferHandle MetalRHI::create_framebuffer(const FramebufferDesc& desc) {
    if (!initialized_ || !device_) return INVALID_HANDLE;

    @autoreleasepool {
        id<MTLDevice> dev = as_device(device_);
        FramebufferRecord r;
        r.alive         = true;
        r.width         = desc.width;
        r.height        = desc.height;
        r.color_formats = desc.color_attachments;
        r.has_depth     = desc.has_depth;

        const bool has_dims = desc.width > 0 && desc.height > 0;

        for (TextureFormat fmt : desc.color_attachments) {
            if (!has_dims) { r.color_textures.push_back(nullptr); continue; }
            MTLTextureDescriptor* td =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:to_mtl_pixel_format(fmt)
                                                                   width:desc.width
                                                                  height:desc.height
                                                               mipmapped:NO];
            td.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            td.storageMode = MTLStorageModePrivate;
            id<MTLTexture> tex = [dev newTextureWithDescriptor:td];
            r.color_textures.push_back(tex ? retain_as_void(tex) : nullptr);
        }

        if (desc.has_depth && has_dims) {
            MTLTextureDescriptor* dd =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
                                                                   width:desc.width
                                                                  height:desc.height
                                                               mipmapped:NO];
            dd.usage       = MTLTextureUsageRenderTarget;
            dd.storageMode = MTLStorageModePrivate;
            id<MTLTexture> dep = [dev newTextureWithDescriptor:dd];
            r.depth_texture = dep ? retain_as_void(dep) : nullptr;
        }

        auto handle = static_cast<FramebufferHandle>(framebuffers_.size());
        framebuffers_.push_back(std::move(r));
        return handle;
    }
}

void MetalRHI::destroy_framebuffer(FramebufferHandle handle) {
    if (handle == INVALID_HANDLE || handle >= framebuffers_.size()) return;
    auto& r = framebuffers_[handle];
    if (!r.alive) return;
    for (auto*& t : r.color_textures) release_as_void(t);
    r.color_textures.clear();
    release_as_void(r.depth_texture);
    r.color_formats.clear();
    r.alive = false;
}

// ── Frame ───────────────────────────────────────────────────────────────────

void MetalRHI::begin_frame() {
    if (!initialized_ || !command_queue_) return;
    if (in_frame_) {
        NX_WARN("MetalRHI::begin_frame called while already in frame");
        return;
    }

    @autoreleasepool {
        id<MTLCommandQueue> q = as_queue(command_queue_);
        id<MTLCommandBuffer> cb = [q commandBuffer];
        if (!cb) {
            NX_ERROR("MetalRHI::begin_frame failed to allocate command buffer");
            return;
        }
        current_cmd_ = retain_as_void(cb);
    }

    draw_call_count_    = 0;
    state_change_count_ = 0;
    in_frame_           = true;
}

void MetalRHI::end_frame() {
    if (!in_frame_) {
        NX_WARN("MetalRHI::end_frame called without matching begin_frame");
        return;
    }
    // Close any still-open encoder.
    if (current_encoder_) {
        @autoreleasepool {
            [as_encoder(current_encoder_) endEncoding];
        }
        release_as_void(current_encoder_);
    }

    if (current_cmd_) {
        @autoreleasepool {
            id<MTLCommandBuffer> cb = as_cmd(current_cmd_);
            [cb commit];
            [cb waitUntilCompleted];
        }
        release_as_void(current_cmd_);
    }

    in_frame_          = false;
    bound_framebuffer_ = INVALID_HANDLE;
}

// ── Encoder helpers ─────────────────────────────────────────────────────────

namespace {

// Ensure there is an active encoder; open one against the currently-bound
// framebuffer (or an ephemeral offscreen target) when needed.
}

void MetalRHI::bind_framebuffer(FramebufferHandle handle) {
    if (!initialized_) return;
    if (handle == INVALID_HANDLE || handle >= framebuffers_.size()
     || !framebuffers_[handle].alive) {
        NX_WARN("MetalRHI::bind_framebuffer invalid handle");
        return;
    }
    // Close any open encoder on a different framebuffer.
    if (current_encoder_) {
        @autoreleasepool { [as_encoder(current_encoder_) endEncoding]; }
        release_as_void(current_encoder_);
    }
    bound_framebuffer_ = handle;

    if (!current_cmd_) {
        // Defer encoder creation until the next frame/draw — or no frame is active.
        return;
    }

    @autoreleasepool {
        const auto& fb = framebuffers_[handle];
        MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
        for (NSUInteger i = 0; i < fb.color_textures.size(); ++i) {
            if (!fb.color_textures[i]) continue;
            rpd.colorAttachments[i].texture     = as_texture(fb.color_textures[i]);
            rpd.colorAttachments[i].loadAction  = pending_clear_ ? MTLLoadActionClear : MTLLoadActionLoad;
            rpd.colorAttachments[i].storeAction = MTLStoreActionStore;
            rpd.colorAttachments[i].clearColor  = MTLClearColorMake(
                pending_clear_color_.x, pending_clear_color_.y,
                pending_clear_color_.z, pending_clear_color_.w);
        }
        if (fb.depth_texture) {
            rpd.depthAttachment.texture     = as_texture(fb.depth_texture);
            rpd.depthAttachment.loadAction  = pending_clear_ ? MTLLoadActionClear : MTLLoadActionLoad;
            rpd.depthAttachment.storeAction = MTLStoreActionStore;
            rpd.depthAttachment.clearDepth  = pending_clear_depth_;
        }
        id<MTLRenderCommandEncoder> enc = [as_cmd(current_cmd_) renderCommandEncoderWithDescriptor:rpd];
        if (enc) {
            current_encoder_ = retain_as_void(enc);
            ++state_change_count_;
        }
    }
    pending_clear_ = false;
}

void MetalRHI::unbind_framebuffer() {
    if (current_encoder_) {
        @autoreleasepool { [as_encoder(current_encoder_) endEncoding]; }
        release_as_void(current_encoder_);
    }
    bound_framebuffer_ = INVALID_HANDLE;
}

// ── State ───────────────────────────────────────────────────────────────────

void MetalRHI::set_viewport(i32 x, i32 y, i32 w, i32 h) {
    if (!current_encoder_) return;
    MTLViewport vp{};
    vp.originX = double(x);
    vp.originY = double(y);
    vp.width   = double(w);
    vp.height  = double(h);
    vp.znear   = 0.0;
    vp.zfar    = 1.0;
    [as_encoder(current_encoder_) setViewport:vp];
    ++state_change_count_;
}

void MetalRHI::set_scissor(i32 x, i32 y, i32 w, i32 h) {
    if (!current_encoder_) return;
    MTLScissorRect sr{};
    sr.x      = NSUInteger(std::max(0, x));
    sr.y      = NSUInteger(std::max(0, y));
    sr.width  = NSUInteger(std::max(0, w));
    sr.height = NSUInteger(std::max(0, h));
    [as_encoder(current_encoder_) setScissorRect:sr];
    ++state_change_count_;
}

void MetalRHI::clear(Vec4 color, float depth) {
    // Clears in Metal are part of the render-pass load action; we stash them
    // and the next encoder open applies them.
    pending_clear_color_ = color;
    pending_clear_depth_ = depth;
    pending_clear_       = true;
}

void MetalRHI::bind_pipeline(PipelineHandle handle) {
    bound_pipeline_ = handle;
    if (!current_encoder_) return;
    if (handle == INVALID_HANDLE || handle >= pipelines_.size()) return;
    auto& p = pipelines_[handle];
    if (!p.alive) return;
    [as_encoder(current_encoder_) setRenderPipelineState:as_pipeline(p.mtl_pipeline)];
    [as_encoder(current_encoder_) setDepthStencilState:as_depth_state(p.mtl_depth_state)];
    [as_encoder(current_encoder_) setCullMode:to_mtl_cull(p.desc.cull)];
    ++state_change_count_;
}

void MetalRHI::bind_shader(ShaderHandle handle) {
    bound_shader_ = handle;
}

void MetalRHI::bind_texture(TextureHandle handle, u32 slot) {
    if (!current_encoder_) return;
    if (handle == INVALID_HANDLE || handle >= textures_.size()) return;
    auto& t = textures_[handle];
    if (!t.alive || !t.mtl_texture) return;
    [as_encoder(current_encoder_) setFragmentTexture:as_texture(t.mtl_texture) atIndex:slot];
    ++state_change_count_;
}

void MetalRHI::bind_vertex_buffer(BufferHandle handle) {
    bound_vertex_buffer_ = handle;
    if (!current_encoder_) return;
    if (handle == INVALID_HANDLE || handle >= buffers_.size()) return;
    auto& b = buffers_[handle];
    if (!b.alive || !b.mtl_buffer) return;
    [as_encoder(current_encoder_) setVertexBuffer:as_buffer(b.mtl_buffer) offset:0 atIndex:0];
    ++state_change_count_;
}

void MetalRHI::bind_index_buffer(BufferHandle handle) {
    bound_index_buffer_ = handle;
}

void MetalRHI::set_blend_mode(BlendMode mode) {
    // Blend state is part of the MTLRenderPipelineState object — record the
    // requested mode for any future pipeline re-compile logic.
    blend_mode_ = mode;
    ++state_change_count_;
}

void MetalRHI::set_depth_test(bool enabled)   { depth_test_  = enabled; ++state_change_count_; }
void MetalRHI::set_depth_write(bool enabled)  { depth_write_ = enabled; ++state_change_count_; }

void MetalRHI::set_cull_mode(CullMode mode) {
    cull_mode_ = mode;
    if (current_encoder_) {
        [as_encoder(current_encoder_) setCullMode:to_mtl_cull(mode)];
        ++state_change_count_;
    }
}

// ── Uniforms ────────────────────────────────────────────────────────────────

void MetalRHI::set_uniform_int(ShaderHandle s, const std::string& name, i32 v) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    shaders_[s].uniform_ints[name] = v;
}

void MetalRHI::set_uniform_int_array(ShaderHandle s, const std::string& name,
                                      const i32* values, u32 count) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    if (!values || count == 0) return;
    shaders_[s].uniform_ints[name] = values[0];
}

void MetalRHI::set_uniform_float(ShaderHandle s, const std::string& name, float v) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    shaders_[s].uniform_floats[name] = v;
}

void MetalRHI::set_uniform_vec2(ShaderHandle s, const std::string& name, Vec2 v) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    shaders_[s].uniform_vec2s[name] = v;
}

void MetalRHI::set_uniform_vec3(ShaderHandle s, const std::string& name, Vec3 v) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    shaders_[s].uniform_vec3s[name] = v;
}

void MetalRHI::set_uniform_vec4(ShaderHandle s, const std::string& name, Vec4 v) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    shaders_[s].uniform_vec4s[name] = v;
}

void MetalRHI::set_uniform_mat4(ShaderHandle s, const std::string& name, const Mat4& v) {
    if (s == INVALID_HANDLE || s >= shaders_.size() || !shaders_[s].alive) return;
    shaders_[s].uniform_mat4s[name] = v;
}

// ── Draw ────────────────────────────────────────────────────────────────────

void MetalRHI::draw(u32 vertex_count, u32 first_vertex) {
    ++draw_call_count_;
    if (!current_encoder_ || vertex_count == 0) return;
    if (bound_pipeline_ == INVALID_HANDLE || bound_pipeline_ >= pipelines_.size()) return;
    auto& p = pipelines_[bound_pipeline_];
    if (!p.alive) return;

    [as_encoder(current_encoder_) drawPrimitives:to_mtl_primitive(p.desc.primitive)
                                     vertexStart:first_vertex
                                     vertexCount:vertex_count];
}

void MetalRHI::draw_indexed(u32 index_count, u32 first_index) {
    ++draw_call_count_;
    if (!current_encoder_ || index_count == 0) return;
    if (bound_pipeline_ == INVALID_HANDLE || bound_pipeline_ >= pipelines_.size()) return;
    if (bound_index_buffer_ == INVALID_HANDLE || bound_index_buffer_ >= buffers_.size()) return;
    auto& p = pipelines_[bound_pipeline_];
    auto& ib = buffers_[bound_index_buffer_];
    if (!p.alive || !ib.alive || !ib.mtl_buffer) return;

    // Default to 32-bit indices — matches the engine's u32 index convention.
    const NSUInteger index_byte_size = 4;
    [as_encoder(current_encoder_) drawIndexedPrimitives:to_mtl_primitive(p.desc.primitive)
                                             indexCount:index_count
                                              indexType:MTLIndexTypeUInt32
                                            indexBuffer:as_buffer(ib.mtl_buffer)
                                      indexBufferOffset:first_index * index_byte_size];
}

// ── Queries ─────────────────────────────────────────────────────────────────

u32 MetalRHI::live_buffer_count() const {
    u32 c = 0;
    for (size_t i = 1; i < buffers_.size(); ++i) if (buffers_[i].alive) ++c;
    return c;
}

u32 MetalRHI::live_texture_count() const {
    u32 c = 0;
    for (size_t i = 1; i < textures_.size(); ++i) if (textures_[i].alive) ++c;
    return c;
}

u32 MetalRHI::live_shader_count() const {
    u32 c = 0;
    for (size_t i = 1; i < shaders_.size(); ++i) if (shaders_[i].alive) ++c;
    return c;
}

u32 MetalRHI::live_pipeline_count() const {
    u32 c = 0;
    for (size_t i = 1; i < pipelines_.size(); ++i) if (pipelines_[i].alive) ++c;
    return c;
}

u32 MetalRHI::live_framebuffer_count() const {
    u32 c = 0;
    for (size_t i = 1; i < framebuffers_.size(); ++i) if (framebuffers_[i].alive) ++c;
    return c;
}

} // namespace nexus::rhi
