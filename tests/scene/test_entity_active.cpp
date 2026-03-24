#include <gtest/gtest.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>

namespace nexus::tests {

TEST(EntityActive, DefaultIsActive) {
    Registry registry;
    auto e = registry.create();
    // Without ActiveComponent, is_active defaults to true
    EXPECT_TRUE(registry.is_active(e));
}

TEST(EntityActive, SetInactive) {
    Registry registry;
    auto e = registry.create();
    registry.add_component<ActiveComponent>(e, ActiveComponent{true});

    registry.set_active(e, false);
    EXPECT_FALSE(registry.is_active(e));
}

TEST(EntityActive, SetActiveToggle) {
    Registry registry;
    auto e = registry.create();
    registry.add_component<ActiveComponent>(e, ActiveComponent{true});

    registry.set_active(e, false);
    EXPECT_FALSE(registry.is_active(e));

    registry.set_active(e, true);
    EXPECT_TRUE(registry.is_active(e));
}

TEST(EntityActive, DeadEntityNotActive) {
    Registry registry;
    auto e = registry.create();
    registry.destroy(e);
    EXPECT_FALSE(registry.is_active(e));
}

TEST(EntityActive, SetActiveCreatesComponentIfMissing) {
    Registry registry;
    auto e = registry.create();
    // No ActiveComponent yet

    registry.set_active(e, false);
    EXPECT_TRUE(registry.has_component<ActiveComponent>(e));
    EXPECT_FALSE(registry.is_active(e));
}

TEST(EntityActive, MultipleEntitiesIndependent) {
    Registry registry;
    auto e1 = registry.create();
    auto e2 = registry.create();
    registry.add_component<ActiveComponent>(e1, ActiveComponent{true});
    registry.add_component<ActiveComponent>(e2, ActiveComponent{true});

    registry.set_active(e1, false);
    EXPECT_FALSE(registry.is_active(e1));
    EXPECT_TRUE(registry.is_active(e2));
}

} // namespace nexus::tests
