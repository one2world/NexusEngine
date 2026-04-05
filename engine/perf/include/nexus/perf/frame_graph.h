#pragma once

#include "nexus/core/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace nexus {

/// Resource type used by render passes.
enum class FrameResourceType : u8 {
    Texture,
    Buffer,
    Framebuffer
};

/// Access mode for a resource.
enum class FrameResourceAccess : u8 {
    Read,
    Write,
    ReadWrite
};

/// A resource node in the frame graph.
struct FrameResource {
    std::string name;
    FrameResourceType type{FrameResourceType::Texture};
    u32 width{0};
    u32 height{0};
    bool transient{true}; // Managed by frame graph lifetime
};

/// A render pass node in the frame graph.
struct FramePass {
    std::string name;
    u32 id{0};

    struct ResourceRef {
        u32 resource_id{0};
        FrameResourceAccess access{FrameResourceAccess::Read};
    };

    std::vector<ResourceRef> inputs;
    std::vector<ResourceRef> outputs;

    using ExecuteFn = std::function<void()>;
    ExecuteFn execute;

    // Profiling
    f64 last_duration_us{0.0};
    f64 avg_duration_us{0.0};
    bool enabled{true};
    bool culled{false}; // Set by compilation step
};

/// FrameGraph — declarative render pass scheduling and resource management.
///
/// Usage:
///   1. Declare passes with add_pass()
///   2. Declare resources with add_resource()
///   3. Call compile() to topologically sort and cull unused passes
///   4. Call execute() to run active passes in order
///   5. Call reset() at end of frame
class FrameGraph {
public:
    FrameGraph() = default;

    /// Add a named resource. Returns its ID.
    u32 add_resource(const std::string& name, FrameResourceType type,
                     u32 width = 0, u32 height = 0);

    /// Add a named render pass. Returns its ID.
    u32 add_pass(const std::string& name);

    /// Declare that a pass reads from a resource.
    void pass_reads(u32 pass_id, u32 resource_id);

    /// Declare that a pass writes to a resource.
    void pass_writes(u32 pass_id, u32 resource_id);

    /// Set the execution function for a pass.
    void set_pass_execute(u32 pass_id, FramePass::ExecuteFn fn);

    /// Mark a resource as the final output (prevents culling of its producers).
    void mark_output(u32 resource_id);

    /// Compile: topological sort, dead-pass culling, resource lifetime analysis.
    /// Returns true if compilation succeeds (no cycles).
    bool compile();

    /// Execute all non-culled passes in topological order.
    void execute();

    /// Reset for next frame (clears execution order, preserves structure).
    void reset();

    /// Clear all passes and resources.
    void clear();

    /// Get the compiled execution order (pass IDs).
    const std::vector<u32>& execution_order() const { return exec_order_; }

    /// Access pass/resource data for visualization.
    const std::vector<FramePass>& passes() const { return passes_; }
    const std::vector<FrameResource>& resources() const { return resources_; }

    /// Get pass by ID.
    FramePass& pass(u32 id) { return passes_[id]; }
    const FramePass& pass(u32 id) const { return passes_[id]; }

    /// Get resource by ID.
    FrameResource& resource(u32 id) { return resources_[id]; }
    const FrameResource& resource(u32 id) const { return resources_[id]; }

    u32 pass_count() const { return static_cast<u32>(passes_.size()); }
    u32 resource_count() const { return static_cast<u32>(resources_.size()); }
    u32 active_pass_count() const;

    /// Generate a text-based visualization of the frame graph.
    std::string dump() const;

private:
    bool topological_sort();
    void cull_unused_passes();

    std::vector<FramePass> passes_;
    std::vector<FrameResource> resources_;
    std::unordered_set<u32> output_resources_;
    std::vector<u32> exec_order_;
    bool compiled_{false};
};

} // namespace nexus
