#include "nexus/core/types.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <mutex>

namespace nexus {

class VirtualFileSystem {
public:
    static VirtualFileSystem& instance() {
        static VirtualFileSystem vfs;
        return vfs;
    }

    void mount(const std::string& virtual_path, const std::string& physical_path) {
        std::lock_guard lock(mutex_);
        mount_points_[virtual_path] = physical_path;
    }

    void unmount(const std::string& virtual_path) {
        std::lock_guard lock(mutex_);
        mount_points_.erase(virtual_path);
    }

    std::optional<std::string> resolve(const std::string& virtual_path) const {
        std::lock_guard lock(mutex_);
        for (const auto& [vpath, ppath] : mount_points_) {
            if (virtual_path.starts_with(vpath)) {
                auto relative = virtual_path.substr(vpath.size());
                auto resolved = std::filesystem::path(ppath) / relative;
                if (std::filesystem::exists(resolved)) {
                    return resolved.string();
                }
            }
        }
        return std::nullopt;
    }

    std::optional<std::vector<u8>> read_binary(const std::string& virtual_path) const {
        auto resolved = resolve(virtual_path);
        if (!resolved) return std::nullopt;

        std::ifstream file(*resolved, std::ios::binary | std::ios::ate);
        if (!file) return std::nullopt;

        auto size = file.tellg();
        file.seekg(0);
        std::vector<u8> data(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }

    std::optional<std::string> read_text(const std::string& virtual_path) const {
        auto resolved = resolve(virtual_path);
        if (!resolved) return std::nullopt;

        std::ifstream file(*resolved);
        if (!file) return std::nullopt;

        return std::string{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
    }

private:
    VirtualFileSystem() = default;

    std::unordered_map<std::string, std::string> mount_points_;
    mutable std::mutex mutex_;
};

} // namespace nexus
