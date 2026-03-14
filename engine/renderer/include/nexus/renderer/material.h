#pragma once

#include <nexus/core/types.h>
#include <nexus/core/math.h>
#include <nexus/rhi/rhi.h>
#include <string>
#include <unordered_map>
#include <variant>

namespace nexus {

// ─────────────────────────────────────────────────────────────────────────────
// MaterialProperty - a typed property value for material instances
// ─────────────────────────────────────────────────────────────────────────────

using MaterialPropertyValue = std::variant<float, Vec2, Vec3, Vec4, i32, rhi::TextureHandle>;

// ─────────────────────────────────────────────────────────────────────────────
// Material - shader + uniform properties + textures
// ─────────────────────────────────────────────────────────────────────────────

class Material {
public:
    Material() = default;

    Material(rhi::ShaderHandle shader, const std::string& name = "Unnamed")
        : shader_(shader), name_(name) {}

    // Property setters
    void set_float(const std::string& name, float value);
    void set_vec2(const std::string& name, Vec2 value);
    void set_vec3(const std::string& name, Vec3 value);
    void set_vec4(const std::string& name, Vec4 value);
    void set_int(const std::string& name, i32 value);
    void set_texture(const std::string& name, rhi::TextureHandle handle, u32 slot);

    // Property getters
    template <typename T>
    const T* get(const std::string& name) const {
        auto it = properties_.find(name);
        if (it == properties_.end()) return nullptr;
        return std::get_if<T>(&it->second);
    }

    // Apply all properties to the RHI (bind shader, upload uniforms, bind textures)
    void apply(rhi::RHI* rhi) const;

    // Accessors
    [[nodiscard]] rhi::ShaderHandle shader() const { return shader_; }
    [[nodiscard]] const std::string& name() const { return name_; }

    // Blend / depth / cull state
    rhi::BlendMode blend{rhi::BlendMode::None};
    rhi::CullMode  cull{rhi::CullMode::Back};
    bool depth_test{true};
    bool depth_write{true};
    bool double_sided{false};

private:
    rhi::ShaderHandle shader_{rhi::INVALID_HANDLE};
    std::string name_;
    std::unordered_map<std::string, MaterialPropertyValue> properties_;

    struct TextureBinding {
        rhi::TextureHandle handle;
        u32 slot;
    };
    std::vector<TextureBinding> texture_bindings_;
};

// ─────────────────────────────────────────────────────────────────────────────
// MaterialLibrary - manages named materials
// ─────────────────────────────────────────────────────────────────────────────

class MaterialLibrary {
public:
    void add(const std::string& name, Material material);
    Material* get(const std::string& name);
    bool has(const std::string& name) const;
    void remove(const std::string& name);
    void clear();

private:
    std::unordered_map<std::string, Material> materials_;
};

} // namespace nexus
