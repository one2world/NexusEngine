#include <gtest/gtest.h>
#include <nexus/core/job_system.h>
#include <nexus/core/types.h>
#include <atomic>

namespace nexus::tests {

TEST(JobSystem, SubmitAndGet) {
    JobSystem jobs(2);
    auto future = jobs.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST(JobSystem, MultipleSubmits) {
    JobSystem jobs(2);
    std::vector<std::future<int>> futures;

    for (u32 i = 0; i < 100; ++i) {
        futures.push_back(jobs.submit([i]() { return static_cast<int>(i * 2); }));
    }

    for (u32 i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[i].get(), static_cast<int>(i * 2));
    }
}

TEST(JobSystem, ParallelFor) {
    JobSystem jobs(4);
    std::atomic<int> sum{0};

    jobs.parallel_for(100, [&](nexus::u32 index) {
        sum += static_cast<int>(index);
    });

    // Sum of 0..99 = 4950
    EXPECT_EQ(sum.load(), 4950);
}

TEST(JobSystem, ThreadCount) {
    JobSystem jobs(3);
    EXPECT_EQ(jobs.thread_count(), 3u);
}

TEST(JobSystem, VoidTasks) {
    JobSystem jobs(2);
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    for (u32 i = 0; i < 50; ++i) {
        futures.push_back(jobs.submit([&]() { ++counter; }));
    }

    for (auto& f : futures) f.get();
    EXPECT_EQ(counter.load(), 50);
}

} // namespace nexus::tests
