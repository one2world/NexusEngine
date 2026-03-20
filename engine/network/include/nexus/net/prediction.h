#pragma once

#include "nexus/core/types.h"
#include <vector>
#include <deque>
#include <functional>

namespace nexus::net {

// ─────────────────────────────────────────────────────────────────────────────
// Client-side prediction and server reconciliation
// ─────────────────────────────────────────────────────────────────────────────

/// An input snapshot captured on the client
struct InputSnapshot {
    u32 sequence{0};         // Monotonic input sequence number
    f32 timestamp{0.0f};     // Client time when input was captured
    std::vector<u8> input_data; // Serialized input state
};

/// State snapshot for reconciliation
struct StateSnapshot {
    u32 sequence{0};         // Corresponds to InputSnapshot sequence
    std::vector<u8> state_data; // Serialized state
};

/// Callback to apply an input to a state and return the resulting state
using ApplyInputFn = std::function<std::vector<u8>(
    const std::vector<u8>& current_state,
    const std::vector<u8>& input_data)>;

/// Callback to compare two states for equality (within tolerance)
using StatesEqualFn = std::function<bool(
    const std::vector<u8>& a,
    const std::vector<u8>& b)>;

// ─────────────────────────────────────────────────────────────────────────────
// ClientPrediction — handles client-side prediction with input buffering
// ─────────────────────────────────────────────────────────────────────────────

class ClientPrediction {
public:
    ClientPrediction() = default;

    /// Set the function to apply input to state.
    void set_apply_input(ApplyInputFn fn) { apply_input_ = std::move(fn); }

    /// Set the function to compare states.
    void set_states_equal(StatesEqualFn fn) { states_equal_ = std::move(fn); }

    /// Record a new input and predict the resulting state.
    void record_input(const std::vector<u8>& input_data,
                      const std::vector<u8>& predicted_state);

    /// Receive authoritative state from server for a given input sequence.
    /// Returns true if reconciliation was needed (prediction was wrong).
    bool reconcile(u32 acked_sequence, const std::vector<u8>& server_state);

    /// Get the current predicted state.
    const std::vector<u8>& predicted_state() const { return predicted_state_; }

    /// Number of unacknowledged inputs.
    u32 pending_input_count() const { return static_cast<u32>(pending_inputs_.size()); }

    /// Get all pending inputs (for sending to server).
    const std::deque<InputSnapshot>& pending_inputs() const { return pending_inputs_; }

    /// Current input sequence number.
    u32 current_sequence() const { return next_sequence_ - 1; }

    /// Number of corrections applied.
    u32 correction_count() const { return corrections_; }

    /// Reset prediction state.
    void reset();

private:
    ApplyInputFn apply_input_;
    StatesEqualFn states_equal_;
    std::deque<InputSnapshot> pending_inputs_;
    std::vector<u8> predicted_state_;
    u32 next_sequence_{1};
    u32 corrections_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// InterpolationBuffer — smoothly interpolates between server snapshots
// ─────────────────────────────────────────────────────────────────────────────

struct TimestampedState {
    f32 timestamp{0.0f};
    std::vector<u8> state;
};

/// Callback to interpolate between two states
using InterpolateFn = std::function<std::vector<u8>(
    const std::vector<u8>& from,
    const std::vector<u8>& to,
    f32 t)>;

class InterpolationBuffer {
public:
    InterpolationBuffer() = default;

    /// Set interpolation delay (how far behind real-time to render).
    void set_delay(f32 seconds) { delay_ = seconds; }
    f32 delay() const { return delay_; }

    /// Set the interpolation function.
    void set_interpolate(InterpolateFn fn) { interpolate_ = std::move(fn); }

    /// Add a new server state snapshot.
    void push_state(f32 server_time, const std::vector<u8>& state);

    /// Get interpolated state at the given render time.
    std::vector<u8> sample(f32 render_time) const;

    /// Number of buffered states.
    u32 buffer_size() const { return static_cast<u32>(buffer_.size()); }

    /// Clear the buffer.
    void clear();

private:
    std::deque<TimestampedState> buffer_;
    InterpolateFn interpolate_;
    f32 delay_{0.1f}; // 100ms default
    static constexpr u32 MAX_BUFFER_SIZE = 64;
};

} // namespace nexus::net
