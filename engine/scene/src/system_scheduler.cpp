#include "nexus/scene/system_scheduler.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <queue>

namespace nexus {

void SystemScheduler::add_system(const std::string& name,
                                 std::function<void(Registry&, float)> fn,
                                 std::vector<std::string> dependencies) {
    // Check for duplicate
    for (auto& s : systems_) {
        if (s.name == name) {
            NX_WARN("SystemScheduler: system '{}' already registered, replacing", name);
            s.execute = std::move(fn);
            s.run_after = std::move(dependencies);
            dirty_ = true;
            return;
        }
    }

    systems_.push_back({name, std::move(fn), std::move(dependencies), true});
    dirty_ = true;
}

void SystemScheduler::remove_system(const std::string& name) {
    systems_.erase(
        std::remove_if(systems_.begin(), systems_.end(),
            [&](const System& s) { return s.name == name; }),
        systems_.end());
    dirty_ = true;
}

void SystemScheduler::set_enabled(const std::string& name, bool enabled) {
    for (auto& s : systems_) {
        if (s.name == name) {
            s.enabled = enabled;
            return;
        }
    }
}

void SystemScheduler::execute(Registry& registry, float dt) {
    if (dirty_) rebuild_order();

    for (size_t idx : execution_order_) {
        if (idx < systems_.size() && systems_[idx].enabled) {
            systems_[idx].execute(registry, dt);
        }
    }
}

std::vector<std::string> SystemScheduler::get_execution_order() const {
    std::vector<std::string> order;
    for (size_t idx : execution_order_) {
        if (idx < systems_.size()) {
            order.push_back(systems_[idx].name);
        }
    }
    return order;
}

void SystemScheduler::rebuild_order() {
    execution_order_.clear();
    dirty_ = false;

    if (systems_.empty()) return;

    size_t n = systems_.size();

    // Build name -> index map
    std::unordered_map<std::string, size_t> name_to_idx;
    for (size_t i = 0; i < n; ++i) {
        name_to_idx[systems_[i].name] = i;
    }

    // Build adjacency list and in-degree for topological sort
    std::vector<std::vector<size_t>> adj(n);
    std::vector<int> in_degree(n, 0);

    bool has_unknown_deps = false;
    for (size_t i = 0; i < n; ++i) {
        for (const auto& dep : systems_[i].run_after) {
            auto it = name_to_idx.find(dep);
            if (it != name_to_idx.end()) {
                adj[it->second].push_back(i);
                in_degree[i]++;
            } else {
                NX_ERROR("SystemScheduler: system '{}' depends on unknown system '{}' — "
                         "dependency will be ignored", systems_[i].name, dep);
                has_unknown_deps = true;
            }
        }
    }

    // Kahn's algorithm
    std::queue<size_t> queue;
    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) queue.push(i);
    }

    while (!queue.empty()) {
        size_t u = queue.front();
        queue.pop();
        execution_order_.push_back(u);

        for (size_t v : adj[u]) {
            if (--in_degree[v] == 0) queue.push(v);
        }
    }

    if (execution_order_.size() != n) {
        // Identify which systems are in the cycle
        std::string cycled;
        for (size_t i = 0; i < n; ++i) {
            if (in_degree[i] > 0) {
                if (!cycled.empty()) cycled += ", ";
                cycled += systems_[i].name;
            }
        }
        NX_ERROR("SystemScheduler: DEPENDENCY CYCLE detected among [{}]! "
                 "{} of {} systems scheduled. Cycled systems will NOT run.",
                 cycled, execution_order_.size(), n);
        // Do NOT include cycled systems — they are excluded from execution_order_
    }

    if (has_unknown_deps) {
        NX_WARN("SystemScheduler: some dependencies reference unknown systems — "
                "check your system registration order");
    }
}

} // namespace nexus
