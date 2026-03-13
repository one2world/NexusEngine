#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <cassert>

namespace nexus {

// Fixed-width integer aliases
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

// Smart pointer aliases
template <typename T>
using Unique = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr Unique<T> make_unique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
using Shared = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr Shared<T> make_shared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T>
using Weak = std::weak_ptr<T>;

// Engine assertion
#ifdef NEXUS_DEBUG
    #define NEXUS_ASSERT(expr, msg) assert((expr) && (msg))
#else
    #define NEXUS_ASSERT(expr, msg) ((void)0)
#endif

#define NEXUS_NON_COPYABLE(Class) \
    Class(const Class&) = delete; \
    Class& operator=(const Class&) = delete;

#define NEXUS_NON_MOVABLE(Class) \
    Class(Class&&) = delete; \
    Class& operator=(Class&&) = delete;

} // namespace nexus
