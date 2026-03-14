#include <gtest/gtest.h>
#include <nexus/scene/system_scheduler.h>

namespace nexus::tests {

TEST(SystemScheduler, ExecutesSystem) {
    SystemScheduler scheduler;
    int called = 0;

    scheduler.add_system("test", [&](Registry&, float) { ++called; });

    Registry reg;
    scheduler.execute(reg, 0.016f);
    EXPECT_EQ(called, 1);
}

TEST(SystemScheduler, ExecutionOrder) {
    SystemScheduler scheduler;
    std::vector<std::string> order;

    scheduler.add_system("physics", [&](Registry&, float) {
        order.push_back("physics");
    });
    scheduler.add_system("render", [&](Registry&, float) {
        order.push_back("render");
    }, {"physics"});

    Registry reg;
    scheduler.execute(reg, 0.016f);

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "physics");
    EXPECT_EQ(order[1], "render");
}

TEST(SystemScheduler, MultiLevelDependencies) {
    SystemScheduler scheduler;
    std::vector<std::string> order;

    scheduler.add_system("input", [&](Registry&, float) { order.push_back("input"); });
    scheduler.add_system("physics", [&](Registry&, float) { order.push_back("physics"); }, {"input"});
    scheduler.add_system("render", [&](Registry&, float) { order.push_back("render"); }, {"physics"});

    Registry reg;
    scheduler.execute(reg, 0.016f);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "input");
    EXPECT_EQ(order[1], "physics");
    EXPECT_EQ(order[2], "render");
}

TEST(SystemScheduler, DisableSystem) {
    SystemScheduler scheduler;
    int physics_calls = 0;
    int render_calls = 0;

    scheduler.add_system("physics", [&](Registry&, float) { ++physics_calls; });
    scheduler.add_system("render", [&](Registry&, float) { ++render_calls; });

    scheduler.set_enabled("physics", false);

    Registry reg;
    scheduler.execute(reg, 0.016f);

    EXPECT_EQ(physics_calls, 0);
    EXPECT_EQ(render_calls, 1);
}

TEST(SystemScheduler, RemoveSystem) {
    SystemScheduler scheduler;
    int called = 0;

    scheduler.add_system("test", [&](Registry&, float) { ++called; });
    scheduler.remove_system("test");

    Registry reg;
    scheduler.execute(reg, 0.016f);
    EXPECT_EQ(called, 0);
}

TEST(SystemScheduler, GetExecutionOrder) {
    SystemScheduler scheduler;
    scheduler.add_system("a", [](Registry&, float) {});
    scheduler.add_system("b", [](Registry&, float) {}, {"a"});
    scheduler.add_system("c", [](Registry&, float) {}, {"b"});

    // Force rebuild by executing
    Registry reg;
    scheduler.execute(reg, 0.016f);

    auto order = scheduler.get_execution_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "a");
    EXPECT_EQ(order[1], "b");
    EXPECT_EQ(order[2], "c");
}

TEST(SystemScheduler, PassesDeltaTime) {
    SystemScheduler scheduler;
    float received_dt = 0.0f;

    scheduler.add_system("test", [&](Registry&, float dt) {
        received_dt = dt;
    });

    Registry reg;
    scheduler.execute(reg, 0.042f);
    EXPECT_FLOAT_EQ(received_dt, 0.042f);
}

} // namespace nexus::tests
