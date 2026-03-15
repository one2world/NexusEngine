#include <gtest/gtest.h>
#include "nexus/renderer/pbr_renderer.h"
#include "nexus/renderer/shadow_map.h"
#include "nexus/renderer/post_process.h"
#include "nexus/renderer/deferred_renderer.h"
#include "nexus/renderer/terrain.h"
#include "nexus/renderer/fog.h"
#include "nexus/renderer/camera.h"

using namespace nexus;

// =============================================================================
// PBR Material Tests
// =============================================================================

TEST(PBRMaterial, DefaultValues) {
    PBRMaterial mat;
    EXPECT_FLOAT_EQ(mat.albedo.r, 1.0f);
    EXPECT_FLOAT_EQ(mat.albedo.g, 1.0f);
    EXPECT_FLOAT_EQ(mat.albedo.b, 1.0f);
    EXPECT_FLOAT_EQ(mat.albedo.a, 1.0f);
    EXPECT_FLOAT_EQ(mat.metallic, 0.0f);
    EXPECT_FLOAT_EQ(mat.roughness, 0.5f);
    EXPECT_FLOAT_EQ(mat.normal_strength, 1.0f);
    EXPECT_FLOAT_EQ(mat.ao_strength, 1.0f);
    EXPECT_FLOAT_EQ(mat.emissive_strength, 1.0f);
    EXPECT_FLOAT_EQ(mat.alpha_cutoff, 0.5f);
    EXPECT_FALSE(mat.transparent);
    EXPECT_FALSE(mat.double_sided);
    EXPECT_EQ(mat.albedo_map, rhi::INVALID_HANDLE);
    EXPECT_EQ(mat.metallic_roughness_map, rhi::INVALID_HANDLE);
    EXPECT_EQ(mat.normal_map, rhi::INVALID_HANDLE);
    EXPECT_EQ(mat.ao_map, rhi::INVALID_HANDLE);
    EXPECT_EQ(mat.emissive_map, rhi::INVALID_HANDLE);
}

TEST(PBRMaterial, CustomValues) {
    PBRMaterial mat;
    mat.albedo = Vec4(0.8f, 0.2f, 0.1f, 1.0f);
    mat.metallic = 1.0f;
    mat.roughness = 0.1f;
    mat.emissive = Vec3(1.0f, 0.5f, 0.0f);
    mat.emissive_strength = 5.0f;
    mat.transparent = true;
    mat.double_sided = true;

    EXPECT_FLOAT_EQ(mat.metallic, 1.0f);
    EXPECT_FLOAT_EQ(mat.roughness, 0.1f);
    EXPECT_FLOAT_EQ(mat.emissive.x, 1.0f);
    EXPECT_FLOAT_EQ(mat.emissive_strength, 5.0f);
    EXPECT_TRUE(mat.transparent);
    EXPECT_TRUE(mat.double_sided);
}

TEST(IBLData, DefaultValues) {
    IBLData ibl;
    EXPECT_EQ(ibl.irradiance_map, rhi::INVALID_HANDLE);
    EXPECT_EQ(ibl.prefiltered_map, rhi::INVALID_HANDLE);
    EXPECT_EQ(ibl.brdf_lut, rhi::INVALID_HANDLE);
    EXPECT_FLOAT_EQ(ibl.intensity, 1.0f);
}

// =============================================================================
// Shadow Map Tests
// =============================================================================

TEST(CascadedShadowMap, ConfigDefaults) {
    CascadedShadowMap::Config config;
    EXPECT_EQ(config.resolution, 1024u);
    EXPECT_EQ(config.num_cascades, 3u);
    EXPECT_FLOAT_EQ(config.cascade_split_lambda, 0.75f);
    EXPECT_FLOAT_EQ(config.shadow_distance, 100.0f);
    EXPECT_FLOAT_EQ(config.bias, 0.005f);
    EXPECT_FLOAT_EQ(config.normal_bias, 0.02f);
}

TEST(CascadedShadowMap, MaxCascades) {
    EXPECT_EQ(CascadedShadowMap::MAX_CASCADES, 4u);
}

TEST(CascadedShadowMap, CascadeDataLayout) {
    CascadedShadowMap::CascadeData data;
    data.light_view_projection = Mat4(1.0f);
    data.split_depth = 50.0f;
    EXPECT_FLOAT_EQ(data.split_depth, 50.0f);
}

