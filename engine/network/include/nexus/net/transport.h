#pragma once

#include "nexus/core/types.h"
#include <string>
#include <vector>
#include <functional>
#include <deque>
#include <chrono>
#include <unordered_map>
#include <random>

namespace nexus::net {

// ─────────────────────────────────────────────────────────────────────────────
// Network address
// ─────────────────────────────────────────────────────────────────────────────

struct Address {
    std::string host{"127.0.0.1"};
    u16 port{0};

    Address() = default;
    Address(std::string h, u16 p) : host(std::move(h)), port(p) {}

    bool operator==(const Address& o) const { return host == o.host && port == o.port; }
    bool operator!=(const Address& o) const { return !(*this == o); }

    std::string to_string() const { return host + ":" + std::to_string(port); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Packet — a chunk of data sent over the network
// ─────────────────────────────────────────────────────────────────────────────

enum class PacketFlag : u8 {
    None       = 0,
    Reliable   = 1 << 0,   // Guaranteed delivery
    Ordered    = 1 << 1,   // Delivered in order
    Compressed = 1 << 2,   // Data is compressed
};

inline PacketFlag operator|(PacketFlag a, PacketFlag b) {
    return static_cast<PacketFlag>(static_cast<u8>(a) | static_cast<u8>(b));
}
inline bool operator&(PacketFlag a, PacketFlag b) {
    return (static_cast<u8>(a) & static_cast<u8>(b)) != 0;
}

struct PacketHeader {
    u32 sequence{0};       // Sequence number
    u32 ack{0};            // Last acknowledged packet from remote
    u32 ack_bits{0};       // Bitfield of previous 32 acks
    u16 payload_size{0};
    u8 channel{0};         // Channel ID
    PacketFlag flags{PacketFlag::None};
};

struct Packet {
    PacketHeader header;
    std::vector<u8> payload;

    u32 total_size() const { return static_cast<u32>(sizeof(PacketHeader) + payload.size()); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Connection — represents a connection to a remote peer
// ─────────────────────────────────────────────────────────────────────────────

enum class ConnectionState : u8 {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
};

struct ConnectionStats {
    u32 packets_sent{0};
    u32 packets_received{0};
    u32 packets_lost{0};
    u32 bytes_sent{0};
    u32 bytes_received{0};
    f32 round_trip_time{0.0f};   // RTT in seconds
    f32 packet_loss{0.0f};       // 0.0 - 1.0
    f32 bandwidth_in{0.0f};      // bytes/sec
    f32 bandwidth_out{0.0f};     // bytes/sec
};

class Connection {
public:
    Connection() = default;
    Connection(u32 id, const Address& remote);

    u32 id() const { return id_; }
    const Address& remote_address() const { return remote_; }
    ConnectionState state() const { return state_; }
    const ConnectionStats& stats() const { return stats_; }

    void set_state(ConnectionState state) { state_ = state; }

    /// Send a packet to this connection.
    void send(const std::vector<u8>& data, PacketFlag flags = PacketFlag::Reliable);

    /// Queue data to be sent on next flush.
    void queue_send(const std::vector<u8>& data, PacketFlag flags = PacketFlag::Reliable);

    /// Get queued outgoing packets.
    std::vector<Packet>& outgoing() { return outgoing_; }
    const std::vector<Packet>& outgoing() const { return outgoing_; }

    /// Push a received packet.
    void push_received(Packet packet);

    /// Drain received packets.
    std::vector<Packet> drain_received();

    /// Acknowledge a sequence number.
    void acknowledge(u32 sequence);

    /// Is a sequence number acknowledged?
    bool is_acknowledged(u32 sequence) const;

    /// Get the next outgoing sequence number.
    u32 next_sequence();

    /// Update connection stats.
    void update_stats(f32 dt);

    /// Time since last packet received.
    f32 time_since_last_recv() const { return time_since_recv_; }

    /// Reset the recv timer.
    void reset_recv_timer() { time_since_recv_ = 0.0f; }

    /// Update recv timer.
    void tick_recv_timer(f32 dt) { time_since_recv_ += dt; }

private:
    u32 id_{0};
    Address remote_;
    ConnectionState state_{ConnectionState::Disconnected};
    ConnectionStats stats_;
    std::vector<Packet> outgoing_;
    std::vector<Packet> received_;
    u32 local_sequence_{0};
    u32 remote_sequence_{0};
    u32 ack_bits_{0};
    f32 time_since_recv_{0.0f};
    f32 rtt_accumulator_{0.0f};
    u32 rtt_samples_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// NetworkSimulation — artificial latency, packet loss, jitter for debugging
// ─────────────────────────────────────────────────────────────────────────────

struct SimulationConfig {
    f32 latency_ms{0.0f};        // One-way latency in milliseconds
    f32 jitter_ms{0.0f};         // Random jitter ± ms
    f32 packet_loss{0.0f};       // 0.0 - 1.0 (fraction of packets dropped)
    f32 duplicate_chance{0.0f};  // 0.0 - 1.0 (fraction of packets duplicated)

    bool is_enabled() const {
        return latency_ms > 0.0f || jitter_ms > 0.0f ||
               packet_loss > 0.0f || duplicate_chance > 0.0f;
    }
};

class NetworkSimulation {
public:
    NetworkSimulation();

    void set_config(const SimulationConfig& config) { config_ = config; }
    const SimulationConfig& config() const { return config_; }

    /// Process a packet through simulation. Returns packets that should be
    /// delivered now (may be empty if delayed, or contain duplicates).
    std::vector<Packet> process(Packet packet, f32 current_time);

    /// Update: release any delayed packets whose time has come.
    std::vector<Packet> update(f32 current_time);

    /// Number of packets currently delayed.
    u32 pending_count() const { return static_cast<u32>(delayed_.size()); }

    /// Reset all state.
    void reset();

private:
    struct DelayedPacket {
        Packet packet;
        f32 deliver_time;
    };

    SimulationConfig config_;
    std::deque<DelayedPacket> delayed_;
    std::mt19937 rng_;
};

} // namespace nexus::net
