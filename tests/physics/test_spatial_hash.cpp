#include <gtest/gtest.h>
#include <nexus/physics/spatial_hash.h>

namespace nexus::physics::tests {

TEST(SpatialHash3D, InsertAndQuery) {
    SpatialHash3D hash(2.0f);

    AABB a{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    AABB b{{0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 1.5f}};
    AABB c{{10.0f, 10.0f, 10.0f}, {11.0f, 11.0f, 11.0f}};

    hash.insert(1, a);
    hash.insert(2, b);
    hash.insert(3, c);

    std::unordered_set<u64> pairs;
    std::vector<std::pair<u32, u32>> out;

    // Query around body 1 — should find pair (1,2) but not (1,3)
    hash.query(a, pairs, 1, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].first, 1u);
    EXPECT_EQ(out[0].second, 2u);
}

TEST(SpatialHash3D, NoDuplicatePairs) {
    SpatialHash3D hash(2.0f);

    AABB a{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    AABB b{{0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 1.5f}};

    hash.insert(1, a);
    hash.insert(2, b);

    std::unordered_set<u64> pairs;
    std::vector<std::pair<u32, u32>> out;

    // Query both bodies — pair should appear only once
    hash.query(a, pairs, 1, out);
    hash.query(b, pairs, 2, out);
    EXPECT_EQ(out.size(), 1u);
}

TEST(SpatialHash3D, FarBodiesNoCollision) {
    SpatialHash3D hash(2.0f);

    AABB a{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    AABB b{{100.0f, 100.0f, 100.0f}, {101.0f, 101.0f, 101.0f}};

    hash.insert(1, a);
    hash.insert(2, b);

    std::unordered_set<u64> pairs;
    std::vector<std::pair<u32, u32>> out;
    hash.query(a, pairs, 1, out);
    EXPECT_TRUE(out.empty());
}

TEST(SpatialHash3D, ClearResetsState) {
    SpatialHash3D hash(2.0f);

    AABB a{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    hash.insert(1, a);
    hash.insert(2, a);

    hash.clear();

    std::unordered_set<u64> pairs;
    std::vector<std::pair<u32, u32>> out;
    hash.query(a, pairs, 1, out);
    EXPECT_TRUE(out.empty());
}

TEST(SpatialHash2D, InsertAndQuery) {
    SpatialHash2D hash(2.0f);

    Vec2 a_min{0.0f, 0.0f}, a_max{1.0f, 1.0f};
    Vec2 b_min{0.5f, 0.5f}, b_max{1.5f, 1.5f};
    Vec2 c_min{20.0f, 20.0f}, c_max{21.0f, 21.0f};

    hash.insert(1, a_min, a_max);
    hash.insert(2, b_min, b_max);
    hash.insert(3, c_min, c_max);

    std::unordered_set<u64> pairs;
    std::vector<std::pair<u32, u32>> out;
    hash.query(a_min, a_max, pairs, 1, out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].first, 1u);
    EXPECT_EQ(out[0].second, 2u);
}

TEST(SpatialHash2D, ChangeCellSize) {
    SpatialHash2D hash(100.0f);  // very large cell — everything in one cell

    Vec2 a_min{0.0f, 0.0f}, a_max{1.0f, 1.0f};
    Vec2 b_min{50.0f, 50.0f}, b_max{51.0f, 51.0f};

    hash.insert(1, a_min, a_max);
    hash.insert(2, b_min, b_max);

    std::unordered_set<u64> pairs;
    std::vector<std::pair<u32, u32>> out;
    hash.query(a_min, a_max, pairs, 1, out);

    // With a huge cell, they share the same cell
    EXPECT_EQ(out.size(), 1u);
}

} // namespace nexus::physics::tests
