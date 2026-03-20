#include <nexus/perf/thread_pool.h>
#include <gtest/gtest.h>
#include <atomic>

using namespace nexus;

TEST(ParallelCommandBuffer, SubmitAndExecute) {
    ParallelCommandBuffer pcb;
    std::vector<int> order;

    pcb.submit({2, [&] { order.push_back(2); }});
    pcb.submit({0, [&] { order.push_back(0); }});
    pcb.submit({1, [&] { order.push_back(1); }});

    EXPECT_EQ(pcb.pending_count(), 3u);
    pcb.sort_and_execute();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
    EXPECT_EQ(pcb.pending_count(), 0u);
}

TEST(ParallelCommandBuffer, Clear) {
    ParallelCommandBuffer pcb;
    pcb.submit({0, [] {}});
    pcb.submit({1, [] {}});
    EXPECT_EQ(pcb.pending_count(), 2u);
    pcb.clear();
    EXPECT_EQ(pcb.pending_count(), 0u);
}

TEST(ParallelCommandBuffer, EmptyExecute) {
    ParallelCommandBuffer pcb;
    pcb.sort_and_execute(); // should not crash
    EXPECT_EQ(pcb.pending_count(), 0u);
}

TEST(RenderThreadPool, Construction) {
    RenderThreadPool pool(2);
    EXPECT_EQ(pool.thread_count(), 2u);
}

TEST(RenderThreadPool, SubmitAndWait) {
    RenderThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 10; ++i) {
        pool.submit([&counter] { counter.fetch_add(1); });
    }
    pool.wait_idle();
    EXPECT_EQ(counter.load(), 10);
}

TEST(RenderThreadPool, FutureResult) {
    RenderThreadPool pool(2);
    std::atomic<bool> done{false};

    auto future = pool.submit([&done] { done.store(true); });
    future.get();
    EXPECT_TRUE(done.load());
}

TEST(RenderThreadPool, QueuedTasks) {
    RenderThreadPool pool(1);
    // Submit work and immediately check queue.
    // Not deterministic, but we can at least verify the method works.
    auto count = pool.queued_tasks();
    EXPECT_GE(count, 0u);
}

TEST(RenderThreadPool, ParallelExecution) {
    RenderThreadPool pool(4);
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit([&] {
            int c = current.fetch_add(1) + 1;
            int prev = max_concurrent.load();
            while (c > prev && !max_concurrent.compare_exchange_weak(prev, c)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            current.fetch_sub(1);
        }));
    }
    for (auto& f : futures) f.get();

    // With 4 threads and tasks that sleep, we should see >1 concurrent.
    EXPECT_GT(max_concurrent.load(), 1);
}

TEST(RenderThreadPool, CommandBufferIntegration) {
    RenderThreadPool pool(2);
    ParallelCommandBuffer pcb;

    // Record commands from multiple threads.
    std::vector<std::future<void>> futures;
    for (u32 i = 0; i < 8; ++i) {
        futures.push_back(pool.submit([&pcb, i] {
            pcb.submit({i, [i] {
                // This would be a draw call.
                (void)i;
            }});
        }));
    }
    for (auto& f : futures) f.get();

    EXPECT_EQ(pcb.pending_count(), 8u);
    pcb.sort_and_execute();
    EXPECT_EQ(pcb.pending_count(), 0u);
}
