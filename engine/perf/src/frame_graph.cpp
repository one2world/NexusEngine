#include "nexus/perf/frame_graph.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <queue>

namespace nexus {

u32 FrameGraph::add_resource(const std::string& name, FrameResourceType type,
                              u32 width, u32 height) {
    u32 id = static_cast<u32>(resources_.size());
    FrameResource res;
    res.name = name;
    res.type = type;
    res.width = width;
    res.height = height;
    resources_.push_back(std::move(res));
    return id;
}

u32 FrameGraph::add_pass(const std::string& name) {
    u32 id = static_cast<u32>(passes_.size());
    FramePass pass;
    pass.name = name;
    pass.id = id;
    passes_.push_back(std::move(pass));
    return id;
}

void FrameGraph::pass_reads(u32 pass_id, u32 resource_id) {
    if (pass_id >= passes_.size() || resource_id >= resources_.size()) return;
    FramePass::ResourceRef ref;
    ref.resource_id = resource_id;
    ref.access = FrameResourceAccess::Read;
    passes_[pass_id].inputs.push_back(ref);
}

void FrameGraph::pass_writes(u32 pass_id, u32 resource_id) {
    if (pass_id >= passes_.size() || resource_id >= resources_.size()) return;
    FramePass::ResourceRef ref;
    ref.resource_id = resource_id;
    ref.access = FrameResourceAccess::Write;
    passes_[pass_id].outputs.push_back(ref);
}

void FrameGraph::set_pass_execute(u32 pass_id, FramePass::ExecuteFn fn) {
    if (pass_id >= passes_.size()) return;
    passes_[pass_id].execute = std::move(fn);
}

void FrameGraph::mark_output(u32 resource_id) {
    output_resources_.insert(resource_id);
}

bool FrameGraph::compile() {
    compiled_ = false;
    exec_order_.clear();

    // Reset culled state
    for (auto& p : passes_) {
        p.culled = false;
    }

    // Cull passes that don't contribute to any output
    cull_unused_passes();

    // Topological sort
    if (!topological_sort()) {
        NX_ERROR("FrameGraph: cycle detected during compilation");
        return false;
    }

    compiled_ = true;
    NX_INFO("FrameGraph: compiled {} passes ({} active, {} culled)",
            passes_.size(), active_pass_count(),
            static_cast<u32>(passes_.size()) - active_pass_count());
    return true;
}

void FrameGraph::cull_unused_passes() {
    if (output_resources_.empty()) return;

    // Work backwards from output resources to find all needed passes
    std::unordered_set<u32> needed_passes;
    std::unordered_set<u32> needed_resources = output_resources_;
    std::queue<u32> work_queue;

    for (u32 res_id : output_resources_) {
        work_queue.push(res_id);
    }

    while (!work_queue.empty()) {
        u32 res_id = work_queue.front();
        work_queue.pop();

        // Find passes that write to this resource
        for (u32 pi = 0; pi < static_cast<u32>(passes_.size()); ++pi) {
            if (needed_passes.count(pi)) continue;

            for (const auto& out : passes_[pi].outputs) {
                if (out.resource_id == res_id) {
                    needed_passes.insert(pi);

                    // This pass's inputs also become needed
                    for (const auto& in : passes_[pi].inputs) {
                        if (!needed_resources.count(in.resource_id)) {
                            needed_resources.insert(in.resource_id);
                            work_queue.push(in.resource_id);
                        }
                    }
                    break;
                }
            }
        }
    }

    // Mark unneeded passes as culled
    for (u32 pi = 0; pi < static_cast<u32>(passes_.size()); ++pi) {
        if (!needed_passes.count(pi)) {
            passes_[pi].culled = true;
        }
    }
}

bool FrameGraph::topological_sort() {
    u32 n = static_cast<u32>(passes_.size());
    if (n == 0) return true;

    // Build adjacency: pass A -> pass B if A writes a resource that B reads
    // Map: resource_id -> producing pass id
    std::unordered_map<u32, u32> resource_producer;
    for (u32 pi = 0; pi < n; ++pi) {
        if (passes_[pi].culled || !passes_[pi].enabled) continue;
        for (const auto& out : passes_[pi].outputs) {
            resource_producer[out.resource_id] = pi;
        }
    }

    // Build in-degree and adjacency list
    std::vector<u32> in_degree(n, 0);
    std::vector<std::vector<u32>> adj(n);

    for (u32 pi = 0; pi < n; ++pi) {
        if (passes_[pi].culled || !passes_[pi].enabled) continue;
        for (const auto& in : passes_[pi].inputs) {
            auto it = resource_producer.find(in.resource_id);
            if (it != resource_producer.end() && it->second != pi) {
                adj[it->second].push_back(pi);
                ++in_degree[pi];
            }
        }
    }

    // Kahn's algorithm
    std::queue<u32> q;
    for (u32 pi = 0; pi < n; ++pi) {
        if (passes_[pi].culled || !passes_[pi].enabled) continue;
        if (in_degree[pi] == 0) {
            q.push(pi);
        }
    }

    exec_order_.clear();
    while (!q.empty()) {
        u32 pi = q.front();
        q.pop();
        exec_order_.push_back(pi);

        for (u32 neighbor : adj[pi]) {
            if (in_degree[neighbor] > 0) {
                --in_degree[neighbor];
                if (in_degree[neighbor] == 0) {
                    q.push(neighbor);
                }
            }
        }
    }

    // Check for cycles
    u32 active = 0;
    for (u32 pi = 0; pi < n; ++pi) {
        if (!passes_[pi].culled && passes_[pi].enabled) ++active;
    }

    return exec_order_.size() == active;
}

void FrameGraph::execute() {
    if (!compiled_) {
        NX_WARN("FrameGraph: execute() called before compile()");
        return;
    }

    for (u32 pass_id : exec_order_) {
        auto& p = passes_[pass_id];
        if (!p.execute) continue;

        auto start = std::chrono::high_resolution_clock::now();
        p.execute();
        auto end = std::chrono::high_resolution_clock::now();

        f64 us = static_cast<f64>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        p.last_duration_us = us;

        // Running average (exponential moving average, alpha = 0.1)
        if (p.avg_duration_us == 0.0) {
            p.avg_duration_us = us;
        } else {
            p.avg_duration_us = p.avg_duration_us * 0.9 + us * 0.1;
        }
    }
}

void FrameGraph::reset() {
    compiled_ = false;
    exec_order_.clear();
    for (auto& p : passes_) {
        p.culled = false;
    }
}

void FrameGraph::clear() {
    passes_.clear();
    resources_.clear();
    output_resources_.clear();
    exec_order_.clear();
    compiled_ = false;
}

u32 FrameGraph::active_pass_count() const {
    u32 count = 0;
    for (const auto& p : passes_) {
        if (!p.culled && p.enabled) ++count;
    }
    return count;
}

std::string FrameGraph::dump() const {
    std::ostringstream ss;
    ss << "=== Frame Graph ===\n";
    ss << "Passes: " << passes_.size() << " (" << active_pass_count() << " active)\n";
    ss << "Resources: " << resources_.size() << "\n\n";

    ss << "--- Resources ---\n";
    for (u32 i = 0; i < static_cast<u32>(resources_.size()); ++i) {
        const auto& r = resources_[i];
        const char* type_str = "Unknown";
        switch (r.type) {
        case FrameResourceType::Texture: type_str = "Texture"; break;
        case FrameResourceType::Buffer: type_str = "Buffer"; break;
        case FrameResourceType::Framebuffer: type_str = "Framebuffer"; break;
        }
        ss << "  [" << i << "] " << r.name << " (" << type_str;
        if (r.width > 0) ss << " " << r.width << "x" << r.height;
        ss << ")\n";
    }

    ss << "\n--- Passes ---\n";
    for (u32 i = 0; i < static_cast<u32>(passes_.size()); ++i) {
        const auto& p = passes_[i];
        ss << "  [" << i << "] " << p.name;
        if (p.culled) ss << " [CULLED]";
        if (!p.enabled) ss << " [DISABLED]";
        ss << "\n";

        if (!p.inputs.empty()) {
            ss << "       reads: ";
            for (u32 j = 0; j < static_cast<u32>(p.inputs.size()); ++j) {
                if (j > 0) ss << ", ";
                ss << resources_[p.inputs[j].resource_id].name;
            }
            ss << "\n";
        }
        if (!p.outputs.empty()) {
            ss << "       writes: ";
            for (u32 j = 0; j < static_cast<u32>(p.outputs.size()); ++j) {
                if (j > 0) ss << ", ";
                ss << resources_[p.outputs[j].resource_id].name;
            }
            ss << "\n";
        }
        if (p.avg_duration_us > 0.0) {
            ss << "       avg: " << p.avg_duration_us / 1000.0 << " ms\n";
        }
    }

    if (!exec_order_.empty()) {
        ss << "\n--- Execution Order ---\n  ";
        for (u32 i = 0; i < static_cast<u32>(exec_order_.size()); ++i) {
            if (i > 0) ss << " -> ";
            ss << passes_[exec_order_[i]].name;
        }
        ss << "\n";
    }

    return ss.str();
}

} // namespace nexus
