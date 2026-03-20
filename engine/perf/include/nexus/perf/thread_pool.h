#pragma once

#include <nexus/core/types.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// CommandBucket — a batch of render commands recorded on a worker thread
// ---------------------------------------------------------------------------
struct CommandBucket {
    u32 sort_key = 0;
    std::function<void()> execute;
};

// ---------------------------------------------------------------------------
// ParallelCommandBuffer — collects command buckets from multiple threads
// ---------------------------------------------------------------------------
class ParallelCommandBuffer {
public:
    ParallelCommandBuffer() = default;

    /// Submit a command bucket (thread-safe).
    void submit(CommandBucket bucket);

    /// Sort all buckets by sort_key and execute them sequentially.
    void sort_and_execute();

    /// Number of pending command buckets.
    [[nodiscard]] u32 pending_count() const;

    /// Clear all command buckets.
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<CommandBucket> buckets_;
};

// ---------------------------------------------------------------------------
// RenderThreadPool — worker threads for recording render commands
// ---------------------------------------------------------------------------
class RenderThreadPool {
public:
    explicit RenderThreadPool(u32 thread_count = 0);
    ~RenderThreadPool();

    NEXUS_NON_COPYABLE(RenderThreadPool)
    NEXUS_NON_MOVABLE(RenderThreadPool)

    /// Submit a task that returns void, to be executed on a worker thread.
    std::future<void> submit(std::function<void()> task);

    /// Wait for all submitted tasks to complete.
    void wait_idle();

    /// Number of worker threads.
    [[nodiscard]] u32 thread_count() const { return static_cast<u32>(workers_.size()); }

    /// Number of tasks currently queued.
    [[nodiscard]] u32 queued_tasks() const;

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::packaged_task<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idle_cv_;
    std::atomic<bool> stop_{false};
    std::atomic<u32>  active_tasks_{0};
};

} // namespace nexus
