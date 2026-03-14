#pragma once

#include "nexus/core/types.h"

#include <cstdlib>
#include <new>
#include <utility>

namespace nexus {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Align `value` up to the nearest multiple of `alignment`.
/// `alignment` must be a power of two.
inline constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

// ---------------------------------------------------------------------------
// LinearAllocator
// ---------------------------------------------------------------------------
/// Bump / linear allocator.  Allocates sequentially from a pre-allocated
/// contiguous buffer.  Individual de-allocations are not supported; call
/// reset() to free everything at once.
class LinearAllocator {
public:
    NEXUS_NON_COPYABLE(LinearAllocator)

    explicit LinearAllocator(std::size_t capacity)
        : m_capacity{capacity}
        , m_offset{0} {
        m_buffer = static_cast<u8*>(std::malloc(capacity));
        if (!m_buffer) {
            throw std::bad_alloc{};
        }
    }

    LinearAllocator(LinearAllocator&& other) noexcept
        : m_buffer{other.m_buffer}
        , m_capacity{other.m_capacity}
        , m_offset{other.m_offset} {
        other.m_buffer   = nullptr;
        other.m_capacity = 0;
        other.m_offset   = 0;
    }

    LinearAllocator& operator=(LinearAllocator&& other) noexcept {
        if (this != &other) {
            std::free(m_buffer);
            m_buffer         = other.m_buffer;
            m_capacity       = other.m_capacity;
            m_offset         = other.m_offset;
            other.m_buffer   = nullptr;
            other.m_capacity = 0;
            other.m_offset   = 0;
        }
        return *this;
    }

    ~LinearAllocator() {
        std::free(m_buffer);
    }

    /// Allocate `size` bytes with the given `alignment` (must be power of 2).
    /// Returns nullptr if the allocator is exhausted.
    void* allocate(std::size_t size, std::size_t alignment = 8) noexcept {
        std::size_t aligned_offset = align_up(m_offset, alignment);
        if (aligned_offset + size > m_capacity) {
            return nullptr;
        }
        void* ptr = m_buffer + aligned_offset;
        m_offset  = aligned_offset + size;
        return ptr;
    }

    /// Reset the allocator – all previous allocations become invalid.
    void reset() noexcept { m_offset = 0; }

    /// Number of bytes currently in use (including alignment padding).
    [[nodiscard]] std::size_t used()     const noexcept { return m_offset; }

    /// Total capacity of the backing buffer in bytes.
    [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }

private:
    friend class ScopedArena;

    u8*         m_buffer   = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_offset   = 0;
};

// ---------------------------------------------------------------------------
// ArenaAllocator
// ---------------------------------------------------------------------------
/// Arena allocator that grows by appending new blocks when the current one is
/// exhausted.  Designed for per-frame allocations where everything is freed at
/// the end of the frame via reset().
class ArenaAllocator {
public:
    NEXUS_NON_COPYABLE(ArenaAllocator)

    static constexpr std::size_t DEFAULT_BLOCK_SIZE = 1024 * 1024; // 1 MB

    explicit ArenaAllocator(std::size_t block_size = DEFAULT_BLOCK_SIZE)
        : m_block_size{block_size} {
        allocate_block(m_block_size);
    }

    ArenaAllocator(ArenaAllocator&& other) noexcept
        : m_blocks{std::move(other.m_blocks)}
        , m_block_size{other.m_block_size}
        , m_current_offset{other.m_current_offset} {
        other.m_current_offset = 0;
    }

    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept {
        if (this != &other) {
            free_all();
            m_blocks             = std::move(other.m_blocks);
            m_block_size         = other.m_block_size;
            m_current_offset     = other.m_current_offset;
            other.m_current_offset = 0;
        }
        return *this;
    }

    ~ArenaAllocator() {
        free_all();
    }

    /// Allocate `size` bytes with the given `alignment`.
    /// Grows by allocating a new block if the current block is exhausted.
    void* allocate(std::size_t size, std::size_t alignment = 8) {
        NEXUS_ASSERT(!m_blocks.empty(), "ArenaAllocator has no blocks");

        Block& current         = m_blocks.back();
        std::size_t aligned    = align_up(m_current_offset, alignment);

        if (aligned + size <= current.size) {
            void* ptr        = current.memory + aligned;
            m_current_offset = aligned + size;
            return ptr;
        }

        // Current block can't satisfy the request – allocate a new one.
        std::size_t new_block_size = m_block_size;
        // If the requested size (with worst-case alignment padding) exceeds the
        // default block size, allocate a block large enough to hold it.
        if (size + alignment > new_block_size) {
            new_block_size = size + alignment;
        }
        allocate_block(new_block_size);

        Block& fresh           = m_blocks.back();
        std::size_t fresh_off  = align_up(0, alignment);
        void* ptr              = fresh.memory + fresh_off;
        m_current_offset       = fresh_off + size;
        return ptr;
    }

