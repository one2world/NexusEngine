#include <gtest/gtest.h>
#include "nexus/scene/registry.h"
#include "nexus/scene/components.h"
#include "nexus/scene/hierarchy.h"
#include "nexus/core/math.h"
#include <chrono>
#include <vector>
#include <functional>

using namespace nexus;

namespace {

double measure_us(std::function<void()> fn) {
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto end = std::chrono::high_resolution_clock::now();
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

} // namespace

// ── ECS Performance Regression Tests ─────────────────────────────────────

TEST(PerfRegression, EntityCreation10K) {
    constexpr u32 COUNT = 10000;
    constexpr double THRESHOLD_US = 50000.0;

    Registry reg;
    double elapsed = measure_us([&]() {
        for (u32 i = 0; i < COUNT; ++i) {
            reg.create();
        }
    });

    EXPECT_LT(elapsed, THRESHOLD_US)
        << "Entity creation regression: " << COUNT << " entities took "
        << elapsed / 1000.0 << " ms (threshold: " << THRESHOLD_US / 1000.0 << " ms)";
}

TEST(PerfRegression, EntityCreationAndDestroy10K) {
    constexpr u32 COUNT = 10000;
    constexpr double THRESHOLD_US = 100000.0;

    Registry reg;
    std::vector<Entity> entities;
    entities.reserve(COUNT);

    double elapsed = measure_us([&]() {
        for (u32 i = 0; i < COUNT; ++i) {
            entities.push_back(reg.create());
        }
        for (auto e : entities) {
            reg.destroy(e);
        }
    });

    EXPECT_LT(elapsed, THRESHOLD_US)
        << "Create+Destroy regression: " << elapsed / 1000.0 << " ms";
}

TEST(PerfRegression, ComponentAdd10K) {
    constexpr u32 COUNT = 10000;
    constexpr double THRESHOLD_US = 50000.0;

    Registry reg;
    std::vector<Entity> entities;
    entities.reserve(COUNT);
    for (u32 i = 0; i < COUNT; ++i) {
        entities.push_back(reg.create());
    }

    double elapsed = measure_us([&]() {
        for (auto e : entities) {
            reg.add_component<Transform3DComponent>(e, {});
        }
    });

    EXPECT_LT(elapsed, THRESHOLD_US)
        << "Component add regression: " << elapsed / 1000.0 << " ms";
}

TEST(PerfRegression, ComponentIteration10K) {
    constexpr u32 COUNT = 10000;
    constexpr double THRESHOLD_US = 5000.0;

    Registry reg;
    for (u32 i = 0; i < COUNT; ++i) {
        auto e = reg.create();
        Transform3DComponent t;
        t.position = Vec3(static_cast<f32>(i), 0.0f, 0.0f);
        reg.add_component<Transform3DComponent>(e, t);
    }

    volatile float sum = 0.0f;
    double elapsed = measure_us([&]() {
        auto entities = reg.view<Transform3DComponent>();
        for (auto e : entities) {
            auto& t = reg.get_component<Transform3DComponent>(e);
            sum += t.position.x;
        }
    });

    EXPECT_LT(elapsed, THRESHOLD_US)
        << "Component iteration regression: " << elapsed / 1000.0 << " ms";
    EXPECT_GT(sum, 0.0f);
}

TEST(PerfRegression, HierarchyPropagation1K) {
    constexpr u32 COUNT = 1000;
    constexpr double THRESHOLD_US = 50000.0;

    Registry reg;
    Entity root = reg.create();
    reg.add_component<Transform3DComponent>(root, {});

    for (u32 i = 0; i < COUNT; ++i) {
        auto child = reg.create();
        Transform3DComponent t;
        t.position = Vec3(1.0f, 0.0f, 0.0f);
        reg.add_component<Transform3DComponent>(child, t);
        Hierarchy::set_parent(reg, child, root);
    }

    double elapsed = measure_us([&]() {
        Hierarchy::propagate_transforms_3d(reg);
    });

    EXPECT_LT(elapsed, THRESHOLD_US)
        << "Hierarchy propagation regression: " << elapsed / 1000.0 << " ms";
}

TEST(PerfRegression, VectorMathBatch) {
    constexpr u32 COUNT = 100000;
    constexpr double THRESHOLD_US = 20000.0;

    std::vector<Vec3> positions(COUNT);
    std::vector<Vec3> velocities(COUNT);
    for (u32 i = 0; i < COUNT; ++i) {
        positions[i] = Vec3(static_cast<f32>(i), 0.0f, 0.0f);
        velocities[i] = Vec3(0.0f, 1.0f, 0.0f);
    }

    double elapsed = measure_us([&]() {
        for (u32 i = 0; i < COUNT; ++i) {
            positions[i] += velocities[i] * 0.016f;
        }
    });

    EXPECT_LT(elapsed, THRESHOLD_US)
        << "Vector math regression: " << elapsed / 1000.0 << " ms";
}
