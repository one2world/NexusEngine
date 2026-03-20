#include <gtest/gtest.h>
#include "nexus/net/serialization.h"
#include <cmath>

using namespace nexus;
using namespace nexus::net;

// =============================================================================
// BitWriter / BitReader roundtrip
// =============================================================================

TEST(BitStream, Bool) {
    BitWriter w;
    w.write_bool(true);
    w.write_bool(false);
    w.write_bool(true);
    w.flush();

    BitReader r(w.data());
    EXPECT_TRUE(r.read_bool());
    EXPECT_FALSE(r.read_bool());
    EXPECT_TRUE(r.read_bool());
}

TEST(BitStream, U8) {
    BitWriter w;
    w.write_u8(0);
    w.write_u8(127);
    w.write_u8(255);
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_u8(), 0);
    EXPECT_EQ(r.read_u8(), 127);
    EXPECT_EQ(r.read_u8(), 255);
}

TEST(BitStream, U16) {
    BitWriter w;
    w.write_u16(0);
    w.write_u16(1000);
    w.write_u16(65535);
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_u16(), 0);
    EXPECT_EQ(r.read_u16(), 1000);
    EXPECT_EQ(r.read_u16(), 65535);
}

TEST(BitStream, U32) {
    BitWriter w;
    w.write_u32(0);
    w.write_u32(12345678);
    w.write_u32(0xFFFFFFFF);
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_u32(), 0u);
    EXPECT_EQ(r.read_u32(), 12345678u);
    EXPECT_EQ(r.read_u32(), 0xFFFFFFFFu);
}

TEST(BitStream, U64) {
    BitWriter w;
    w.write_u64(0);
    w.write_u64(0x123456789ABCDEF0ULL);
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_u64(), 0ULL);
    EXPECT_EQ(r.read_u64(), 0x123456789ABCDEF0ULL);
}

TEST(BitStream, I32) {
    BitWriter w;
    w.write_i32(0);
    w.write_i32(-42);
    w.write_i32(2147483647);
    w.write_i32(-2147483647);
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_i32(), 0);
    EXPECT_EQ(r.read_i32(), -42);
    EXPECT_EQ(r.read_i32(), 2147483647);
    EXPECT_EQ(r.read_i32(), -2147483647);
}

TEST(BitStream, F32) {
    BitWriter w;
    w.write_f32(0.0f);
    w.write_f32(3.14159f);
    w.write_f32(-1.0f);
    w.flush();

    BitReader r(w.data());
    EXPECT_FLOAT_EQ(r.read_f32(), 0.0f);
    EXPECT_FLOAT_EQ(r.read_f32(), 3.14159f);
    EXPECT_FLOAT_EQ(r.read_f32(), -1.0f);
}

TEST(BitStream, String) {
    BitWriter w;
    w.write_string("");
    w.write_string("hello");
    w.write_string("world! 123");
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_string(), "");
    EXPECT_EQ(r.read_string(), "hello");
    EXPECT_EQ(r.read_string(), "world! 123");
}

TEST(BitStream, Bytes) {
    std::vector<u8> data = {0xDE, 0xAD, 0xBE, 0xEF};

    BitWriter w;
    w.write_bytes(data.data(), static_cast<u32>(data.size()));
    w.flush();

    BitReader r(w.data());
    u32 size = r.read_u32();
    auto result = r.read_bytes(size);
    EXPECT_EQ(result, data);
}

TEST(BitStream, RangedValues) {
    BitWriter w;
    w.write_ranged(5, 0, 10);
    w.write_ranged(0, 0, 10);
    w.write_ranged(10, 0, 10);
    w.write_ranged(-3, -5, 5);
    w.flush();

    BitReader r(w.data());
    EXPECT_EQ(r.read_ranged(0, 10), 5);
    EXPECT_EQ(r.read_ranged(0, 10), 0);
    EXPECT_EQ(r.read_ranged(0, 10), 10);
    EXPECT_EQ(r.read_ranged(-5, 5), -3);
}

TEST(BitStream, RangedUsesMinBits) {
    BitWriter w;
    // Range 0-1 needs 1 bit
    w.write_ranged(0, 0, 1);
    w.write_ranged(1, 0, 1);
    w.flush();

    // Should use 2 bits + padding = 1 byte
    EXPECT_LE(w.bytes_used(), 1u);
}

TEST(BitStream, MixedTypes) {
    BitWriter w;
    w.write_bool(true);
    w.write_u8(42);
    w.write_f32(1.5f);
    w.write_string("test");
    w.write_i32(-99);
    w.flush();

    BitReader r(w.data());
    EXPECT_TRUE(r.read_bool());
    EXPECT_EQ(r.read_u8(), 42);
    EXPECT_FLOAT_EQ(r.read_f32(), 1.5f);
    EXPECT_EQ(r.read_string(), "test");
    EXPECT_EQ(r.read_i32(), -99);
}

