#pragma once

#include "nexus/core/types.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <functional>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// ResourceManager — type-safe centralized resource cache
// ─────────────────────────────────────────────────────────────────────────────

class ResourceManager {
public:
    /// Store a resource of type T by name.
    template <typename T>
    void store(const std::string& name, std::shared_ptr<T> resource) {
        auto& cache = get_cache<T>();
        cache[name] = std::move(resource);
    }

    /// Retrieve a resource of type T by name.
    template <typename T>
    std::shared_ptr<T> get(const std::string& name) const {
        auto& cache = get_cache<T>();
        auto it = cache.find(name);
        return it != cache.end() ? it->second : nullptr;
    }

    /// Check if a resource exists.
    template <typename T>
    bool has(const std::string& name) const {
        auto& cache = get_cache<T>();
        return cache.find(name) != cache.end();
    }

    /// Remove a specific resource.
    template <typename T>
    void remove(const std::string& name) {
        auto& cache = get_cache<T>();
        cache.erase(name);
    }

    /// Get or load: returns cached resource, or calls loader and caches result.
    template <typename T>
    std::shared_ptr<T> get_or_load(const std::string& name,
                                     std::function<std::shared_ptr<T>()> loader) {
        auto existing = get<T>(name);
        if (existing) return existing;
        auto resource = loader();
        if (resource) store<T>(name, resource);
        return resource;
    }

    /// Remove all resources of a given type.
    template <typename T>
    void clear_type() {
        auto key = std::type_index(typeid(T));
        caches_.erase(key);
    }

    /// Remove all resources of all types.
    void clear_all() { caches_.clear(); }

    /// Count resources of a given type.
    template <typename T>
    u32 count() const {
        auto& cache = get_cache<T>();
        return static_cast<u32>(cache.size());
    }

private:
    struct ICacheBase {
        virtual ~ICacheBase() = default;
    };

    template <typename T>
    struct TypedCache : ICacheBase {
        std::unordered_map<std::string, std::shared_ptr<T>> entries;
    };

    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<T>>& get_cache() const {
        auto key = std::type_index(typeid(T));
        auto it = caches_.find(key);
        if (it == caches_.end()) {
            auto cache = std::make_unique<TypedCache<T>>();
            auto* ptr = cache.get();
            caches_[key] = std::move(cache);
            return ptr->entries;
        }
        return static_cast<TypedCache<T>*>(it->second.get())->entries;
    }

    mutable std::unordered_map<std::type_index, std::unique_ptr<ICacheBase>> caches_;
};

} // namespace nexus
