#pragma once

#include <nexus/core/types.h>

#include <string>
#include <vector>

namespace nexus {

// ---------------------------------------------------------------------------
// RenderStats — per-frame rendering statistics
// ---------------------------------------------------------------------------
struct RenderStats {
    u32 draw_calls     = 0;
    u32 triangles      = 0;
    u32 vertices       = 0;
    u32 texture_binds  = 0;
    u32 shader_switches = 0;
    u64 gpu_memory_used = 0;   // bytes
    f32 fps            = 0.0f;
    f32 frame_time_ms  = 0.0f;
};

// ---------------------------------------------------------------------------
// RenderStatsCollector — accumulate stats within a frame, then snapshot
// ---------------------------------------------------------------------------
class RenderStatsCollector {
public:
    RenderStatsCollector() = default;

    /// Reset counters for a new frame.
    void begin_frame();

    /// Finalize and push to history.
    void end_frame(f32 delta_time);

    /// Increment counters during the frame.
    void add_draw_call(u32 triangle_count, u32 vertex_count);
    void add_texture_bind();
    void add_shader_switch();
    void set_gpu_memory(u64 bytes);

    /// Current in-progress stats for the active frame.
    [[nodiscard]] const RenderStats& current() const { return current_; }

    /// Most recent completed frame stats.
    [[nodiscard]] const RenderStats* last_frame() const;

    /// Average over the history buffer.
    [[nodiscard]] RenderStats average() const;

    /// Format a concise overlay string.
    [[nodiscard]] std::string format_overlay() const;

    /// Access the history ring buffer.
    [[nodiscard]] const std::vector<RenderStats>& history() const { return history_; }

    static constexpr u32 MAX_HISTORY = 120;

private:
    RenderStats current_{};
    std::vector<RenderStats> history_;
};

} // namespace nexus
