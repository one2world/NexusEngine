#pragma once

#include "nexus/core/types.h"

namespace nexus {

/// Lightweight entity identifier used throughout the ECS.
using Entity = u32;

/// Sentinel value representing an invalid / null entity.
constexpr Entity INVALID_ENTITY = ~u32(0);

} // namespace nexus
