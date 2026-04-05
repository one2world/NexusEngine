#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi.h"
#include <vector>
#include <string>

namespace nexus {

/// Mobile rendering quality tiers.
enum class MobileQuality : u8 {
    Low,
    Medium,
    High
};

/// Configuration for the mobile renderer.
struct MobileRendererConfig {
    MobileQuality quality{MobileQuality::Medium};
    u32 render_width{1280};
    u32 render_height{720};
    bool enable_shadows{true};
    bool enable_bloom{false};
    u32 shadow_resolution{512};
    u32 max_lights{4};
    f32 resolution_scale{1.0f};
};

/// GLES3-compatible shader source strings.
namespace mobile_shaders {
    extern const char* MOBILE_VERT;
    extern const char* MOBILE_FRAG;
    extern const char* MOBILE_SHADOW_VERT;
    extern const char* MOBILE_SHADOW_FRAG;
    extern const char* MOBILE_SKYBOX_VERT;
    extern const char* MOBILE_SKYBOX_FRAG;
}

/// MobileRenderer — simplified forward renderer for mobile/GLES3 targets.
class MobileRenderer {
public:
    MobileRenderer() = default;

    void init(rhi::RHI* rhi, const MobileRendererConfig& config);
    void shutdown();

    void set_config(const MobileRendererConfig& config) { config_ = config; }
    const MobileRendererConfig& config() const { return config_; }

    void begin_frame(const Mat4& view, const Mat4& projection);

    struct RenderItem {
        rhi::BufferHandle vbo{rhi::INVALID_HANDLE};
        rhi::BufferHandle ibo{rhi::INVALID_HANDLE};
        u32 index_count{0};
        Mat4 model{1.0f};
        Vec4 base_color{1.0f};
        f32 metallic{0.0f};
        f32 roughness{0.5f};
        rhi::TextureHandle albedo_texture{rhi::INVALID_HANDLE};
    };

    void submit(const RenderItem& item);

    struct LightData {
        Vec3 position{0.0f};
        Vec3 color{1.0f};
        f32 intensity{1.0f};
        f32 radius{10.0f};
        bool is_directional{false};
        Vec3 direction{0.0f, -1.0f, 0.0f};
    };

    void set_ambient(Vec3 color, f32 intensity);
    void add_light(const LightData& light);

    void end_frame();

    u32 draw_calls() const { return draw_calls_; }
    u32 triangle_count() const { return triangle_count_; }

private:
    void apply_quality_preset();

    rhi::RHI* rhi_{nullptr};
    MobileRendererConfig config_;

    rhi::ShaderHandle main_shader_{rhi::INVALID_HANDLE};
    rhi::PipelineHandle main_pipeline_{rhi::INVALID_HANDLE};

    Mat4 view_{1.0f};
    Mat4 projection_{1.0f};
    Vec3 ambient_color_{0.1f};
    f32 ambient_intensity_{0.3f};

    std::vector<RenderItem> render_queue_;
    std::vector<LightData> lights_;

    u32 draw_calls_{0};
    u32 triangle_count_{0};
};

} // namespace nexus