TEST(PointLightShadow, ConfigDefaults) {
    PointLightShadow::Config config;
    EXPECT_EQ(config.resolution, 512u);
    EXPECT_FLOAT_EQ(config.near_plane, 0.1f);
    EXPECT_FLOAT_EQ(config.far_plane, 25.0f);
    EXPECT_FLOAT_EQ(config.bias, 0.005f);
}

// =============================================================================
// Post-Process Tests
// =============================================================================

TEST(ToneMappingEffect, DefaultValues) {
    ToneMappingEffect effect;
    EXPECT_FLOAT_EQ(effect.exposure, 1.0f);
    EXPECT_FLOAT_EQ(effect.gamma, 2.2f);
    EXPECT_EQ(effect.mode, ToneMappingEffect::ACES);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(effect.name(), "ToneMapping");
}

TEST(ToneMappingEffect, Modes) {
    ToneMappingEffect effect;
    effect.mode = ToneMappingEffect::Reinhard;
    EXPECT_EQ(effect.mode, ToneMappingEffect::Reinhard);
    effect.mode = ToneMappingEffect::Uncharted2;
    EXPECT_EQ(effect.mode, ToneMappingEffect::Uncharted2);
    effect.mode = ToneMappingEffect::None;
    EXPECT_EQ(effect.mode, ToneMappingEffect::None);
}

TEST(BloomEffect, DefaultValues) {
    BloomEffect effect;
    EXPECT_FLOAT_EQ(effect.threshold, 1.0f);
    EXPECT_FLOAT_EQ(effect.intensity, 0.5f);
    EXPECT_EQ(effect.blur_passes, 5u);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(effect.name(), "Bloom");
}

TEST(FXAAEffect, DefaultValues) {
    FXAAEffect effect;
    EXPECT_FLOAT_EQ(effect.subpixel_quality, 0.75f);
    EXPECT_FLOAT_EQ(effect.edge_threshold, 0.125f);
    EXPECT_FLOAT_EQ(effect.edge_threshold_min, 0.0625f);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(effect.name(), "FXAA");
}

TEST(VignetteEffect, DefaultValues) {
    VignetteEffect effect;
    EXPECT_FLOAT_EQ(effect.intensity, 0.3f);
    EXPECT_FLOAT_EQ(effect.smoothness, 0.5f);
    EXPECT_TRUE(effect.enabled);
    EXPECT_EQ(effect.name(), "Vignette");
}

TEST(PostProcessEffect, EnableDisable) {
    ToneMappingEffect effect;
    EXPECT_TRUE(effect.enabled);
    effect.enabled = false;
    EXPECT_FALSE(effect.enabled);
    effect.enabled = true;
    EXPECT_TRUE(effect.enabled);
}

// =============================================================================
// GBuffer / Deferred Tests
// =============================================================================

TEST(GBuffer, DefaultValues) {
    GBuffer gb;
    EXPECT_EQ(gb.framebuffer, rhi::INVALID_HANDLE);
    EXPECT_EQ(gb.albedo_metallic, rhi::INVALID_HANDLE);
    EXPECT_EQ(gb.normal, rhi::INVALID_HANDLE);
    EXPECT_EQ(gb.roughness_ao, rhi::INVALID_HANDLE);
    EXPECT_EQ(gb.depth, rhi::INVALID_HANDLE);
    EXPECT_EQ(gb.width, 0u);
    EXPECT_EQ(gb.height, 0u);
}

TEST(PointLight, DefaultValues) {
    PointLight light;
    EXPECT_FLOAT_EQ(light.position.x, 0.0f);
    EXPECT_FLOAT_EQ(light.radius, 10.0f);
    EXPECT_FLOAT_EQ(light.color.x, 1.0f);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
}

TEST(PointLight, CustomValues) {
    PointLight light;
    light.position = Vec3(5.0f, 3.0f, -2.0f);
    light.radius = 25.0f;
    light.color = Vec3(1.0f, 0.5f, 0.0f);
    light.intensity = 3.0f;

    EXPECT_FLOAT_EQ(light.position.x, 5.0f);
    EXPECT_FLOAT_EQ(light.radius, 25.0f);
    EXPECT_FLOAT_EQ(light.intensity, 3.0f);
}

