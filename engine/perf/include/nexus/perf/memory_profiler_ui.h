#pragma once

#include "nexus/perf/memory_tracker.h"
#include <vector>
#include <string>

namespace nexus {

struct MemorySnapshot {
    struct CategoryInfo {
        std::string name;
        std::size_t current_bytes{0};
        std::size_t peak_bytes{0};
        u64 allocation_count{0};
        f32 percentage{0.0f};
    };

    std::vector<CategoryInfo> categories;
    std::size_t total_current{0};
    std::size_t total_peak{0};
    u64 total_live{0};
};

class MemoryProfilerUI {
public:
    explicit MemoryProfilerUI(const MemoryTracker& tracker);

    void update();
    const MemorySnapshot& snapshot() const { return current_; }
    const std::vector<MemorySnapshot>& history() const { return history_; }
    static std::string format_bytes(std::size_t bytes);

    static constexpr u32 MAX_HISTORY = 120;

private:
    const MemoryTracker& tracker_;
    MemorySnapshot current_;
    std::vector<MemorySnapshot> history_;
};

} // namespace nexus
