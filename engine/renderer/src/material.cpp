#include <nexus/renderer/material.h>

namespace nexus {

// ── Property setters ────────────────────────────────────────────────────────

void Material::set_float(const std::string& name, float value) {
    properties_[name] = value;
}

void Material::set_vec2(const std::string& name, Vec2 value) {
    properties_[name] = value;
}

void Material::set_vec3(const std::string& name, Vec3 value) {
    properties_[name] = value;
}

void Material::set_vec4(const std::string& name, Vec4 value) {
    properties_[name] = value;
}

void Material::set_int(const std::string& name, i32 value) {
    properties_[name] = value;
}

void Material::set_texture(const std::string& name, rhi::TextureHandle handle, u32 slot) {
    properties_[name] = static_cast<i32>(slot);
    texture_bindings_.push_back({handle, slot});
}

// ── Apply ───────────────────────────────────────────────────────────────────

void Material::apply(rhi::RHI* rhi) const {
    if (shader_ == rhi::INVALID_HANDLE) return;

    rhi->bind_shader(shader_);

    // Bind textures
    for (const auto& binding : texture_bindings_) {
        rhi->bind_texture(binding.handle, binding.slot);
    }

    // Set uniforms
    for (const auto& [prop_name, value] : properties_) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>) {
                rhi->set_uniform_float(shader_, prop_name, v);
            } else if constexpr (std::is_same_v<T, Vec2>) {
                rhi->set_uniform_vec2(shader_, prop_name, v);
            } else if constexpr (std::is_same_v<T, Vec3>) {
                rhi->set_uniform_vec3(shader_, prop_name, v);
            } else if constexpr (std::is_same_v<T, Vec4>) {
                rhi->set_uniform_vec4(shader_, prop_name, v);
            } else if constexpr (std::is_same_v<T, i32>) {
                rhi->set_uniform_int(shader_, prop_name, v);
            } else if constexpr (std::is_same_v<T, rhi::TextureHandle>) {
                // Texture handles are bound separately above
            }
        }, value);
    }

    // Apply render state
    rhi->set_blend_mode(blend);
    rhi->set_depth_test(depth_test);
}

// ── MaterialLibrary ─────────────────────────────────────────────────────────

void MaterialLibrary::add(const std::string& name, Material material) {
    materials_[name] = std::move(material);
}

Material* MaterialLibrary::get(const std::string& name) {
    auto it = materials_.find(name);
    return it != materials_.end() ? &it->second : nullptr;
}

bool MaterialLibrary::has(const std::string& name) const {
    return materials_.count(name) > 0;
}

void MaterialLibrary::remove(const std::string& name) {
    materials_.erase(name);
}

void MaterialLibrary::clear() {
    materials_.clear();
}

} // namespace nexus
