#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace nexus {

// Vector types
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using IVec2 = glm::ivec2;
using IVec3 = glm::ivec3;
using IVec4 = glm::ivec4;
using UVec2 = glm::uvec2;
using UVec3 = glm::uvec3;

// Matrix types
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// Quaternion
using Quat = glm::quat;

// Common geometric primitives
struct Rect {
    Vec2 position{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};

    [[nodiscard]] float left() const   { return position.x; }
    [[nodiscard]] float right() const  { return position.x + size.x; }
    [[nodiscard]] float top() const    { return position.y; }
    [[nodiscard]] float bottom() const { return position.y + size.y; }
    [[nodiscard]] Vec2 center() const  { return position + size * 0.5f; }

    [[nodiscard]] bool contains(Vec2 point) const {
        return point.x >= left() && point.x <= right() &&
               point.y >= top()  && point.y <= bottom();
    }

    [[nodiscard]] bool overlaps(const Rect& other) const {
        return left() < other.right()  && right()  > other.left() &&
               top()  < other.bottom() && bottom() > other.top();
    }
};

struct AABB {
    Vec3 min{0.0f};
    Vec3 max{0.0f};

    [[nodiscard]] Vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] Vec3 extents() const { return (max - min) * 0.5f; }

    [[nodiscard]] bool contains(Vec3 point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] bool overlaps(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
};

struct Transform2D {
    Vec2  position{0.0f, 0.0f};
    float rotation{0.0f};  // radians
    Vec2  scale{1.0f, 1.0f};

    [[nodiscard]] Mat4 to_matrix() const {
        Mat4 m(1.0f);
        m = glm::translate(m, Vec3(position, 0.0f));
        m = glm::rotate(m, rotation, Vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, Vec3(scale, 1.0f));
        return m;
    }
};

struct Transform3D {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};

    [[nodiscard]] Mat4 to_matrix() const {
        Mat4 m(1.0f);
        m = glm::translate(m, position);
        m *= glm::toMat4(rotation);
        m = glm::scale(m, scale);
        return m;
    }

    [[nodiscard]] Vec3 forward() const { return rotation * Vec3(0.0f, 0.0f, -1.0f); }
    [[nodiscard]] Vec3 right() const   { return rotation * Vec3(1.0f, 0.0f, 0.0f); }
    [[nodiscard]] Vec3 up() const      { return rotation * Vec3(0.0f, 1.0f, 0.0f); }
};

// Utility constants
namespace math {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = PI * 2.0f;
    constexpr float HALF_PI = PI * 0.5f;
    constexpr float DEG2RAD = PI / 180.0f;
    constexpr float RAD2DEG = 180.0f / PI;
    constexpr float EPSILON = 1e-6f;

    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
    inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
    inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

    inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float remap(float v, float inMin, float inMax, float outMin, float outMax) {
        float t = (v - inMin) / (inMax - inMin);
        return outMin + t * (outMax - outMin);
    }
} // namespace math

} // namespace nexus
