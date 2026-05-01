#include <gtest/gtest.h>

#include "nexus/editor/animation_asset.h"

#include "nexus/animation/animation_clip.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace nexus::editor::tests {

namespace fs = std::filesystem;

namespace {

fs::path tmp_path(const char* name) {
    return fs::temp_directory_path() / name;
}

// Build a small clip with two channels (one bone) — position + rotation
// keyframes plus an event.  Round-trip target.
nexus::anim::AnimationClip make_test_clip() {
    nexus::anim::AnimationClip clip("walk", 1.5f);
    nexus::anim::BoneChannel ch;
    ch.bone_index = 0;
    ch.positions.push_back({0.0f, Vec3(0.0f, 0.0f, 0.0f)});
    ch.positions.push_back({1.0f, Vec3(1.0f, 0.5f, 0.25f)});
    ch.rotations.push_back({0.0f, Quat(1.0f, 0.0f, 0.0f, 0.0f)});
    ch.rotations.push_back({1.0f, glm::angleAxis(1.5f, Vec3(0.0f, 1.0f, 0.0f))});
    ch.scales.push_back({0.0f, Vec3(1.0f)});
    ch.scales.push_back({1.5f, Vec3(2.0f, 1.0f, 1.0f)});
    clip.add_channel(std::move(ch));
    clip.add_event(0.5f, "footstep");
    clip.add_event(1.0f, "swing");
    return clip;
}

}  // namespace

// ── Save round-trip ─────────────────────────────────────────────────────────

TEST(AnimationAssetCache, SaveAndReloadRoundTrip) {
    const fs::path p = tmp_path("nexus_anim_save.anim");
    if (fs::exists(p)) fs::remove(p);

    AnimationAssetCache cache;
    auto orig = make_test_clip();
    ASSERT_TRUE(cache.save(p.string(), orig));
    ASSERT_TRUE(fs::exists(p));

    AnimationAssetCache reload;
    const auto* clip = reload.load(p.string());
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->name(), "walk");
    EXPECT_NEAR(clip->duration(), 1.5f, 1e-5f);
    ASSERT_EQ(clip->channels().size(), 1u);
    const auto& ch = clip->channels()[0];
    EXPECT_EQ(ch.bone_index, 0);
    ASSERT_EQ(ch.positions.size(), 2u);
    EXPECT_NEAR(ch.positions[1].value.x, 1.0f, 1e-5f);
    EXPECT_NEAR(ch.positions[1].value.y, 0.5f, 1e-5f);
    ASSERT_EQ(ch.rotations.size(), 2u);
    ASSERT_EQ(ch.scales.size(), 2u);
    EXPECT_NEAR(ch.scales[1].time, 1.5f, 1e-5f);
    EXPECT_NEAR(ch.scales[1].value.x, 2.0f, 1e-5f);
    ASSERT_EQ(clip->events().size(), 2u);
    EXPECT_EQ(clip->events()[0].name, "footstep");
    EXPECT_NEAR(clip->events()[1].time, 1.0f, 1e-5f);

    fs::remove(p);
}

TEST(AnimationAssetCache, SaveRefusesEmptyPath) {
    AnimationAssetCache cache;
    EXPECT_FALSE(cache.save("", make_test_clip()));
}

TEST(AnimationAssetCache, SaveCachesForLaterGet) {
    const fs::path p = tmp_path("nexus_anim_cached.anim");
    if (fs::exists(p)) fs::remove(p);
    AnimationAssetCache cache;
    ASSERT_TRUE(cache.save(p.string(), make_test_clip()));
    EXPECT_NE(cache.get(p.string()), nullptr);
    EXPECT_EQ(cache.size(), 1u);
    fs::remove(p);
}

// ── Load semantics ──────────────────────────────────────────────────────────

