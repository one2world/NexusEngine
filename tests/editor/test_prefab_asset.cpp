#include <gtest/gtest.h>

#include "nexus/editor/prefab_asset.h"
#include "nexus/scene/components.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/scene/scene.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace nexus::editor::tests {

namespace fs = std::filesystem;

namespace {

fs::path tmp_path(const char* name) {
    return fs::temp_directory_path() / name;
}

}  // namespace

// ── Save round-trip ─────────────────────────────────────────────────────────

TEST(PrefabAssetCache, SaveFromEntityWritesJSONAndCaches) {
    const fs::path p = tmp_path("nexus_prefab_save.prefab");
    if (fs::exists(p)) fs::remove(p);

    Scene scene;
    Entity e = scene.create_entity_3d("PlayerCharacter");
    auto& tc = scene.registry().get_component<Transform3DComponent>(e);
    tc.position = Vec3(1.0f, 2.0f, 3.0f);

    PrefabAssetCache cache;
    const Prefab* pf = cache.save_from_entity(p.string(), scene, e);
    ASSERT_NE(pf, nullptr);
    EXPECT_TRUE(pf->valid());
    EXPECT_TRUE(fs::exists(p));
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_TRUE(cache.contains(p.string()));

    // Cached pointer should be re-fetchable via get().
    EXPECT_NE(cache.get(p.string()), nullptr);

    fs::remove(p);
}

TEST(PrefabAssetCache, SaveFromDeadEntityFails) {
    const fs::path p = tmp_path("nexus_prefab_dead.prefab");
    Scene scene;
    Entity e = scene.create_entity_3d("Tmp");
    scene.registry().destroy(e);
    PrefabAssetCache cache;
    EXPECT_EQ(cache.save_from_entity(p.string(), scene, e), nullptr);
    EXPECT_FALSE(fs::exists(p));
}

TEST(PrefabAssetCache, SaveRefusesEmptyPath) {
    Scene scene;
    Entity e = scene.create_entity_3d("X");
    PrefabAssetCache cache;
    EXPECT_EQ(cache.save_from_entity("", scene, e), nullptr);
}

// ── Load round-trip ─────────────────────────────────────────────────────────

TEST(PrefabAssetCache, LoadCachesByPath) {
    const fs::path p = tmp_path("nexus_prefab_load.prefab");
    {
        Scene scene;
        Entity e = scene.create_entity_3d("Cube");
        PrefabAssetCache cache;
        ASSERT_NE(cache.save_from_entity(p.string(), scene, e), nullptr);
    }

    PrefabAssetCache cache;
    const Prefab* a = cache.load(p.string());
    const Prefab* b = cache.load(p.string());
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a, b);  // cached, same pointer
    EXPECT_EQ(cache.size(), 1u);

    fs::remove(p);
}