    /// Free all blocks except the first one and reset offsets.
    void reset() noexcept {
        if (m_blocks.empty()) return;

        // Keep the first block, free the rest.
        for (std::size_t i = 1; i < m_blocks.size(); ++i) {
            std::free(m_blocks[i].memory);
        }
        std::size_t first_size = m_blocks[0].size;
        m_blocks.resize(1);
        m_blocks[0].size = first_size;
        m_current_offset = 0;
    }

private:
    struct Block {
        u8*         memory = nullptr;
        std::size_t size   = 0;
    };

    void allocate_block(std::size_t size) {
        Block block;
        block.memory = static_cast<u8*>(std::malloc(size));
        if (!block.memory) {
            throw std::bad_alloc{};
        }
        block.size = size;
        m_blocks.push_back(block);
        m_current_offset = 0;
    }

    void free_all() noexcept {
        for (auto& b : m_blocks) {
            std::free(b.memory);
        }
        m_blocks.clear();
        m_current_offset = 0;
    }

    std::vector<Block> m_blocks;
    std::size_t        m_block_size      = DEFAULT_BLOCK_SIZE;
    std::size_t        m_current_offset  = 0;
};

// ---------------------------------------------------------------------------
// PoolAllocator
// ---------------------------------------------------------------------------
/// Fixed-size object pool backed by a free list.
/// `ObjectSize` is the size of each slot in bytes.
/// `Alignment` is the alignment requirement for each slot.
template <std::size_t ObjectSize, std::size_t Alignment = 8>
class PoolAllocator {
public:
    NEXUS_NON_COPYABLE(PoolAllocator)

    static constexpr std::size_t SLOT_SIZE =
        align_up(ObjectSize < sizeof(void*) ? sizeof(void*) : ObjectSize, Alignment);

    /// Create a pool that can hold `count` objects.
    explicit PoolAllocator(std::size_t count)
        : m_count{count} {
        // Allocate aligned raw storage.
        std::size_t total = SLOT_SIZE * count;
        m_buffer = static_cast<u8*>(std::aligned_alloc(Alignment, total));
        if (!m_buffer) {
            throw std::bad_alloc{};
        }

        // Build the free list.
        m_free_head = nullptr;
        for (std::size_t i = count; i > 0; --i) {
            auto* node  = reinterpret_cast<FreeNode*>(m_buffer + (i - 1) * SLOT_SIZE);
            node->next  = m_free_head;
            m_free_head = node;
        }
    }

    PoolAllocator(PoolAllocator&& other) noexcept
        : m_buffer{other.m_buffer}
        , m_free_head{other.m_free_head}
        , m_count{other.m_count} {
        other.m_buffer    = nullptr;
        other.m_free_head = nullptr;
        other.m_count     = 0;
    }

    PoolAllocator& operator=(PoolAllocator&& other) noexcept {
        if (this != &other) {
            std::free(m_buffer);
            m_buffer          = other.m_buffer;
            m_free_head       = other.m_free_head;
            m_count           = other.m_count;
            other.m_buffer    = nullptr;
            other.m_free_head = nullptr;
            other.m_count     = 0;
        }
        return *this;
    }

    ~PoolAllocator() {
        std::free(m_buffer);
    }

    /// Allocate a single slot.  Returns nullptr if the pool is exhausted.
    void* allocate() noexcept {
        if (!m_free_head) {
            return nullptr;
        }
        FreeNode* node = m_free_head;
        m_free_head    = node->next;
        return static_cast<void*>(node);
    }

    /// Return a previously allocated slot to the pool.
    void deallocate(void* ptr) noexcept {
        if (!ptr) return;
        NEXUS_ASSERT(
            ptr >= m_buffer && ptr < m_buffer + SLOT_SIZE * m_count,
            "PoolAllocator::deallocate – pointer not from this pool");
        auto* node  = static_cast<FreeNode*>(ptr);
        node->next  = m_free_head;
        m_free_head = node;
    }

    /// Total number of slots in the pool.
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

private:
    struct FreeNode {
        FreeNode* next = nullptr;
    };

    u8*         m_buffer    = nullptr;
    FreeNode*   m_free_head = nullptr;
    std::size_t m_count     = 0;
};

// ---------------------------------------------------------------------------
// ScopedArena
// ---------------------------------------------------------------------------
/// RAII helper that saves the current offset of a LinearAllocator on
/// construction and restores it on destruction, effectively freeing any
/// allocations made within the scope.
class ScopedArena {
public:
    NEXUS_NON_COPYABLE(ScopedArena)
    NEXUS_NON_MOVABLE(ScopedArena)

    explicit ScopedArena(LinearAllocator& allocator) noexcept
        : m_allocator{allocator}
        , m_saved_offset{allocator.m_offset} {}

    ~ScopedArena() noexcept {
        m_allocator.m_offset = m_saved_offset;
    }

    /// Convenience: forward allocations to the underlying LinearAllocator.
    void* allocate(std::size_t size, std::size_t alignment = 8) noexcept {
        return m_allocator.allocate(size, alignment);
    }

private:
    LinearAllocator& m_allocator;
    std::size_t      m_saved_offset;
};

} // namespace nexus
