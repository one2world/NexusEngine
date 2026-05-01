#include <gtest/gtest.h>

#include "nexus/editor/material_asset.h"

#include "nexus/assets/asset_handle.h"
#include "nexus/assets/asset_loader.h"
#include "nexus/assets/asset_registry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace nexus::editor::tests {

namespace fs = std::filesystem;

namespace {

fs::path tmp_path(const char* name) {
    return fs::temp_directory_path() / name;
}

bool write_legacy_kv(const fs::path& path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "# legacy material\n"
        << "shader = phong\n"
        << "albedo = textures/wood.png\n"
        << "metallic = 0.25\n"
        << "roughness = 0.75\n";
    return true;
}

bool write_json_mat(const fs::path& path) {
    std::ofstream out(path);
    if (!out) return false;
    out << R"({
  "shader": "pbr",
  "albedo": "textures/marble.png",
  "normal": "textures/marble_n.png",
  "metallic": 0.4,
  "roughness": 0.2,
  "color": [0.5, 0.6, 0.7, 1.0]
})";
    return true;
}

}  // namespace

// ── load() — legacy + JSON ──────────────────────────────────────────────────

TEST(MaterialAssetCache, LoadLegacyKVMaterial) {
    const fs::path p = tmp_path("nexus_mat_kv.mat");
    ASSERT_TRUE(write_legacy_kv(p));

    MaterialAssetCache cache;
    auto m = cache.load(p.string());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->shader_path, "phong");
    EXPECT_EQ(m->albedo_texture, "textures/wood.png");
    EXPECT_NEAR(m->metallic,  0.25f, 1e-5f);
    EXPECT_NEAR(m->roughness, 0.75f, 1e-5f);
    EXPECT_EQ(cache.size(), 1u);

    fs::remove(p);
}

TEST(MaterialAssetCache, LoadJSONMaterial) {
    const fs::path p = tmp_path("nexus_mat_json.mat");
    ASSERT_TRUE(write_json_mat(p));

    MaterialAssetCache cache;
    auto m = cache.load(p.string());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->shader_path, "pbr");
    EXPECT_EQ(m->albedo_texture, "textures/marble.png");
    EXPECT_EQ(m->normal_texture, "textures/marble_n.png");
    EXPECT_NEAR(m->metallic,  0.4f, 1e-5f);
    EXPECT_NEAR(m->roughness, 0.2f, 1e-5f);
    EXPECT_NEAR(m->color[0], 0.5f, 1e-5f);
    EXPECT_NEAR(m->color[3], 1.0f, 1e-5f);

    fs::remove(p);
}

TEST(MaterialAssetCache, LoadCachesByPath) {
    const fs::path p = tmp_path("nexus_mat_cache.mat");
    ASSERT_TRUE(write_json_mat(p));
    MaterialAssetCache cache;
    auto a = cache.load(p.string());
    auto b = cache.load(p.string());
    EXPECT_EQ(a.get(), b.get());
    EXPECT_EQ(cache.size(), 1u);
    fs::remove(p);
}

TEST(MaterialAssetCache, LoadMissingFileReturnsNullptr) {
    MaterialAssetCache cache;
    EXPECT_EQ(cache.load("/nope/missing.mat"), nullptr);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(MaterialAssetCache, LoadCorruptJSONReturnsNullptr) {
    const fs::path p = tmp_path("nexus_mat_corrupt.mat");
    {
        std::ofstream out(p);
        out << "{ broken json: true,";
    }
    MaterialAssetCache cache;
    EXPECT_EQ(cache.load(p.string()), nullptr);
    fs::remove(p);
}

// ── id_for() / path_for() / get_by_id() ─────────────────────────────────────

TEST(MaterialAssetCache, IdForAllocatesAndIsStable) {
    MaterialAssetCache cache;
    const u32 a1 = cache.id_for("/proj/a.mat");
    const u32 a2 = cache.id_for("/proj/a.mat");
    const u32 b  = cache.id_for("/proj/b.mat");
    EXPECT_NE(a1, 0u);
    EXPECT_EQ(a1, a2);
    EXPECT_NE(a1, b);
    EXPECT_GE(a1, 4096u);  // material ids start above mesh-import range
}

TEST(MaterialAssetCache, IdForEmptyPathReturnsZero) {
    MaterialAssetCache cache;
    EXPECT_EQ(cache.id_for(""), 0u);
}

TEST(MaterialAssetCache, PathForRoundTripsAfterLoad) {
    const fs::path p = tmp_path("nexus_mat_roundtrip.mat");
    ASSERT_TRUE(write_json_mat(p));
    MaterialAssetCache cache;
    cache.load(p.string());
    const u32 id = cache.id_for(p.string());
    EXPECT_EQ(cache.path_for(id), p.string());
    EXPECT_EQ(cache.path_for(0xDEADBEEFu), "");  // unknown id
    fs::remove(p);
}

TEST(MaterialAssetCache, GetByIdReturnsCachedMaterial) {
    const fs::path p = tmp_path("nexus_mat_byid.mat");
    ASSERT_TRUE(write_json_mat(p));
    MaterialAssetCache cache;
    cache.load(p.string());
    const u32 id = cache.id_for(p.string());
    auto m = cache.get_by_id(id);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->shader_path, "pbr");
    EXPECT_EQ(cache.get_by_id(0xDEADu), nullptr);
    fs::remove(p);
}

