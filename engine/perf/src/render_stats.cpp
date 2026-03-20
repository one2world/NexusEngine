#include "nexus/perf/render_stats.h"

#include <cstdio>

namespace nexus {

void RenderStatsCollector::begin_frame() {
    current_ = RenderStats{};
}

void RenderStatsCollector::end_frame(f32 delta_time) {
    current_.frame_time_ms = delta_time * 1000.0f;
    current_.fps = delta_time > 0.0f ? 1.0f / delta_time : 0.0f;

    if (history_.size() >= MAX_HISTORY) {
        history_.erase(history_.begin());
    }
    history_.push_back(current_);
}

void RenderStatsCollector::add_draw_call(u32 triangle_count, u32 vertex_count) {
    current_.draw_calls++;
    current_.triangles += triangle_count;
    current_.vertices += vertex_count;
}

void RenderStatsCollector::add_texture_bind() {
    current_.texture_binds++;
}

void RenderStatsCollector::add_shader_switch() {
    current_.shader_switches++;
}

void RenderStatsCollector::set_gpu_memory(u64 bytes) {
    current_.gpu_memory_used = bytes;
}

const RenderStats* RenderStatsCollector::last_frame() const {
    if (history_.empty()) return nullptr;
    return &history_.back();
}

RenderStats RenderStatsCollector::average() const {
    if (history_.empty()) return {};

    RenderStats avg{};
    for (const auto& s : history_) {
        avg.draw_calls     += s.draw_calls;
        avg.triangles      += s.triangles;
        avg.vertices       += s.vertices;
        avg.texture_binds  += s.texture_binds;
        avg.shader_switches += s.shader_switches;
        avg.fps            += s.fps;
        avg.frame_time_ms  += s.frame_time_ms;
        avg.gpu_memory_used += s.gpu_memory_used;
    }
    auto n = static_cast<f32>(history_.size());
    auto un = static_cast<u32>(history_.size());
    avg.draw_calls     /= un;
    avg.triangles      /= un;
    avg.vertices       /= un;
    avg.texture_binds  /= un;
    avg.shader_switches /= un;
    avg.fps            /= n;
    avg.frame_time_ms  /= n;
    avg.gpu_memory_used /= static_cast<u64>(history_.size());
    return avg;
}

std::string RenderStatsCollector::format_overlay() const {
    const auto* last = last_frame();
    if (!last) return "No render stats";

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "FPS: %.1f | Frame: %.2fms | DC: %u | Tris: %u | Verts: %u | Tex: %u | Shaders: %u | VRAM: %.1fMB",
        last->fps, last->frame_time_ms,
        last->draw_calls, last->triangles, last->vertices,
        last->texture_binds, last->shader_switches,
        static_cast<f32>(last->gpu_memory_used) / (1024.0f * 1024.0f));
    return buf;
}

} // namespace nexus
