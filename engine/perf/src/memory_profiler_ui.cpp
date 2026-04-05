#include "nexus/perf/memory_profiler_ui.h"
#include <cstdio>

namespace nexus {

MemoryProfilerUI::MemoryProfilerUI(const MemoryTracker& tracker)
    : tracker_(tracker) {}

void MemoryProfilerUI::update() {
    auto stats = tracker_.all_stats();

    current_ = {};
    current_.total_current = tracker_.total_allocated();
    current_.total_peak = tracker_.total_peak();
    current_.total_live = tracker_.live_count();

    for (const auto& s : stats) {
        MemorySnapshot::CategoryInfo info;
        info.name = s.tag;
        info.current_bytes = s.current_bytes;
        info.peak_bytes = s.peak_bytes;
        info.allocation_count = s.total_allocations;
        info.percentage = (current_.total_current > 0)
            ? static_cast<f32>(s.current_bytes) / static_cast<f32>(current_.total_current) * 100.0f
            : 0.0f;
        current_.categories.push_back(std::move(info));
    }

    history_.push_back(current_);
    if (history_.size() > MAX_HISTORY) {
        history_.erase(history_.begin());
    }
}

std::string MemoryProfilerUI::format_bytes(std::size_t bytes) {
    char buf[64];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

} // namespace nexus