// ── save() round-trips through JSON ─────────────────────────────────────────

TEST(MaterialAssetCache, SaveWritesJSONAndUpdatesCache) {
    const fs::path p = tmp_path("nexus_mat_saved.mat");
    if (fs::exists(p)) fs::remove(p);

    MaterialAssetCache cache;
    nexus::assets::MaterialData data;
    data.shader_path        = "pbr_v2";
    data.albedo_texture     = "textures/test.png";
    data.metallic           = 0.6f;
    data.roughness          = 0.3f;
    data.color[0]           = 0.1f;
    data.color[1]           = 0.2f;
    data.color[2]           = 0.3f;
    data.color[3]           = 1.0f;

    ASSERT_TRUE(cache.save(p.string(), data));
    ASSERT_TRUE(fs::exists(p));

    // The saved file should be parseable by a fresh cache.
    MaterialAssetCache reload;
    auto m = reload.load(p.string());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->shader_path, "pbr_v2");
    EXPECT_NEAR(m->metallic,  0.6f, 1e-5f);
    EXPECT_NEAR(m->roughness, 0.3f, 1e-5f);
    EXPECT_NEAR(m->color[0],  0.1f, 1e-5f);
    EXPECT_NEAR(m->color[1],  0.2f, 1e-5f);

    // Saving must populate the originating cache so subsequent get_by_id
    // works without a re-read.
    EXPECT_EQ(cache.size(), 1u);
    const u32 id = cache.id_for(p.string());
    auto cached = cache.get_by_id(id);
    ASSERT_NE(cached, nullptr);
    EXPECT_NEAR(cached->metallic, 0.6f, 1e-5f);

    fs::remove(p);
}

TEST(MaterialAssetCache, SaveRefusesEmptyPath) {
    MaterialAssetCache cache;
    nexus::assets::MaterialData data;
    EXPECT_FALSE(cache.save("", data));
    EXPECT_EQ(cache.size(), 0u);
}

// ── create_default() ────────────────────────────────────────────────────────

TEST(MaterialAssetCache, CreateDefaultWritesNewMaterial) {
    const fs::path p = tmp_path("nexus_mat_default.mat");
    if (fs::exists(p)) fs::remove(p);

    MaterialAssetCache cache;
    const u32 id = cache.create_default(p.string());
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(fs::exists(p));
    auto m = cache.get_by_id(id);
    ASSERT_NE(m, nullptr);
    EXPECT_NEAR(m->metallic,  0.0f, 1e-5f);
    EXPECT_NEAR(m->roughness, 1.0f, 1e-5f);
    EXPECT_NEAR(m->color[0],  1.0f, 1e-5f);
    EXPECT_NEAR(m->color[3],  1.0f, 1e-5f);

    fs::remove(p);
}

TEST(MaterialAssetCache, CreateDefaultRefusesOverwriteByDefault) {
    const fs::path p = tmp_path("nexus_mat_overwrite.mat");
    {
        std::ofstream out(p);
        out << R"({"metallic": 0.99, "roughness": 0.01})";
    }
    MaterialAssetCache cache;
    const u32 id = cache.create_default(p.string());
    EXPECT_NE(id, 0u);  // returns existing id, doesn't overwrite
    auto m = cache.get_by_id(id);
    ASSERT_NE(m, nullptr);
    // Existing content preserved.
    EXPECT_NEAR(m->metallic,  0.99f, 1e-5f);
    EXPECT_NEAR(m->roughness, 0.01f, 1e-5f);

    // With overwrite=true the file is rewritten with defaults.  Use a
    // fresh cache instance because the type intentionally disables copy /
    // move (it owns shared_ptrs that mustn't double-release).
    MaterialAssetCache cache2;
    EXPECT_NE(cache2.create_default(p.string(), /*overwrite=*/true), 0u);
    auto fresh = cache2.get_by_id(cache2.id_for(p.string()));
    ASSERT_NE(fresh, nullptr);
    EXPECT_NEAR(fresh->metallic,  0.0f, 1e-5f);
    EXPECT_NEAR(fresh->roughness, 1.0f, 1e-5f);

    fs::remove(p);
}

}  // namespace nexus::editor::tests
