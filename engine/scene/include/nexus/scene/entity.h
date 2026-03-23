#pragma once

#include "nexus/core/types.h"

namespace nexus {

/// Entity identifier with generation tracking to detect stale references.
/// Layout: low 20 bits = index, high 12 bits = generation.
/// Supports up to ~1M concurrent entities with 4096 generational reuses.
using Entity = u32;

/// Sentinel value representing an invalid / null entity.
constexpr Entity INVALID_ENTITY = ~u32(0);

/// Bit layout constants
constexpr u32 ENTITY_INDEX_BITS  = 20;
constexpr u32 ENTITY_GEN_BITS   = 12;
constexpr u32 ENTITY_INDEX_MASK  = (1u << ENTITY_INDEX_BITS) - 1;
constexpr u32 ENTITY_GEN_MASK   = (1u << ENTITY_GEN_BITS) - 1;

/// Extract the index portion from an entity handle.
inline u32 entity_index(Entity e) { return e & ENTITY_INDEX_MASK; }

/// Extract the generation portion from an entity handle.
inline u32 entity_generation(Entity e) { return (e >> ENTITY_INDEX_BITS) & ENTITY_GEN_MASK; }

/// Construct an entity handle from index and generation.
inline Entity make_entity(u32 index, u32 generation) {
    return (index & ENTITY_INDEX_MASK) | ((generation & ENTITY_GEN_MASK) << ENTITY_INDEX_BITS);
}

} // namespace nexus