// =============================================================================
// BitWriter specifics
// =============================================================================

TEST(BitWriter, BitsWritten) {
    BitWriter w;
    w.write_bool(true);
    EXPECT_EQ(w.bits_written(), 1u);

    w.write_u8(0);
    EXPECT_EQ(w.bits_written(), 9u);
}

TEST(BitWriter, BytesUsed) {
    BitWriter w;
    w.write_bool(true);
    EXPECT_EQ(w.bytes_used(), 1u); // 1 bit rounds up

    w.write_u8(0);
    EXPECT_EQ(w.bytes_used(), 2u); // 9 bits rounds up
}

TEST(BitWriter, Reset) {
    BitWriter w;
    w.write_u32(12345);
    w.reset();
    EXPECT_EQ(w.bits_written(), 0u);
    EXPECT_TRUE(w.data().empty());
}

TEST(BitWriter, InitialCapacity) {
    BitWriter w(1024);
    w.write_u32(42);
    EXPECT_EQ(w.bits_written(), 32u);
}

TEST(BitWriter, Flush) {
    BitWriter w;
    w.write_bool(true);
    EXPECT_EQ(w.bits_written(), 1u);
    w.flush();
    EXPECT_EQ(w.bits_written(), 8u); // Padded to byte boundary
}

// =============================================================================
// BitReader specifics
// =============================================================================

TEST(BitReader, Exhausted) {
    u8 data[] = {0xFF};
    BitReader r(data, 1);
    EXPECT_FALSE(r.is_exhausted());

    r.read_u8();
    EXPECT_TRUE(r.is_exhausted());
}

TEST(BitReader, ReadPastEnd) {
    u8 data[] = {0x00};
    BitReader r(data, 1);
    r.read_u8();  // Consumes all 8 bits
    r.read_u8();  // Past end
    EXPECT_TRUE(r.has_error());
}

TEST(BitReader, RemainingBits) {
    u8 data[] = {0x00, 0x00};
    BitReader r(data, 2);
    EXPECT_EQ(r.remaining_bits(), 16u);
    r.read_u8();
    EXPECT_EQ(r.remaining_bits(), 8u);
}

// =============================================================================
// bits_required helper
// =============================================================================

TEST(BitsRequired, Values) {
    EXPECT_EQ(bits_required(1), 1u);
    EXPECT_EQ(bits_required(2), 1u);
    EXPECT_EQ(bits_required(3), 2u);
    EXPECT_EQ(bits_required(4), 2u);
    EXPECT_EQ(bits_required(5), 3u);
    EXPECT_EQ(bits_required(256), 8u);
}

// =============================================================================
// DeltaCompressor
// =============================================================================

TEST(DeltaCompressor, IdenticalData) {
    std::vector<u8> data = {1, 2, 3, 4, 5};
    auto delta = DeltaCompressor::compress(data, data);
    auto result = DeltaCompressor::decompress(data, delta);
    EXPECT_EQ(result, data);
}

TEST(DeltaCompressor, SingleByteChange) {
    std::vector<u8> baseline = {1, 2, 3, 4, 5};
    std::vector<u8> current = {1, 2, 99, 4, 5};

    auto delta = DeltaCompressor::compress(baseline, current);
    auto result = DeltaCompressor::decompress(baseline, delta);
    EXPECT_EQ(result, current);
}

TEST(DeltaCompressor, AllBytesChanged) {
    std::vector<u8> baseline = {0, 0, 0, 0};
    std::vector<u8> current = {1, 2, 3, 4};

    auto delta = DeltaCompressor::compress(baseline, current);
    auto result = DeltaCompressor::decompress(baseline, delta);
    EXPECT_EQ(result, current);
}

TEST(DeltaCompressor, EmptyBaseline) {
    std::vector<u8> baseline;
    std::vector<u8> current = {1, 2, 3};

    auto delta = DeltaCompressor::compress(baseline, current);
    auto result = DeltaCompressor::decompress(baseline, delta);
    EXPECT_EQ(result, current);
}

TEST(DeltaCompressor, EmptyCurrent) {
    std::vector<u8> baseline = {1, 2, 3};
    std::vector<u8> current;

    auto delta = DeltaCompressor::compress(baseline, current);
    auto result = DeltaCompressor::decompress(baseline, delta);
    EXPECT_EQ(result, current);
}

TEST(DeltaCompressor, Ratio) {
    std::vector<u8> original(100, 0);
    std::vector<u8> compressed(25, 0);
    EXPECT_NEAR(DeltaCompressor::ratio(original, compressed), 0.25f, 0.01f);
}

TEST(DeltaCompressor, RatioEmpty) {
    EXPECT_NEAR(DeltaCompressor::ratio({}, {}), 0.0f, 0.01f);
}
