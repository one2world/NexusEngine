#include <gtest/gtest.h>
#include <nexus/core/containers.h>

namespace nexus::tests {

// ── SlotMap tests ───────────────────────────────────────────────────────────

TEST(SlotMap, InsertAndAccess) {
    SlotMap<std::string> map;
    auto key = map.insert("hello");
    ASSERT_TRUE(map.valid(key));
    EXPECT_EQ(*map.get(key), "hello");
}

TEST(SlotMap, MultipleInserts) {
    SlotMap<int> map;
    auto k1 = map.insert(10);
    auto k2 = map.insert(20);
    auto k3 = map.insert(30);

    EXPECT_EQ(*map.get(k1), 10);
    EXPECT_EQ(*map.get(k2), 20);
    EXPECT_EQ(*map.get(k3), 30);
    EXPECT_EQ(map.size(), 3u);
}

TEST(SlotMap, RemoveInvalidatesKey) {
    SlotMap<int> map;
    auto key = map.insert(42);
    ASSERT_TRUE(map.valid(key));
    map.remove(key);
    EXPECT_FALSE(map.valid(key));
    EXPECT_EQ(map.get(key), nullptr);
}

TEST(SlotMap, ReuseSlotAfterRemove) {
    SlotMap<int> map;
    auto k1 = map.insert(10);
    map.remove(k1);
    auto k2 = map.insert(20);

    // Old key should be invalid
    EXPECT_FALSE(map.valid(k1));
    // New key should be valid
    EXPECT_TRUE(map.valid(k2));
    EXPECT_EQ(*map.get(k2), 20);
}

// ── RingBuffer tests ────────────────────────────────────────────────────────

TEST(RingBuffer, PushAndPop) {
    RingBuffer<int> rb(4);
    EXPECT_TRUE(rb.empty());

    rb.push(1);
    rb.push(2);
    rb.push(3);

    EXPECT_EQ(rb.size(), 3u);
    EXPECT_FALSE(rb.empty());

    auto v1 = rb.pop();
    EXPECT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 1);
    auto v2 = rb.pop();
    EXPECT_EQ(*v2, 2);
    auto v3 = rb.pop();
    EXPECT_EQ(*v3, 3);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, Wraparound) {
    RingBuffer<int> rb(3);
    rb.push(1);
    rb.push(2);
    (void)rb.pop(); // remove 1
    rb.push(3);
    rb.push(4); // wraps around

    EXPECT_EQ(rb.size(), 3u);
    EXPECT_EQ(rb.front(), 2);
}

TEST(RingBuffer, FullBuffer) {
    RingBuffer<int> rb(2);
    rb.push(1);
    rb.push(2);
    EXPECT_TRUE(rb.full());
}

// ── SparseSet tests ─────────────────────────────────────────────────────────

TEST(SparseSet, AddAndHas) {
    SparseSet<float> ss;
    ss.add(5, 3.14f);
    ss.add(10, 2.71f);

    EXPECT_TRUE(ss.has(5));
    EXPECT_TRUE(ss.has(10));
    EXPECT_FALSE(ss.has(0));
    EXPECT_EQ(ss.size(), 2u);
}

TEST(SparseSet, GetValue) {
    SparseSet<std::string> ss;
    ss.add(3, "three");
    EXPECT_EQ(ss.get(3), "three");
}

TEST(SparseSet, Remove) {
    SparseSet<int> ss;
    ss.add(1, 10);
    ss.add(2, 20);
    ss.add(3, 30);

    ss.remove(2);
    EXPECT_FALSE(ss.has(2));
    EXPECT_TRUE(ss.has(1));
    EXPECT_TRUE(ss.has(3));
    EXPECT_EQ(ss.size(), 2u);
}

} // namespace nexus::tests
