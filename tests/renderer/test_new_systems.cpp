#include <gtest/gtest.h>
#include "nexus/renderer/lightmap_baker.h"
#include "nexus/renderer/gpu_particles.h"
#include "nexus/renderer/decal_system.h"
#include "nexus/renderer/shader_library.h"
#include "nexus/renderer/light_probe.h"
#include "nexus/renderer/mobile_renderer.h"

using namespace nexus;

// ── Lightmap Baker Tests ─────────────────────────────────────────────────

TEST(LightmapBaker, DefaultConfig) {
    LightmapConfig config;
    EXPECT_EQ(config.resolution, 256u);
    EXPECT_EQ(config.samples_per_texel, 64u);
    EXPECT_EQ(config.bounces, 2u);
    EXPECT_FLOAT_EQ(config.bias, 0.001f);
}

TEST(LightmapBaker, AddMeshAndLight) {
    LightmapBaker baker;

    BakeMesh mesh;
    BakeTriangle tri;
    tri.v0 = Vec3(0, 0, 0); tri.v1 = Vec3(1, 0, 0); tri.v2 = Vec3(0, 1, 0);
    tri.n0 = tri.n1 = tri.n2 = Vec3(0, 0, 1);
    tri.uv0 = Vec2(0, 0); tri.uv1 = Vec2(1, 0); tri.uv2 = Vec2(0, 1);
    mesh.triangles.push_back(tri);

    baker.add_mesh(mesh);
    EXPECT_EQ(baker.mesh_count(), 1u);

    BakeLight light;
    light.direction = Vec3(0, -1, 0);
    light.color = Vec3(1, 1, 1);
    light.intensity = 1.0f;
    baker.add_light(light);
    EXPECT_EQ(baker.light_count(), 1u);

    baker.clear();
    EXPECT_EQ(baker.mesh_count(), 0u);
    EXPECT_EQ(baker.light_count(), 0u);
}

TEST(LightmapBaker, BakeProducesResult) {
    LightmapBaker baker;

    LightmapConfig config;
    config.resolution = 16;
    config.samples_per_texel = 4;
    config.bounces = 1;
    baker.set_config(config);

    BakeMesh mesh;
    BakeTriangle tri;
    tri.v0 = Vec3(-1, 0, -1); tri.v1 = Vec3(1, 0, -1); tri.v2 = Vec3(0, 0, 1);
    tri.n0 = tri.n1 = tri.n2 = Vec3(0, 1, 0);
    tri.uv0 = Vec2(0, 0); tri.uv1 = Vec2(1, 0); tri.uv2 = Vec2(0.5f, 1);
    mesh.triangles.push_back(tri);
    baker.add_mesh(mesh);

    BakeLight light;
    light.direction = Vec3(0, -1, 0);
    baker.add_light(light);

    auto result = baker.bake(0);
    EXPECT_EQ(result.width, 16u);
    EXPECT_EQ(result.height, 16u);
    EXPECT_EQ(result.pixels.size(), static_cast<std::size_t>(16 * 16));
}

TEST(LightmapBaker, Tonemap) {
    LightmapResult result;
    result.width = 2;
    result.height = 2;
    result.pixels = {Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(0.5f)};
    result.tonemap(1.0f);

    EXPECT_EQ(result.pixels_ldr.size(), static_cast<std::size_t>(2 * 2 * 4)); // RGBA8
}

// ── GPU Particles Tests ──────────────────────────────────────────────────

TEST(GPUParticles, DefaultConfig) {
    GPUParticleEmitterConfig config;
    EXPECT_EQ(config.max_particles, 100000u);
    EXPECT_GT(config.emission_rate, 0.0f);
}

// ── Decal System Tests ───────────────────────────────────────────────────

TEST(DecalSystem, DefaultDecal) {
    Decal decal;
    EXPECT_EQ(decal.albedo_tex, rhi::INVALID_HANDLE);
    EXPECT_EQ(decal.normal_tex, rhi::INVALID_HANDLE);
    EXPECT_EQ(decal.layer, 0u);
}

// ── Light Probe Tests ────────────────────────────────────────────────────

TEST(LightProbe, SH9Evaluate) {
    SH9 sh;
    for (u32 i = 0; i < 9; ++i) {
        sh.coefficients[i] = Vec3(0.0f);
    }
    sh.coefficients[0] = Vec3(1.0f); // DC term

    Vec3 result = sh.evaluate(Vec3(0, 1, 0));
    EXPECT_GT(result.x, 0.0f);
}

TEST(LightProbe, SH9Addition) {
    SH9 a, b;
    for (u32 i = 0; i < 9; ++i) {
        a.coefficients[i] = Vec3(1.0f);
        b.coefficients[i] = Vec3(2.0f);
    }
    SH9 c = a + b;
    EXPECT_FLOAT_EQ(c.coefficients[0].x, 3.0f);
}

TEST(LightProbe, SH9Scale) {
    SH9 a;
    for (u32 i = 0; i < 9; ++i) {
        a.coefficients[i] = Vec3(2.0f);
    }
    SH9 b = a * 0.5f;
    EXPECT_FLOAT_EQ(b.coefficients[0].x, 1.0f);
}

TEST(LightProbe, GridConfig) {
    LightProbeGrid::Config config;
    config.origin = Vec3(0.0f);
    config.extent = Vec3(10.0f);
    config.resolution = IVec3(2, 2, 2);

    EXPECT_EQ(config.resolution.x, 2);
    EXPECT_EQ(config.resolution.y, 2);
    EXPECT_EQ(config.resolution.z, 2);
}

// ── Mobile Renderer Tests ────────────────────────────────────────────────

TEST(MobileRenderer, DefaultConfig) {
    MobileRendererConfig config;
    EXPECT_EQ(config.quality, MobileQuality::Medium);
    EXPECT_EQ(config.render_width, 1280u);
    EXPECT_EQ(config.render_height, 720u);
    EXPECT_TRUE(config.enable_shadows);
}

TEST(MobileRenderer, RenderItem) {
    MobileRenderer::RenderItem item;
    EXPECT_EQ(item.vbo, rhi::INVALID_HANDLE);
    EXPECT_EQ(item.ibo, rhi::INVALID_HANDLE);
    EXPECT_EQ(item.index_count, 0u);
    EXPECT_FLOAT_EQ(item.roughness, 0.5f);
}

TEST(MobileRenderer, ShaderStrings) {
    EXPECT_NE(mobile_shaders::MOBILE_VERT, nullptr);
    EXPECT_NE(mobile_shaders::MOBILE_FRAG, nullptr);
    EXPECT_NE(mobile_shaders::MOBILE_SHADOW_VERT, nullptr);
    EXPECT_NE(mobile_shaders::MOBILE_SHADOW_FRAG, nullptr);
    EXPECT_NE(mobile_shaders::MOBILE_SKYBOX_VERT, nullptr);
    EXPECT_NE(mobile_shaders::MOBILE_SKYBOX_FRAG, nullptr);
}
