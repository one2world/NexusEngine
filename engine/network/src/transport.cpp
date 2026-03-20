#include "nexus/net/transport.h"
#include <algorithm>
#include <chrono>

namespace nexus::net {

// ── Connection ──────────────────────────────────────────────────────────────

Connection::Connection(u32 id, const Address& remote)
    : id_(id), remote_(remote), state_(ConnectionState::Connecting) {}

void Connection::send(const std::vector<u8>& data, PacketFlag flags) {
    Packet pkt;
    pkt.header.sequence = next_sequence();
    pkt.header.payload_size = static_cast<u16>(data.size());
    pkt.header.flags = flags;
    pkt.header.ack = remote_sequence_;
    pkt.header.ack_bits = ack_bits_;
    pkt.payload = data;

    outgoing_.push_back(std::move(pkt));
    stats_.packets_sent++;
    stats_.bytes_sent += static_cast<u32>(data.size() + sizeof(PacketHeader));
}

void Connection::queue_send(const std::vector<u8>& data, PacketFlag flags) {
    send(data, flags);
}

void Connection::push_received(Packet packet) {
    u32 seq = packet.header.sequence;

    stats_.packets_received++;
    stats_.bytes_received += packet.total_size();

    // Update remote sequence and ack bits
    if (seq > remote_sequence_) {
        u32 shift = seq - remote_sequence_;
        if (shift <= 32) {
            ack_bits_ <<= shift;
            ack_bits_ |= 1u; // Mark the old remote_sequence as acked
        } else {
            ack_bits_ = 1u;
        }
        remote_sequence_ = seq;
    } else {
        u32 diff = remote_sequence_ - seq;
        if (diff > 0 && diff <= 32) {
            ack_bits_ |= (1u << diff);
        }
    }

    reset_recv_timer();
    received_.push_back(std::move(packet));
}

std::vector<Packet> Connection::drain_received() {
    std::vector<Packet> result;
    std::swap(result, received_);
    return result;
}

void Connection::acknowledge(u32 sequence) {
    // Nothing special needed — ack tracking is done via ack_bits
    (void)sequence;
}

bool Connection::is_acknowledged(u32 sequence) const {
    if (sequence == remote_sequence_) return true;
    u32 diff = remote_sequence_ - sequence;
    if (diff > 0 && diff <= 32) {
        return (ack_bits_ & (1u << diff)) != 0;
    }
    return false;
}

u32 Connection::next_sequence() {
    return ++local_sequence_;
}

void Connection::update_stats(f32 dt) {
    // Simple bandwidth estimation
    if (dt > 0.0f) {
        stats_.bandwidth_out = static_cast<f32>(stats_.bytes_sent) / dt;
        stats_.bandwidth_in = static_cast<f32>(stats_.bytes_received) / dt;
    }

    // Packet loss estimation
    u32 total = stats_.packets_sent + stats_.packets_received;
    if (total > 0) {
        stats_.packet_loss = static_cast<f32>(stats_.packets_lost) /
                            static_cast<f32>(total);
    }
}

// ── NetworkSimulation ───────────────────────────────────────────────────────

NetworkSimulation::NetworkSimulation()
    : rng_(static_cast<u32>(std::chrono::steady_clock::now()
                                .time_since_epoch().count())) {}

std::vector<Packet> NetworkSimulation::process(Packet packet, f32 current_time) {
    std::vector<Packet> result;

    if (!config_.is_enabled()) {
        result.push_back(std::move(packet));
        return result;
    }

    // Packet loss check
    if (config_.packet_loss > 0.0f) {
        std::uniform_real_distribution<f32> dist(0.0f, 1.0f);
        if (dist(rng_) < config_.packet_loss) {
            return result; // Dropped
        }
    }

    // Calculate delay
    f32 delay = config_.latency_ms / 1000.0f;
    if (config_.jitter_ms > 0.0f) {
        std::uniform_real_distribution<f32> jitter(
            -config_.jitter_ms / 1000.0f,
             config_.jitter_ms / 1000.0f);
        delay += jitter(rng_);
        if (delay < 0.0f) delay = 0.0f;
    }

    // Check for duplication
    bool duplicate = false;
    if (config_.duplicate_chance > 0.0f) {
        std::uniform_real_distribution<f32> dist(0.0f, 1.0f);
        duplicate = dist(rng_) < config_.duplicate_chance;
    }

    if (delay > 0.001f) {
        f32 deliver_time = current_time + delay;
        if (duplicate) {
            Packet dup;
            dup.header = packet.header;
            dup.payload = packet.payload;
            delayed_.push_back({std::move(dup), deliver_time + delay});
        }
        delayed_.push_back({std::move(packet), deliver_time});
    } else {
        if (duplicate) {
            Packet dup;
            dup.header = packet.header;
            dup.payload = packet.payload;
            result.push_back(std::move(dup));
        }
        result.push_back(std::move(packet));
    }

    return result;
}

std::vector<Packet> NetworkSimulation::update(f32 current_time) {
    std::vector<Packet> result;

    while (!delayed_.empty() && delayed_.front().deliver_time <= current_time) {
        result.push_back(std::move(delayed_.front().packet));
        delayed_.pop_front();
    }

    return result;
}

void NetworkSimulation::reset() {
    delayed_.clear();
}

} // namespace nexus::net
