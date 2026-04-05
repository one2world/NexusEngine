#include "nexus/perf/gpu_profiler.h"
#include <algorithm>

namespace nexus {

void GPUProfiler::begin_frame() {
    if (!enabled_) return;
    current_passes_.clear();
    current_depth_ = 0;
}

void GPUProfiler::end_frame() {
    if (!enabled_) return;

    GPUFrameSummary summary;
    summary.total_gpu_ms = 0.0;

    for (const auto& pass : current_passes_) {
        f64 ms = pass.duration_us / 1000.0;
        summary.total_gpu_ms += ms;

        auto& stats = pass_stats_[pass.name];
        stats.total_us += pass.duration_us;
        ++stats.sample_count;
        if (pass.duration_us > stats.peak_us) {
            stats.peak_us = pass.duration_us;
        }

        GPUPassTiming timing;
        timing.name = pass.name;
        timing.duration_ms = ms;
        timing.depth = pass.depth;
        timing.avg_duration_ms = (stats.total_us / static_cast<f64>(stats.sample_count)) / 1000.0;
        timing.peak_duration_ms = stats.peak_us / 1000.0;
        summary.passes.push_back(std::move(timing));
    }

    // Compute frame-level averages
    f64 total_sum = 0.0;
    f64 peak = 0.0;
    for (const auto& h : history_) {
        total_sum += h.total_gpu_ms;
        if (h.total_gpu_ms > peak) peak = h.total_gpu_ms;
    }
    total_sum += summary.total_gpu_ms;
    if (summary.total_gpu_ms > peak) peak = summary.total_gpu_ms;

    u32 count = static_cast<u32>(history_.size()) + 1;
    summary.avg_gpu_ms = total_sum / static_cast<f64>(count);
    summary.peak_gpu_ms = peak;

    current_summary_ = summary;
    history_.push_back(std::move(summary));
    if (history_.size() > MAX_HISTORY) {
        history_.erase(history_.begin());
    }
}

void GPUProfiler::begin_pass(const std::string& /*name*/) {
    if (!enabled_) return;
    ++current_depth_;
}

void GPUProfiler::end_pass() {
    if (!enabled_) return;
    if (current_depth_ > 0) --current_depth_;
}

void GPUProfiler::record_timing(const std::string& name, f64 duration_us) {
    if (!enabled_) return;
    PassRecord record;
    record.name = name;
    record.duration_us = duration_us;
    record.depth = current_depth_;
    current_passes_.push_back(std::move(record));
}

} // namespace nexus
