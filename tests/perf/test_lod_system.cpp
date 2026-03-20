#include <nexus/perf/lod_system.h>
#include <gtest/gtest.h>

using namespace nexus;

TEST(LODGroup, SelectLevel) {
    LODGroup g;
    g.add_level(10.0f, 1000, 0);
    g.add_level(25.0f, 500, 1);
    g.add_level(50.0f, 100, 2);
    g.cull_distance = 80.0f;

    auto* lv = g.select(5.0f);
    ASSERT_NE(lv, nullptr);
    EXPECT_EQ(lv->mesh_id, 0u);

    lv = g.select(15.0f);
    ASSERT_NE(lv, nullptr);
    EXPECT_EQ(lv->mesh_id, 1u);

    lv = g.select(30.0f);
    ASSERT_NE(lv, nullptr);
    EXPECT_EQ(lv->mesh_id, 2u);
}

TEST(LODGroup, SelectBeyondLastLevel) {
    LODGroup g;
    g.add_level(10.0f, 1000, 0);
    g.add_level(25.0f, 500, 1);
    g.cull_distance = 100.0f;

    // Beyond max_distance of last level but within cull distance.
    auto* lv = g.select(50.0f);
    ASSERT_NE(lv, nullptr);
    EXPECT_EQ(lv->mesh_id, 1u); // should use last level
}

TEST(LODGroup, CullDistance) {
    LODGroup g;
    g.add_level(10.0f, 1000, 0);
    g.cull_distance = 20.0f;

    auto* lv = g.select(25.0f);
    EXPECT_EQ(lv, nullptr);
}

TEST(LODGroup, NoCullDistance) {
    LODGroup g;
    g.add_level(10.0f, 1000, 0);
    // cull_distance = 0 means no culling.
    auto* lv = g.select(100.0f);
    ASSERT_NE(lv, nullptr);
    EXPECT_EQ(lv->mesh_id, 0u);
}

TEST(LODGroup, EmptyGroup) {
    LODGroup g;
    EXPECT_EQ(g.select(5.0f), nullptr);
}

TEST(LODGroup, AddLevelSorted) {
    LODGroup g;
    g.add_level(50.0f, 100, 2);
    g.add_level(10.0f, 1000, 0);
    g.add_level(25.0f, 500, 1);

    EXPECT_EQ(g.levels[0].max_distance, 10.0f);
    EXPECT_EQ(g.levels[1].max_distance, 25.0f);
    EXPECT_EQ(g.levels[2].max_distance, 50.0f);
    EXPECT_EQ(g.levels[0].level, 0u);
    EXPECT_EQ(g.levels[1].level, 1u);
    EXPECT_EQ(g.levels[2].level, 2u);
}

TEST(LODEvaluator, RegisterAndFind) {
    LODEvaluator eval;
    LODGroup g;
    g.name = "Tree";
    g.add_level(10.0f, 1000, 0);
    u32 id = eval.register_group(std::move(g));

    EXPECT_EQ(id, 0u);
    EXPECT_EQ(eval.group_count(), 1u);
    auto* found = eval.find_group(id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "Tree");
}

TEST(LODEvaluator, FindInvalid) {
    LODEvaluator eval;
    EXPECT_EQ(eval.find_group(999), nullptr);
}

TEST(LODEvaluator, Evaluate) {
    LODEvaluator eval;

    LODGroup g;
    g.add_level(10.0f, 1000, 10);
    g.add_level(30.0f, 500, 11);
    g.cull_distance = 50.0f;
    u32 gid = eval.register_group(std::move(g));

    Vec3 camera{0, 0, 0};
    std::vector<std::pair<u32, Vec3>> objects = {
        {gid, {5, 0, 0}},   // distance 5 → LOD 0
        {gid, {20, 0, 0}},  // distance 20 → LOD 1
        {gid, {60, 0, 0}},  // distance 60 → culled
    };

    auto results = eval.evaluate(camera, objects);
    ASSERT_EQ(results.size(), 3u);

    EXPECT_FALSE(results[0].culled);
    EXPECT_EQ(results[0].mesh_id, 10u);
    EXPECT_EQ(results[0].lod_level, 0u);

    EXPECT_FALSE(results[1].culled);
    EXPECT_EQ(results[1].mesh_id, 11u);
    EXPECT_EQ(results[1].lod_level, 1u);

    EXPECT_TRUE(results[2].culled);
}

TEST(LODEvaluator, Bias) {
    LODEvaluator eval;
    eval.set_bias(2.0f);
    EXPECT_FLOAT_EQ(eval.bias(), 2.0f);

    LODGroup g;
    g.add_level(10.0f, 1000, 10);
    g.add_level(30.0f, 500, 11);
    g.cull_distance = 50.0f;
    u32 gid = eval.register_group(std::move(g));

    Vec3 camera{0, 0, 0};
    // Actual distance 8, biased distance 16 → should be LOD 1 (>10).
    auto results = eval.evaluate(camera, {{gid, {8, 0, 0}}});
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].lod_level, 1u);
}

TEST(LODEvaluator, InvalidGroupId) {
    LODEvaluator eval;
    auto results = eval.evaluate({0, 0, 0}, {{999, {1, 0, 0}}});
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].culled);
}
