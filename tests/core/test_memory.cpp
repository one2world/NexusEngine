#include <gtest/gtest.h>
#include <nexus/core/memory.h>

namespace nexus::tests {

TEST(LinearAllocator, AllocateAndReset) {
    LinearAllocator alloc(1024);
    auto* p1 = alloc.allocate(64);
    auto* p2 = alloc.allocate(128);

    EXPECT_NE(p1, nullptr);
    EXPECT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);

    alloc.reset();
    auto* p3 = alloc.allocate(64);
    // After reset, should reuse space from the start
    EXPECT_EQ(p3, p1);
}

TEST(LinearAllocator, OversizeReturnsNull) {
    LinearAllocator alloc(64);
    auto* p = alloc.allocate(128);
    EXPECT_EQ(p, nullptr);
}

TEST(PoolAllocator, AllocateAndDeallocate) {
    PoolAllocator<sizeof(int)> pool(4);
    auto* p1 = pool.allocate();
    auto* p2 = pool.allocate();

    EXPECT_NE(p1, nullptr);
    EXPECT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);

    pool.deallocate(p1);
    auto* p3 = pool.allocate();
    // Should reuse freed slot
    EXPECT_EQ(p3, p1);
}

TEST(ArenaAllocator, GrowsAutomatically) {
    ArenaAllocator arena(64); // Small initial block
    void* ptrs[10];
    for (int i = 0; i < 10; ++i) {
        ptrs[i] = arena.allocate(32); // Will need multiple blocks
        EXPECT_NE(ptrs[i], nullptr);
    }
}

} // namespace nexus::tests
