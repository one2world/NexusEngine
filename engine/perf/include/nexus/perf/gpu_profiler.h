#pragma once

#include "nexus/core/types.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace nexus {

struct GPUPassTiming {
    std::string name;
    f64 duration_ms{0.0};
    f64 avg_duration_ms{0.0};
    f64 peak_duration_ms{0.0};
    u32 depth{0};
};

struct GPUFrameSummary {
    f64 total_gpu_ms{0.0};
    f64 avg_gpu_ms{0.0};
    f64 peak_gpu_ms{0.0};
    std::vector<GPUPassTiming> passes;
};

class GPUProfiler {
public:
    GPUProfiler() = default;

    void begin_frame();
    void end_frame();
    void begin_pass(const std::string& name);
    void end_pass();
    void record_timing(const std::string& name, f64 duration_us);

    const GPUFrameSummary& summary() const { return current_summary_; }
    const std::vector<GPUFrameSummary>& history() const { return history_; }

    bool enabled() const { return enabled_; }
    void set_enabled(bool e) { enabled_ = e; }

    static constexpr u32 MAX_HISTORY = 120;

private:
    struct PassRecord {
        std::string name;
        f64 duration_us{0.0};
        u32 depth{0};
    };

    struct PassStats {
        f64 total_us{0.0};
        f64 peak_us{0.0};
        u32 sample_count{0};
    };

    bool enabled_{true};
    u32 current_depth_{0};
    std::vector<PassRecord> current_passes_;
    GPUFrameSummary current_summary_;
    std::vector<GPUFrameSummary> history_;
    std::unordered_map<std::string, PassStats> pass_stats_;
};

} // namespace nexus
