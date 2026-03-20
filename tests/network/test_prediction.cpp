#include <gtest/gtest.h>
#include "nexus/net/prediction.h"
#include <cstring>

using namespace nexus;
using namespace nexus::net;

// Helper: encode a float as bytes
static std::vector<u8> float_to_bytes(f32 val) {
    std::vector<u8> bytes(sizeof(f32));
    std::memcpy(bytes.data(), &val, sizeof(f32));
    return bytes;
}

static f32 bytes_to_float(const std::vector<u8>& bytes) {
    f32 val = 0.0f;
    if (bytes.size() >= sizeof(f32)) {
        std::memcpy(&val, bytes.data(), sizeof(f32));
    }
    return val;
}

// =============================================================================
// ClientPrediction
// =============================================================================

TEST(ClientPrediction, RecordInput) {
    ClientPrediction pred;
    pred.record_input({1}, float_to_bytes(1.0f));
    EXPECT_EQ(pred.pending_input_count(), 1u);
    EXPECT_EQ(pred.current_sequence(), 1u);

    pred.record_input({2}, float_to_bytes(2.0f));
    EXPECT_EQ(pred.pending_input_count(), 2u);
    EXPECT_EQ(pred.current_sequence(), 2u);
}

TEST(ClientPrediction, PredictedState) {
    ClientPrediction pred;
    auto state = float_to_bytes(5.0f);
    pred.record_input({1}, state);
    EXPECT_NEAR(bytes_to_float(pred.predicted_state()), 5.0f, 0.001f);
}

TEST(ClientPrediction, ReconcileCorrect) {
    ClientPrediction pred;
    auto state = float_to_bytes(1.0f);
    pred.record_input({1}, state);

    // Server confirms: prediction was correct
    bool corrected = pred.reconcile(1, state);
    EXPECT_FALSE(corrected);
    EXPECT_EQ(pred.pending_input_count(), 0u);
}

TEST(ClientPrediction, ReconcileIncorrect) {
    ClientPrediction pred;

    pred.set_apply_input([](const std::vector<u8>& current,
                            const std::vector<u8>&) {
        return current; // No-op for simplicity
    });

    pred.record_input({1}, float_to_bytes(1.0f));
    pred.record_input({2}, float_to_bytes(2.0f));

    // Server says after input 1 the state should be 1.5 (not 1.0)
    bool corrected = pred.reconcile(1, float_to_bytes(1.5f));
    EXPECT_TRUE(corrected);
    EXPECT_EQ(pred.correction_count(), 1u);

    // Input 1 removed, input 2 still pending
    EXPECT_EQ(pred.pending_input_count(), 1u);
}

TEST(ClientPrediction, ReconcileRemovesPastInputs) {
    ClientPrediction pred;
    pred.record_input({1}, float_to_bytes(1.0f));
    pred.record_input({2}, float_to_bytes(2.0f));
    pred.record_input({3}, float_to_bytes(3.0f));

    pred.reconcile(2, float_to_bytes(2.0f));
    EXPECT_EQ(pred.pending_input_count(), 1u); // Only input 3 remains
}

TEST(ClientPrediction, Reset) {
    ClientPrediction pred;
    pred.record_input({1}, float_to_bytes(1.0f));
    pred.reset();

    EXPECT_EQ(pred.pending_input_count(), 0u);
    EXPECT_TRUE(pred.predicted_state().empty());
    EXPECT_EQ(pred.correction_count(), 0u);
}

TEST(ClientPrediction, CustomStatesEqual) {
    ClientPrediction pred;

    pred.set_states_equal([](const std::vector<u8>& a,
                             const std::vector<u8>& b) {
        // Consider equal if within tolerance
        f32 va = bytes_to_float(a);
        f32 vb = bytes_to_float(b);
        return std::abs(va - vb) < 0.1f;
    });

    pred.record_input({1}, float_to_bytes(1.0f));

    // Close enough — no correction
    bool corrected = pred.reconcile(1, float_to_bytes(1.05f));
    EXPECT_FALSE(corrected);
}

