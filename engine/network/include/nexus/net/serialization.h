#pragma once

#include "nexus/core/types.h"
#include <string>
#include <vector>
#include <cstring>
#include <type_traits>

namespace nexus::net {

// ─────────────────────────────────────────────────────────────────────────────
// BitWriter — writes data at bit granularity for efficient packing
// ─────────────────────────────────────────────────────────────────────────────

class BitWriter {
public:
    BitWriter() = default;
    explicit BitWriter(u32 initial_capacity_bytes);

    /// Write N bits from value (LSB first).
    void write_bits(u32 value, u32 bit_count);

    /// Write common types.
    void write_bool(bool value);
    void write_u8(u8 value);
    void write_u16(u16 value);
    void write_u32(u32 value);
    void write_u64(u64 value);
    void write_i32(i32 value);
    void write_f32(f32 value);
    void write_string(const std::string& value);
    void write_bytes(const u8* data, u32 size);

    /// Write a value clamped to [min, max] using only enough bits.
    void write_ranged(i32 value, i32 min_val, i32 max_val);

    /// Flush remaining bits (pad to byte boundary).
    void flush();

    /// Get the written data.
    const std::vector<u8>& data() const { return buffer_; }

    /// Total bits written.
    u32 bits_written() const { return bits_written_; }

    /// Total bytes used (rounded up).
    u32 bytes_used() const { return (bits_written_ + 7) / 8; }

    /// Reset writer.
    void reset();

private:
    void ensure_capacity(u32 bits);

    std::vector<u8> buffer_;
    u32 bits_written_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// BitReader — reads data at bit granularity
// ─────────────────────────────────────────────────────────────────────────────

class BitReader {
public:
    BitReader() = default;
    BitReader(const u8* data, u32 size_bytes);
    explicit BitReader(const std::vector<u8>& data);

    /// Read N bits as u32.
    u32 read_bits(u32 bit_count);

    /// Read common types.
    bool read_bool();
    u8 read_u8();
    u16 read_u16();
    u32 read_u32();
    u64 read_u64();
    i32 read_i32();
    f32 read_f32();
    std::string read_string();
    std::vector<u8> read_bytes(u32 size);

    /// Read a ranged value.
    i32 read_ranged(i32 min_val, i32 max_val);

    /// Current read position in bits.
    u32 bits_read() const { return bits_read_; }

    /// Total bits available.
    u32 total_bits() const { return total_bits_; }

    /// Remaining bits.
    u32 remaining_bits() const {
        return bits_read_ <= total_bits_ ? total_bits_ - bits_read_ : 0;
    }

    /// Has the reader reached the end?
    bool is_exhausted() const { return bits_read_ >= total_bits_; }

    /// Has an error occurred (read past end)?
    bool has_error() const { return error_; }

private:
    const u8* data_{nullptr};
    u32 total_bits_{0};
    u32 bits_read_{0};
    bool error_{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// DeltaCompressor — compresses data by only sending differences
// ─────────────────────────────────────────────────────────────────────────────

class DeltaCompressor {
public:
    /// Compute delta between baseline and current state.
    /// Returns only the changed bytes (with a bitmask header).
    static std::vector<u8> compress(const std::vector<u8>& baseline,
                                     const std::vector<u8>& current);

    /// Apply delta to baseline to reconstruct current state.
    static std::vector<u8> decompress(const std::vector<u8>& baseline,
                                       const std::vector<u8>& delta);

    /// Compute the compression ratio (0.0 = perfect, 1.0 = no compression).
    static f32 ratio(const std::vector<u8>& original, const std::vector<u8>& compressed);
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: compute bits needed to represent a range
// ─────────────────────────────────────────────────────────────────────────────

inline u32 bits_required(u32 range) {
    if (range <= 1) return 1;
    u32 bits = 0;
    u32 v = range - 1;
    while (v > 0) {
        v >>= 1;
        bits++;
    }
    return bits;
}

} // namespace nexus::net