TEST(DeferredRenderer, MaxPointLights) {
    EXPECT_EQ(DeferredRenderer::MAX_POINT_LIGHTS, 32u);
}

// =============================================================================
// Heightmap Tests
// =============================================================================

TEST(Heightmap, CreateFlat) {
    Heightmap hm;
    hm.create(8, 8, 0.0f);
    EXPECT_EQ(hm.width(), 8u);
    EXPECT_EQ(hm.height(), 8u);
    EXPECT_FLOAT_EQ(hm.get(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(hm.get(7, 7), 0.0f);
}

TEST(Heightmap, CreateWithDefaultHeight) {
    Heightmap hm;
    hm.create(4, 4, 5.0f);
    for (u32 z = 0; z < 4; ++z) {
        for (u32 x = 0; x < 4; ++x) {
            EXPECT_FLOAT_EQ(hm.get(x, z), 5.0f);
        }
    }
}

TEST(Heightmap, SetAndGet) {
    Heightmap hm;
    hm.create(4, 4);
    hm.set(1, 2, 10.0f);
    EXPECT_FLOAT_EQ(hm.get(1, 2), 10.0f);
    EXPECT_FLOAT_EQ(hm.get(0, 0), 0.0f);
}

TEST(Heightmap, OutOfBounds) {
    Heightmap hm;
    hm.create(4, 4, 1.0f);
    EXPECT_FLOAT_EQ(hm.get(10, 10), 0.0f);  // out of bounds returns 0
    hm.set(10, 10, 99.0f);  // should be silently ignored
    EXPECT_FLOAT_EQ(hm.get(10, 10), 0.0f);
}

TEST(Heightmap, CreateFromData) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    Heightmap hm;
    hm.create_from_data(2, 2, data);
    EXPECT_FLOAT_EQ(hm.get(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(hm.get(1, 0), 2.0f);
    EXPECT_FLOAT_EQ(hm.get(0, 1), 3.0f);
    EXPECT_FLOAT_EQ(hm.get(1, 1), 4.0f);
}

TEST(Heightmap, BilinearSample) {
    // 2x2 heightmap: corners at 0, 1, 2, 3
    float data[] = {0.0f, 1.0f, 2.0f, 3.0f};
    Heightmap hm;
    hm.create_from_data(2, 2, data);

    // Corners
    EXPECT_FLOAT_EQ(hm.sample(0.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(hm.sample(1.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(hm.sample(0.0f, 1.0f), 2.0f);
    EXPECT_FLOAT_EQ(hm.sample(1.0f, 1.0f), 3.0f);

    // Center should interpolate
    float center = hm.sample(0.5f, 0.5f);
    EXPECT_NEAR(center, 1.5f, 0.01f);
}

TEST(Heightmap, Normal) {
    // Flat heightmap: normals should point up
    Heightmap hm;
    hm.create(4, 4, 0.0f);
    Vec3 n = hm.normal_at(2, 2, 1.0f);
    EXPECT_NEAR(n.y, 1.0f, 0.01f);
}

TEST(Heightmap, NormalOnSlope) {
    // Create a heightmap with a slope in the X direction
    Heightmap hm;
    hm.create(4, 4, 0.0f);
    for (u32 x = 0; x < 4; ++x) {
        for (u32 z = 0; z < 4; ++z) {
            hm.set(x, z, static_cast<float>(x) * 1.0f);
        }
    }
    Vec3 n = hm.normal_at(2, 2, 1.0f);
    // Normal should tilt in negative X direction (slope goes up in +X)
    EXPECT_LT(n.x, 0.0f);
    EXPECT_GT(n.y, 0.0f);
}

// =============================================================================
// Terrain Renderer Config Tests
// =============================================================================

TEST(TerrainRenderer, ConfigDefaults) {
    TerrainRenderer::Config config;
    EXPECT_FLOAT_EQ(config.cell_size, 1.0f);
    EXPECT_FLOAT_EQ(config.height_scale, 50.0f);
    EXPECT_EQ(config.chunk_size, 32u);
    EXPECT_EQ(config.max_lod, 4u);
    EXPECT_FLOAT_EQ(config.lod_distance, 100.0f);
}

TEST(TerrainRenderer, SplatLayerDefaults) {
    TerrainRenderer::SplatLayer layer;
    EXPECT_EQ(layer.texture, rhi::INVALID_HANDLE);
    EXPECT_FLOAT_EQ(layer.uv_scale, 1.0f);
}

// =============================================================================
// Fog Tests
// =============================================================================

TEST(FogSettings, DefaultValues) {
    FogSettings fog;
    EXPECT_EQ(fog.mode, FogSettings::Mode::None);
    EXPECT_FLOAT_EQ(fog.density, 0.02f);
    EXPECT_FLOAT_EQ(fog.near_distance, 10.0f);
    EXPECT_FLOAT_EQ(fog.far_distance, 100.0f);
    EXPECT_FLOAT_EQ(fog.height_falloff, 0.1f);
    EXPECT_FLOAT_EQ(fog.base_height, 0.0f);
    EXPECT_FLOAT_EQ(fog.max_height, 50.0f);
}

TEST(FogSettings, NoFog) {
    FogSettings fog;
    fog.mode = FogSettings::Mode::None;
    EXPECT_FLOAT_EQ(fog.compute_factor(50.0f), 1.0f);
    EXPECT_FLOAT_EQ(fog.compute_factor(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(fog.compute_factor(1000.0f), 1.0f);
}

TEST(FogSettings, LinearFog) {
    FogSettings fog;
    fog.mode = FogSettings::Mode::Linear;
    fog.near_distance = 10.0f;
    fog.far_distance = 100.0f;

    // At near: no fog (factor = 1)
    EXPECT_FLOAT_EQ(fog.compute_factor(10.0f), 1.0f);

    // At far: full fog (factor = 0)
    EXPECT_FLOAT_EQ(fog.compute_factor(100.0f), 0.0f);

    // Midpoint
    EXPECT_NEAR(fog.compute_factor(55.0f), 0.5f, 0.01f);

    // Beyond far: clamped to 0
    EXPECT_FLOAT_EQ(fog.compute_factor(200.0f), 0.0f);

    // Before near: clamped to 1
    EXPECT_FLOAT_EQ(fog.compute_factor(0.0f), 1.0f);
}

TEST(FogSettings, ExponentialFog) {
    FogSettings fog;
    fog.mode = FogSettings::Mode::Exponential;
    fog.density = 0.05f;

    // At distance 0: no fog
    EXPECT_FLOAT_EQ(fog.compute_factor(0.0f), 1.0f);

    // Factor decreases with distance
    float f20 = fog.compute_factor(20.0f);
    float f50 = fog.compute_factor(50.0f);
    EXPECT_GT(f20, f50);
    EXPECT_GT(f20, 0.0f);
    EXPECT_LT(f20, 1.0f);

    // Very large distance: nearly full fog
    float f_far = fog.compute_factor(500.0f);
    EXPECT_NEAR(f_far, 0.0f, 0.01f);
}

TEST(FogSettings, ExponentialSquaredFog) {
    FogSettings fog;
    fog.mode = FogSettings::Mode::ExponentialSquared;
    fog.density = 0.05f;

    EXPECT_FLOAT_EQ(fog.compute_factor(0.0f), 1.0f);

    // Should decrease faster than linear exponential
    float f_exp2 = fog.compute_factor(30.0f);
    EXPECT_GT(f_exp2, 0.0f);
    EXPECT_LT(f_exp2, 1.0f);
}

TEST(FogSettings, HeightBasedFog) {
    FogSettings fog;
    fog.mode = FogSettings::Mode::HeightBased;
    fog.density = 0.02f;
    fog.base_height = 0.0f;
    fog.max_height = 100.0f;
    fog.height_falloff = 0.1f;

    // At ground level, fog should be present (based on distance)
    float f_ground = fog.compute_factor(50.0f, 0.0f);

    // High up, fog should be less
    float f_high = fog.compute_factor(50.0f, 80.0f);
    EXPECT_GT(f_high, f_ground);  // higher = less fog = higher factor
}

TEST(FogSettings, FogColor) {
    FogSettings fog;
    fog.color = Vec3(0.5f, 0.6f, 0.7f);
    EXPECT_FLOAT_EQ(fog.color.x, 0.5f);
    EXPECT_FLOAT_EQ(fog.color.y, 0.6f);
    EXPECT_FLOAT_EQ(fog.color.z, 0.7f);
}