// =============================================================================
// InterpolationBuffer
// =============================================================================

TEST(InterpolationBuffer, Empty) {
    InterpolationBuffer buf;
    auto state = buf.sample(1.0f);
    EXPECT_TRUE(state.empty());
}

TEST(InterpolationBuffer, SingleState) {
    InterpolationBuffer buf;
    buf.push_state(0.0f, float_to_bytes(42.0f));

    auto state = buf.sample(0.0f);
    EXPECT_NEAR(bytes_to_float(state), 42.0f, 0.001f);
}

TEST(InterpolationBuffer, ReturnOldestBeforeBuffer) {
    InterpolationBuffer buf;
    buf.set_delay(0.0f);
    buf.push_state(1.0f, float_to_bytes(10.0f));
    buf.push_state(2.0f, float_to_bytes(20.0f));

    auto state = buf.sample(0.5f); // Before all samples
    EXPECT_NEAR(bytes_to_float(state), 10.0f, 0.001f);
}

TEST(InterpolationBuffer, ReturnNewestAfterBuffer) {
    InterpolationBuffer buf;
    buf.set_delay(0.0f);
    buf.push_state(1.0f, float_to_bytes(10.0f));
    buf.push_state(2.0f, float_to_bytes(20.0f));

    auto state = buf.sample(5.0f); // After all samples
    EXPECT_NEAR(bytes_to_float(state), 20.0f, 0.001f);
}

TEST(InterpolationBuffer, InterpolateWithCallback) {
    InterpolationBuffer buf;
    buf.set_delay(0.0f);

    buf.set_interpolate([](const std::vector<u8>& from,
                           const std::vector<u8>& to,
                           f32 t) {
        f32 a = bytes_to_float(from);
        f32 b = bytes_to_float(to);
        return float_to_bytes(a + (b - a) * t);
    });

    buf.push_state(0.0f, float_to_bytes(0.0f));
    buf.push_state(1.0f, float_to_bytes(10.0f));

    auto mid = buf.sample(0.5f);
    EXPECT_NEAR(bytes_to_float(mid), 5.0f, 0.1f);
}

TEST(InterpolationBuffer, DelayOffset) {
    InterpolationBuffer buf;
    buf.set_delay(0.1f); // 100ms behind

    buf.push_state(0.0f, float_to_bytes(0.0f));
    buf.push_state(0.5f, float_to_bytes(50.0f));
    buf.push_state(1.0f, float_to_bytes(100.0f));

    // At render_time = 0.6, target = 0.6 - 0.1 = 0.5
    auto state = buf.sample(0.6f);
    EXPECT_NEAR(bytes_to_float(state), 50.0f, 0.1f);
}

TEST(InterpolationBuffer, BufferSize) {
    InterpolationBuffer buf;
    EXPECT_EQ(buf.buffer_size(), 0u);

    buf.push_state(0.0f, {});
    buf.push_state(1.0f, {});
    EXPECT_EQ(buf.buffer_size(), 2u);
}

TEST(InterpolationBuffer, Clear) {
    InterpolationBuffer buf;
    buf.push_state(0.0f, {1});
    buf.push_state(1.0f, {2});
    buf.clear();
    EXPECT_EQ(buf.buffer_size(), 0u);
}

TEST(InterpolationBuffer, DefaultDelay) {
    InterpolationBuffer buf;
    EXPECT_NEAR(buf.delay(), 0.1f, 0.001f);
}

// =============================================================================
// InputSnapshot
// =============================================================================

TEST(InputSnapshot, Default) {
    InputSnapshot snap;
    EXPECT_EQ(snap.sequence, 0u);
    EXPECT_NEAR(snap.timestamp, 0.0f, 0.001f);
    EXPECT_TRUE(snap.input_data.empty());
}