TEST(PrefabAssetCache, LoadMissingFileReturnsNullptr) {
    PrefabAssetCache cache;
    EXPECT_EQ(cache.load("/nope/missing.prefab"), nullptr);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(PrefabAssetCache, LoadEmptyFileReturnsNullptr) {
    const fs::path p = tmp_path("nexus_prefab_empty.prefab");
    { std::ofstream out(p); }  // truncate to empty
    PrefabAssetCache cache;
    EXPECT_EQ(cache.load(p.string()), nullptr);
    fs::remove(p);
}

TEST(PrefabAssetCache, LoadInvalidJSONReturnsNullptr) {
    const fs::path p = tmp_path("nexus_prefab_bad.prefab");
    {
        std::ofstream out(p);
        out << "this is not json";
    }
    PrefabAssetCache cache;
    EXPECT_EQ(cache.load(p.string()), nullptr);
    fs::remove(p);
}

// ── Instantiate ─────────────────────────────────────────────────────────────

TEST(PrefabAssetCache, InstantiateProducesEntityWithComponents) {
    const fs::path p = tmp_path("nexus_prefab_inst.prefab");
    {
        Scene src;
        Entity e = src.create_entity_3d("Hero");
        auto& tc = src.registry().get_component<Transform3DComponent>(e);
        tc.position = Vec3(7.0f, 8.0f, 9.0f);
        src.registry().add_component<MeshRendererComponent>(e, MeshRendererComponent{});
        PrefabAssetCache cache;
        ASSERT_NE(cache.save_from_entity(p.string(), src, e), nullptr);
    }

    Scene dst;
    PrefabAssetCache cache;
    const Entity root = cache.instantiate(p.string(), dst);
    EXPECT_NE(root, INVALID_ENTITY);
    auto& reg = dst.registry();
    ASSERT_TRUE(reg.alive(root));
    ASSERT_TRUE(reg.has_component<TagComponent>(root));
    EXPECT_EQ(reg.get_component<TagComponent>(root).name, "Hero");
    ASSERT_TRUE(reg.has_component<Transform3DComponent>(root));
    EXPECT_NEAR(reg.get_component<Transform3DComponent>(root).position.x, 7.0f, 1e-5f);
    EXPECT_TRUE(reg.has_component<MeshRendererComponent>(root));

    fs::remove(p);
}

TEST(PrefabAssetCache, InstantiateMissingFileReturnsInvalidEntity) {
    Scene dst;
    PrefabAssetCache cache;
    EXPECT_EQ(cache.instantiate("/nope/file.prefab", dst), INVALID_ENTITY);
}

TEST(PrefabAssetCache, InstantiateTwiceProducesIndependentEntities) {
    const fs::path p = tmp_path("nexus_prefab_dup.prefab");
    {
        Scene src;
        Entity e = src.create_entity_3d("Token");
        PrefabAssetCache cache;
        ASSERT_NE(cache.save_from_entity(p.string(), src, e), nullptr);
    }

    Scene dst;
    PrefabAssetCache cache;
    const Entity a = cache.instantiate(p.string(), dst);
    const Entity b = cache.instantiate(p.string(), dst);
    EXPECT_NE(a, INVALID_ENTITY);
    EXPECT_NE(b, INVALID_ENTITY);
    EXPECT_NE(a, b);

    fs::remove(p);
}

TEST(PrefabAssetCache, InstantiatePreservesAudioAndLockedComponents) {
    // Regression guard for the prefab fix in this milestone — Audio /
    // Tilemap / Active / Locked were silently dropped before because
    // copy_entity_components didn't list them.
    const fs::path p = tmp_path("nexus_prefab_audio.prefab");
    {
        Scene src;
        Entity e = src.create_entity_3d("Speaker");
        src.registry().add_component<AudioSourceComponent>(e, AudioSourceComponent{});
        src.registry().add_component<LockedComponent>(e, LockedComponent{});
        src.registry().set_active(e, false);
        PrefabAssetCache cache;
        ASSERT_NE(cache.save_from_entity(p.string(), src, e), nullptr);
    }

    Scene dst;
    PrefabAssetCache cache;
    const Entity root = cache.instantiate(p.string(), dst);
    ASSERT_NE(root, INVALID_ENTITY);
    auto& reg = dst.registry();
    EXPECT_TRUE(reg.has_component<AudioSourceComponent>(root));
    EXPECT_TRUE(reg.has_component<LockedComponent>(root));
    EXPECT_FALSE(reg.is_active(root));

    fs::remove(p);
}

// ── Stable id allocation ────────────────────────────────────────────────────

TEST(PrefabAssetCache, IdForAllocatesAndIsStable) {
    PrefabAssetCache cache;
    const u32 a1 = cache.id_for("/proj/a.prefab");
    const u32 a2 = cache.id_for("/proj/a.prefab");
    const u32 b  = cache.id_for("/proj/b.prefab");
    EXPECT_NE(a1, 0u);
    EXPECT_EQ(a1, a2);
    EXPECT_NE(a1, b);
    EXPECT_GE(a1, 8192u);  // prefab ids start above material range (4096+)
}

TEST(PrefabAssetCache, IdForEmptyPathReturnsZero) {
    PrefabAssetCache cache;
    EXPECT_EQ(cache.id_for(""), 0u);
}

TEST(PrefabAssetCache, PathForRoundTripsAfterLoad) {
    const fs::path p = tmp_path("nexus_prefab_id.prefab");
    {
        Scene src;
        Entity e = src.create_entity_3d("X");
        PrefabAssetCache cache;
        ASSERT_NE(cache.save_from_entity(p.string(), src, e), nullptr);
    }
    PrefabAssetCache cache;
    cache.load(p.string());
    const u32 id = cache.id_for(p.string());
    EXPECT_EQ(cache.path_for(id), p.string());
    EXPECT_EQ(cache.path_for(0xDEADBEEFu), "");
    fs::remove(p);
}

}  // namespace nexus::editor::tests
