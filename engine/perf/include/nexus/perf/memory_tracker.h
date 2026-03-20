#pragma once

#include <nexus/core/types.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// AllocationRecord — metadata about one live allocation
// ---------------------------------------------------------------------------
struct AllocationRecord {
    void*       address  = nullptr;
    std::size_t size     = 0;
    std::string tag;       // category / subsystem label
};

// ---------------------------------------------------------------------------
// AllocationStats — aggregate per-tag statistics
// ---------------------------------------------------------------------------
struct AllocationStats {
    std::string tag;
    std::size_t current_bytes       = 0;
    std::size_t peak_bytes          = 0;
    u64         total_allocations   = 0;
    u64         total_deallocations = 0;
};

// ---------------------------------------------------------------------------
// MemoryTracker — allocation stats and leak detection
// ---------------------------------------------------------------------------
class MemoryTracker {
public:
    MemoryTracker() = default;

    /// Record an allocation with an optional tag.
    void record_alloc(void* ptr, std::size_t size, const std::string& tag = "default");

    /// Record a deallocation.
    void record_free(void* ptr);

    /// Get stats for a specific tag.
    [[nodiscard]] AllocationStats stats_for(const std::string& tag) const;

    /// Get stats for all tags.
    [[nodiscard]] std::vector<AllocationStats> all_stats() const;

    /// Return all currently live allocations (potential leaks).
    [[nodiscard]] std::vector<AllocationRecord> live_allocations() const;

    /// Total bytes currently allocated across all tags.
    [[nodiscard]] std::size_t total_allocated() const;

    /// Total peak bytes across all tags.
    [[nodiscard]] std::size_t total_peak() const;

    /// Number of live allocations.
    [[nodiscard]] u64 live_count() const;

    /// Reset all tracking state.
    void reset();

private:
    mutable std::mutex mutex_;

    // Live allocations keyed by address.
    std::unordered_map<uintptr_t, AllocationRecord> live_;

    // Per-tag aggregated stats.
    std::unordered_map<std::string, AllocationStats> tag_stats_;
};

} // namespace nexus
