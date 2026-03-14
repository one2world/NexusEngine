#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/registry.h"
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace nexus {

// ---------------------------------------------------------------------------
// System - a named function that operates on the registry
// ---------------------------------------------------------------------------
struct System {
    std::string name;
    std::function<void(Registry&, float dt)> execute;
    std::vector<std::string> run_after;  // dependencies
    bool enabled{true};
};

// ---------------------------------------------------------------------------
// SystemScheduler - topological sort + execution of ECS systems
// ---------------------------------------------------------------------------
class SystemScheduler {
public:
    /// Register a system with optional dependencies.
    void add_system(const std::string& name,
                    std::function<void(Registry&, float dt)> fn,
                    std::vector<std::string> dependencies = {});

    /// Remove a system by name.
    void remove_system(const std::string& name);

    /// Enable or disable a system at runtime.
    void set_enabled(const std::string& name, bool enabled);

    /// Execute all enabled systems in dependency order.
    void execute(Registry& registry, float dt);

    /// Get the resolved execution order (for debugging).
    std::vector<std::string> get_execution_order() const;

private:
    void rebuild_order();

    std::vector<System> systems_;
    std::vector<size_t> execution_order_;
    bool dirty_{true};
};

} // namespace nexus
