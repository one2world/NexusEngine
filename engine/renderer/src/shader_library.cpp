#include "nexus/renderer/shader_library.h"
#include "nexus/core/log.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace nexus {

ShaderLibrary::ShaderLibrary(rhi::RHI* rhi)
    : rhi_(rhi) {}

ShaderLibrary::~ShaderLibrary() = default;

// ── File I/O helpers ───────────────────────────────────────────────────────

u64 ShaderLibrary::get_file_mtime(const std::string& path) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return static_cast<u64>(ftime.time_since_epoch().count());
}

std::string ShaderLibrary::read_shader_file(const std::string& path,
                                             std::unordered_set<std::string>& included) {
    // Prevent circular includes
    if (included.count(path)) {
        NX_WARN("ShaderLibrary: circular #include detected for '{}'", path);
        return "";
    }
    included.insert(path);

    std::string full_path = path;
    if (!shader_dir_.empty() && !std::filesystem::path(path).is_absolute()) {
        full_path = shader_dir_ + "/" + path;
    }

    std::ifstream file(full_path);
    if (!file.is_open()) {
        NX_ERROR("ShaderLibrary: failed to open shader file '{}'", full_path);
        return "";
    }

    std::stringstream result;
    std::string line;
    u32 line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;

        // Handle #include "filename"
        if (line.find("#include") == 0) {
            auto quote1 = line.find('"');
            auto quote2 = line.rfind('"');
            if (quote1 != std::string::npos && quote2 != std::string::npos && quote2 > quote1) {
                std::string include_file = line.substr(quote1 + 1, quote2 - quote1 - 1);

                // Resolve relative to current file's directory
                std::filesystem::path current_dir = std::filesystem::path(full_path).parent_path();
                std::string include_path = (current_dir / include_file).string();

                result << "// --- begin include: " << include_file << " ---\n";
                result << read_shader_file(include_path, included);
                result << "// --- end include: " << include_file << " ---\n";
                continue;
            }
        }

        result << line << "\n";
    }

    return result.str();
}

std::string ShaderLibrary::inject_defines(
    const std::string& source,
    const std::unordered_map<std::string, std::string>& defines) {
    if (defines.empty()) return source;

    // Find the #version line and inject defines after it
    auto version_pos = source.find("#version");
    if (version_pos == std::string::npos) {
        // No #version — prepend defines at the top
        std::string result;
        for (auto& [key, val] : defines) {
            result += "#define " + key + " " + val + "\n";
        }
        result += source;
        return result;
    }

    // Find end of #version line
    auto newline_pos = source.find('\n', version_pos);
    if (newline_pos == std::string::npos) newline_pos = source.size();

    std::string result = source.substr(0, newline_pos + 1);
    for (auto& [key, val] : defines) {
        result += "#define " + key + " " + val + "\n";
    }
    result += source.substr(newline_pos + 1);
    return result;
}

// ── Compilation ────────────────────────────────────────────────────────────

bool ShaderLibrary::compile_entry(ShaderEntry& entry) {
    std::unordered_set<std::string> included_vert;
    std::string vert_source = read_shader_file(entry.vertex_path, included_vert);
    if (vert_source.empty()) return false;

    std::unordered_set<std::string> included_frag;
    std::string frag_source = read_shader_file(entry.fragment_path, included_frag);
    if (frag_source.empty()) return false;

    // Inject variant defines
    vert_source = inject_defines(vert_source, entry.defines);
    frag_source = inject_defines(frag_source, entry.defines);

    // Destroy old shader
    if (entry.handle != rhi::INVALID_HANDLE) {
        rhi_->destroy_shader(entry.handle);
    }

    entry.handle = rhi_->create_shader(vert_source.c_str(), frag_source.c_str());
    if (entry.handle == rhi::INVALID_HANDLE) {
        NX_ERROR("ShaderLibrary: failed to compile shader '{}'", entry.name);
        return false;
    }

    // Update modification times
    std::string vert_full = shader_dir_.empty() ? entry.vertex_path
        : (shader_dir_ + "/" + entry.vertex_path);
    std::string frag_full = shader_dir_.empty() ? entry.fragment_path
        : (shader_dir_ + "/" + entry.fragment_path);
    entry.vertex_last_modified = get_file_mtime(vert_full);
    entry.fragment_last_modified = get_file_mtime(frag_full);

    return true;
}

// ── Public API ─────────────────────────────────────────────────────────────

rhi::ShaderHandle ShaderLibrary::load(const std::string& name,
                                       const std::string& vertex_path,
                                       const std::string& fragment_path) {
    return load_variant(name, vertex_path, fragment_path, {});
}

rhi::ShaderHandle ShaderLibrary::load_variant(
    const std::string& name,
    const std::string& vertex_path,
    const std::string& fragment_path,
    const std::unordered_map<std::string, std::string>& defines) {

    ShaderEntry entry;
    entry.name = name;
    entry.vertex_path = vertex_path;
    entry.fragment_path = fragment_path;
    entry.defines = defines;

    if (!compile_entry(entry)) {
        return rhi::INVALID_HANDLE;
    }

    entries_[name] = std::move(entry);
    NX_INFO("ShaderLibrary: loaded shader '{}' (vert={}, frag={})",
            name, vertex_path, fragment_path);
    return entries_[name].handle;
}

rhi::ShaderHandle ShaderLibrary::get(const std::string& name) const {
    auto it = entries_.find(name);
    return it != entries_.end() ? it->second.handle : rhi::INVALID_HANDLE;
}

u32 ShaderLibrary::check_hot_reload() {
    u32 reloaded = 0;
    for (auto& [name, entry] : entries_) {
        std::string vert_full = shader_dir_.empty() ? entry.vertex_path
            : (shader_dir_ + "/" + entry.vertex_path);
        std::string frag_full = shader_dir_.empty() ? entry.fragment_path
            : (shader_dir_ + "/" + entry.fragment_path);

        u64 vert_mtime = get_file_mtime(vert_full);
        u64 frag_mtime = get_file_mtime(frag_full);

        if (vert_mtime != entry.vertex_last_modified ||
            frag_mtime != entry.fragment_last_modified) {
            NX_INFO("ShaderLibrary: change detected in '{}', reloading...", name);
            if (compile_entry(entry)) {
                ++reloaded;
                if (reload_callback_) {
                    reload_callback_(name, entry.handle);
                }
            }
        }
    }
    return reloaded;
}

bool ShaderLibrary::reload(const std::string& name) {
    auto it = entries_.find(name);
    if (it == entries_.end()) return false;
    bool ok = compile_entry(it->second);
    if (ok && reload_callback_) {
        reload_callback_(name, it->second.handle);
    }
    return ok;
}

void ShaderLibrary::reload_all() {
    for (auto& [name, entry] : entries_) {
        if (compile_entry(entry) && reload_callback_) {
            reload_callback_(name, entry.handle);
        }
    }
}

} // namespace nexus