TEST(AnimationAssetCache, LoadCachesByPath) {
    const fs::path p = tmp_path("nexus_anim_load.anim");
    {
        AnimationAssetCache writer;
        ASSERT_TRUE(writer.save(p.string(), make_test_clip()));
    }
    AnimationAssetCache cache;
    const auto* a = cache.load(p.string());
    const auto* b = cache.load(p.string());
    EXPECT_EQ(a, b);
    EXPECT_EQ(cache.size(), 1u);
    fs::remove(p);
}

TEST(AnimationAssetCache, LoadMissingFileReturnsNullptr) {
    AnimationAssetCache cache;
    EXPECT_EQ(cache.load("/nope/missing.anim"), nullptr);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(AnimationAssetCache, LoadCorruptJSONReturnsNullptr) {
    const fs::path p = tmp_path("nexus_anim_bad.anim");
    {
        std::ofstream out(p);
        out << "{ this isn't json }";
    }
    AnimationAssetCache cache;
    EXPECT_EQ(cache.load(p.string()), nullptr);
    fs::remove(p);
}

TEST(AnimationAssetCache, LoadMinimalClipWithoutChannelsOrEvents) {
    const fs::path p = tmp_path("nexus_anim_min.anim");
    {
        std::ofstream out(p);
        out << R"({"name":"empty","duration":2.0})";
    }
    AnimationAssetCache cache;
    const auto* clip = cache.load(p.string());
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->name(), "empty");
    EXPECT_NEAR(clip->duration(), 2.0f, 1e-5f);
    EXPECT_TRUE(clip->channels().empty());
    EXPECT_TRUE(clip->events().empty());
    fs::remove(p);
}

// ── Sampling correctness post-roundtrip ─────────────────────────────────────

TEST(AnimationAssetCache, SamplingMatchesAfterSaveLoad) {
    const fs::path p = tmp_path("nexus_anim_sample.anim");
    if (fs::exists(p)) fs::remove(p);
    AnimationAssetCache writer;
    ASSERT_TRUE(writer.save(p.string(), make_test_clip()));

    AnimationAssetCache reader;
    const auto* clip = reader.load(p.string());
    ASSERT_NE(clip, nullptr);

    // Sample halfway through the position track — should be (0.5, 0.25, 0.125).
    std::vector<nexus::anim::BonePose> poses;
    poses.resize(1);  // one bone
    clip->sample(0.5f, poses);
    EXPECT_NEAR(poses[0].position.x, 0.5f,  1e-4f);
    EXPECT_NEAR(poses[0].position.y, 0.25f, 1e-4f);

    fs::remove(p);
}

// ── Stable id allocation ────────────────────────────────────────────────────

TEST(AnimationAssetCache, IdForAllocatesAndIsStable) {
    AnimationAssetCache cache;
    const u32 a1 = cache.id_for("/proj/a.anim");
    const u32 a2 = cache.id_for("/proj/a.anim");
    const u32 b  = cache.id_for("/proj/b.anim");
    EXPECT_NE(a1, 0u);
    EXPECT_EQ(a1, a2);
    EXPECT_NE(a1, b);
    EXPECT_GE(a1, 12288u);  // animation ids start above prefab range (8192+)
}

TEST(AnimationAssetCache, IdForEmptyPathReturnsZero) {
    AnimationAssetCache cache;
    EXPECT_EQ(cache.id_for(""), 0u);
}

TEST(AnimationAssetCache, GetByIdRoundTripsAfterLoad) {
    const fs::path p = tmp_path("nexus_anim_byid.anim");
    if (fs::exists(p)) fs::remove(p);
    AnimationAssetCache writer;
    ASSERT_TRUE(writer.save(p.string(), make_test_clip()));

    AnimationAssetCache cache;
    cache.load(p.string());
    const u32 id = cache.id_for(p.string());
    EXPECT_NE(id, 0u);
    const auto* clip = cache.get_by_id(id);
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->name(), "walk");
    EXPECT_EQ(cache.get_by_id(0xDEADu), nullptr);
    EXPECT_EQ(cache.path_for(id), p.string());

    fs::remove(p);
}

}  // namespace nexus::editor::tests
