#pragma once

#include "nexus/core/types.h"
#include "nexus/rhi/rhi.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// ShaderLibrary — file-based shader management with hot-reload and variants
// ─────────────────────────────────────────────────────────────────────────────

class ShaderLibrary {
public:
    explicit ShaderLibrary(rhi::RHI* rhi);
    ~ShaderLibrary();

    /// Set the base directory for shader files.
    void set_shader_directory(const std::string& dir) { shader_dir_ = dir; }

    /// Load a shader from vertex + fragment file paths.
    /// Returns a handle or INVALID_HANDLE on failure.
    rhi::ShaderHandle load(const std::string& name,
                           const std::string& vertex_path,
                           const std::string& fragment_path);

    /// Load a shader with variant defines.
    /// Defines are prepended as #define KEY VALUE after the #version line.
    rhi::ShaderHandle load_variant(const std::string& name,
                                    const std::string& vertex_path,
                                    const std::string& fragment_path,
                                    const std::unordered_map<std::string, std::string>& defines);

    /// Get a previously loaded shader by name.
    [[nodiscard]] rhi::ShaderHandle get(const std::string& name) const;

    /// Check all loaded shaders for file changes. Recompile if modified.
    /// Returns number of shaders reloaded.
    u32 check_hot_reload();

    /// Force reload a specific shader.
    bool reload(const std::string& name);

    /// Reload all shaders.
    void reload_all();

    /// Number of loaded shaders.
    [[nodiscard]] u32 shader_count() const { return static_cast<u32>(entries_.size()); }

    /// Set a callback for when a shader is reloaded.
    using ReloadCallback = std::function<void(const std::string& name, rhi::ShaderHandle handle)>;
    void set_reload_callback(ReloadCallback cb) { reload_callback_ = std::move(cb); }

private:
    struct ShaderEntry {
        std::string name;
        std::string vertex_path;
        std::string fragment_path;
        std::unordered_map<std::string, std::string> defines;
        rhi::ShaderHandle handle{rhi::INVALID_HANDLE};
        u64 vertex_last_modified{0};
        u64 fragment_last_modified{0};
    };

    /// Read a shader file, resolving #include directives.
    std::string read_shader_file(const std::string& path,
                                  std::unordered_set<std::string>& included);

    /// Inject #define statements after the #version line.
    static std::string inject_defines(const std::string& source,
                                       const std::unordered_map<std::string, std::string>& defines);

    /// Get file modification time.
    static u64 get_file_mtime(const std::string& path);

    /// Compile a shader entry. Returns true on success.
    bool compile_entry(ShaderEntry& entry);

    rhi::RHI* rhi_;
    std::string shader_dir_;
    std::unordered_map<std::string, ShaderEntry> entries_;
    ReloadCallback reload_callback_;
};

} // namespace nexus
