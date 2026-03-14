#pragma once

#include "nexus/core/types.h"
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <atomic>

namespace nexus {

// ---------------------------------------------------------------------------
// JobSystem - thread pool with task submission and dependencies
// ---------------------------------------------------------------------------
class JobSystem {
public:
    NEXUS_NON_COPYABLE(JobSystem)
    NEXUS_NON_MOVABLE(JobSystem)

    /// Create a job system with the given number of worker threads.
    /// If thread_count is 0, uses hardware_concurrency - 1 (at least 1).
    explicit JobSystem(u32 thread_count = 0);
    ~JobSystem();

    /// Submit a task and get a future for its result.
    template <typename F>
    auto submit(F&& task) -> std::future<decltype(task())> {
        using ReturnType = decltype(task());

        auto packaged = std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<F>(task));

        auto future = packaged->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++pending_;
            tasks_.emplace([packaged]() { (*packaged)(); });
        }

        condition_.notify_one();
        return future;
    }

    /// Submit a batch of tasks and wait for all of them to complete.
    void parallel_for(u32 count, const std::function<void(u32 index)>& task);

    /// Wait for all submitted tasks to finish.
    void wait_idle();

    /// Number of worker threads.
    [[nodiscard]] u32 thread_count() const { return static_cast<u32>(workers_.size()); }

    /// Number of tasks currently queued (approximate).
    [[nodiscard]] u32 pending_count() const { return pending_.load(); }

    /// Get the global job system instance (created on first call).
    static JobSystem& instance();

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable idle_condition_;
    std::atomic<bool> stop_{false};
    std::atomic<u32> pending_{0};
};

} // namespace nexus
