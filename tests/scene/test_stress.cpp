#include <gtest/gtest.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>
#include <chrono>

namespace nexus::tests {

// ── Stress: 10K entities ──────────────────────────────────────────────────

TEST(StressTest, Create10KEntities) {
    Registry registry;
    constexpr std::size_t COUNT = 10000;

    for (std::size_t i = 0; i < COUNT; ++i) {
        auto e = registry.create();
        registry.add_component<Transform3DComponent>(e, Transform3DComponent{});
        EXPECT_TRUE(registry.alive(e));
    }

    EXPECT_EQ(registry.size(), COUNT);
}

TEST(StressTest, Create10KEntitiesWithMultipleComponents) {
    Registry registry;
    constexpr std::size_t COUNT = 10000;

    for (std::size_t i = 0; i < COUNT; ++i) {
        auto e = registry.create();
        registry.add_component<TagComponent>(e, TagComponent{"entity_" + std::to_string(i)});
        registry.add_component<Transform3DComponent>(e, Transform3DComponent{});
        registry.add_component<ActiveComponent>(e, ActiveComponent{});
    }

    EXPECT_EQ(registry.size(), COUNT);
}

TEST(StressTest, CreateAndDestroyHalf) {
    Registry registry;
    constexpr std::size_t COUNT = 10000;
    std::vector<Entity> entities;
    entities.reserve(COUNT);

    for (std::size_t i = 0; i < COUNT; ++i) {
        auto e = registry.create();
        registry.add_component<Transform3DComponent>(e, Transform3DComponent{});
        entities.push_back(e);
    }

    EXPECT_EQ(registry.size(), COUNT);

    // Destroy even-indexed entities
    for (std::size_t i = 0; i < COUNT; i += 2) {
        registry.destroy(entities[i]);
    }

    EXPECT_EQ(registry.size(), COUNT / 2);

    // Verify remaining entities are alive
    for (std::size_t i = 1; i < COUNT; i += 2) {
        EXPECT_TRUE(registry.alive(entities[i]));
    }
}

TEST(StressTest, ComponentIterationPerformance) {
    Registry registry;
    constexpr std::size_t COUNT = 10000;

    for (std::size_t i = 0; i < COUNT; ++i) {
        auto e = registry.create();
        Transform3DComponent t;
        t.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
        registry.add_component<Transform3DComponent>(e, t);
    }

    // Iterate all transforms
    auto start = std::chrono::high_resolution_clock::now();
    auto entities = registry.view<Transform3DComponent>();
    float sum = 0.0f;
    for (auto e : entities) {
        auto& t = registry.get_component<Transform3DComponent>(e);
        sum += t.position.x;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Should be fast — less than 50ms for 10K iterations
    EXPECT_LT(us, 50000);
    EXPECT_GT(sum, 0.0f);
}

TEST(StressTest, RapidCreateDestroy) {
    Registry registry;
    constexpr std::size_t ITERATIONS = 500;

    for (std::size_t i = 0; i < ITERATIONS; ++i) {
        auto e = registry.create();
        registry.add_component<TagComponent>(e, TagComponent{"temp"});
        registry.add_component<Transform3DComponent>(e, Transform3DComponent{});
        registry.destroy(e);
    }

    EXPECT_EQ(registry.size(), static_cast<std::size_t>(0));
}

} // namespace nexus::tests
