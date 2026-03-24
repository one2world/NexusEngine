#include <gtest/gtest.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/components.h>

namespace nexus::tests {

TEST(ComponentLifecycle, OnAddedCallbackFires) {
    Registry registry;
    int add_count = 0;

    registry.on_component_added<TagComponent>([&](Entity) {
        add_count++;
    });

    auto e1 = registry.create();
    registry.add_component<TagComponent>(e1, TagComponent{"A"});
    EXPECT_EQ(add_count, 1);

    auto e2 = registry.create();
    registry.add_component<TagComponent>(e2, TagComponent{"B"});
    EXPECT_EQ(add_count, 2);
}

TEST(ComponentLifecycle, OnRemovedCallbackFires) {
    Registry registry;
    int remove_count = 0;

    registry.on_component_removed<TagComponent>([&](Entity) {
        remove_count++;
    });

    auto e = registry.create();
    registry.add_component<TagComponent>(e, TagComponent{"Test"});
    EXPECT_EQ(remove_count, 0);

    registry.remove_component<TagComponent>(e);
    EXPECT_EQ(remove_count, 1);
}

TEST(ComponentLifecycle, OnRemovedFiresOnEntityDestroy) {
    Registry registry;
    int remove_count = 0;

    registry.on_component_removed<TagComponent>([&](Entity) {
        remove_count++;
    });

    auto e = registry.create();
    registry.add_component<TagComponent>(e, TagComponent{"Test"});

    registry.destroy(e);
    EXPECT_EQ(remove_count, 1);
}

TEST(ComponentLifecycle, CallbackReceivesCorrectEntity) {
    Registry registry;
    Entity received{};

    registry.on_component_added<TagComponent>([&](Entity e) {
        received = e;
    });

    auto e = registry.create();
    registry.add_component<TagComponent>(e, TagComponent{"Hello"});
    EXPECT_EQ(received, e);
}

TEST(ComponentLifecycle, NoCallbackNoCrash) {
    // No callbacks registered — should not crash
    Registry registry;
    auto e = registry.create();
    registry.add_component<TagComponent>(e, TagComponent{"OK"});
    registry.remove_component<TagComponent>(e);
}

TEST(ComponentLifecycle, AddExistingDoesNotFireAgain) {
    Registry registry;
    int add_count = 0;

    registry.on_component_added<TagComponent>([&](Entity) {
        add_count++;
    });

    auto e = registry.create();
    registry.add_component<TagComponent>(e, TagComponent{"First"});
    EXPECT_EQ(add_count, 1);

    // Adding again to same entity — overwrites, not a new addition
    registry.add_component<TagComponent>(e, TagComponent{"Second"});
    // Should still be 1 (no new add callback)
    EXPECT_EQ(add_count, 1);
}

} // namespace nexus::tests
