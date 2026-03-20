#include <gtest/gtest.h>
#include "nexus/net/transport.h"

using namespace nexus;
using namespace nexus::net;

// =============================================================================
// Address
// =============================================================================

TEST(Address, Default) {
    Address addr;
    EXPECT_EQ(addr.host, "127.0.0.1");
    EXPECT_EQ(addr.port, 0);
}

TEST(Address, Constructor) {
    Address addr("192.168.1.1", 7777);
    EXPECT_EQ(addr.host, "192.168.1.1");
    EXPECT_EQ(addr.port, 7777);
}

TEST(Address, Equality) {
    Address a("10.0.0.1", 8080);
    Address b("10.0.0.1", 8080);
    Address c("10.0.0.2", 8080);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(Address, ToString) {
    Address addr("localhost", 1234);
    EXPECT_EQ(addr.to_string(), "localhost:1234");
}

// =============================================================================
// PacketFlag
// =============================================================================

TEST(PacketFlag, BitwiseOr) {
    auto flags = PacketFlag::Reliable | PacketFlag::Ordered;
    EXPECT_TRUE(flags & PacketFlag::Reliable);
    EXPECT_TRUE(flags & PacketFlag::Ordered);
    EXPECT_FALSE(flags & PacketFlag::Compressed);
}

// =============================================================================
// Packet
// =============================================================================

TEST(Packet, TotalSize) {
    Packet pkt;
    pkt.payload = {1, 2, 3, 4, 5};
    EXPECT_EQ(pkt.total_size(), sizeof(PacketHeader) + 5);
}

// =============================================================================
// Connection
// =============================================================================

TEST(Connection, Construction) {
    Connection conn(1, Address("10.0.0.1", 7777));
    EXPECT_EQ(conn.id(), 1u);
    EXPECT_EQ(conn.remote_address().host, "10.0.0.1");
    EXPECT_EQ(conn.remote_address().port, 7777);
    EXPECT_EQ(conn.state(), ConnectionState::Connecting);
}

TEST(Connection, SetState) {
    Connection conn(1, Address());
    conn.set_state(ConnectionState::Connected);
    EXPECT_EQ(conn.state(), ConnectionState::Connected);
}

TEST(Connection, SendCreatesPacket) {
    Connection conn(1, Address());
    conn.send({1, 2, 3}, PacketFlag::Reliable);
    EXPECT_EQ(conn.outgoing().size(), 1u);
    EXPECT_EQ(conn.outgoing()[0].payload.size(), 3u);
    EXPECT_EQ(conn.stats().packets_sent, 1u);
}

TEST(Connection, SequenceNumbers) {
    Connection conn(1, Address());
    conn.send({1}, PacketFlag::None);
    conn.send({2}, PacketFlag::None);
    EXPECT_EQ(conn.outgoing()[0].header.sequence, 1u);
    EXPECT_EQ(conn.outgoing()[1].header.sequence, 2u);
}

TEST(Connection, ReceivePacket) {
    Connection conn(1, Address());
    Packet pkt;
    pkt.header.sequence = 1;
    pkt.payload = {10, 20, 30};
    conn.push_received(std::move(pkt));

    EXPECT_EQ(conn.stats().packets_received, 1u);
    auto received = conn.drain_received();
    EXPECT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].payload.size(), 3u);
}

TEST(Connection, DrainClearsReceived) {
    Connection conn(1, Address());
    Packet pkt;
    pkt.header.sequence = 1;
    conn.push_received(std::move(pkt));

    conn.drain_received();
    auto second = conn.drain_received();
    EXPECT_TRUE(second.empty());
}

TEST(Connection, AckTracking) {
    Connection conn(1, Address());

    Packet p1; p1.header.sequence = 1;
    Packet p2; p2.header.sequence = 2;
    Packet p3; p3.header.sequence = 3;

    conn.push_received(std::move(p1));
    conn.push_received(std::move(p2));
    conn.push_received(std::move(p3));

    EXPECT_TRUE(conn.is_acknowledged(3));
    EXPECT_TRUE(conn.is_acknowledged(2));
    EXPECT_TRUE(conn.is_acknowledged(1));
}

TEST(Connection, RecvTimer) {
    Connection conn(1, Address());
    EXPECT_NEAR(conn.time_since_last_recv(), 0.0f, 0.001f);

    conn.tick_recv_timer(1.0f);
    EXPECT_NEAR(conn.time_since_last_recv(), 1.0f, 0.001f);

    conn.reset_recv_timer();
    EXPECT_NEAR(conn.time_since_last_recv(), 0.0f, 0.001f);
}

TEST(Connection, StatsUpdate) {
    Connection conn(1, Address());
    conn.send({1, 2}, PacketFlag::None);
    conn.update_stats(1.0f);
    EXPECT_GT(conn.stats().bandwidth_out, 0.0f);
}

// =============================================================================
// NetworkSimulation
// =============================================================================

TEST(NetworkSimulation, PassthroughWhenDisabled) {
    NetworkSimulation sim;
    // Default config has all zeros = disabled
    Packet pkt;
    pkt.payload = {1, 2, 3};

    auto result = sim.process(std::move(pkt), 0.0f);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].payload.size(), 3u);
}

TEST(NetworkSimulation, DelayPackets) {
    NetworkSimulation sim;
    SimulationConfig cfg;
    cfg.latency_ms = 100.0f;
    sim.set_config(cfg);

    Packet pkt;
    pkt.payload = {42};

    auto immediate = sim.process(std::move(pkt), 0.0f);
    EXPECT_TRUE(immediate.empty()); // Should be delayed

    EXPECT_EQ(sim.pending_count(), 1u);

    // Not enough time
    auto early = sim.update(0.05f);
    EXPECT_TRUE(early.empty());

    // Enough time
    auto delivered = sim.update(0.15f);
    EXPECT_EQ(delivered.size(), 1u);
    EXPECT_EQ(delivered[0].payload[0], 42);
}

TEST(NetworkSimulation, PacketLoss) {
    NetworkSimulation sim;
    SimulationConfig cfg;
    cfg.packet_loss = 1.0f; // Drop everything
    sim.set_config(cfg);

    Packet pkt;
    pkt.payload = {1};

    auto result = sim.process(std::move(pkt), 0.0f);
    EXPECT_TRUE(result.empty());
}

TEST(NetworkSimulation, Reset) {
    NetworkSimulation sim;
    SimulationConfig cfg;
    cfg.latency_ms = 1000.0f;
    sim.set_config(cfg);

    Packet pkt;
    sim.process(std::move(pkt), 0.0f);
    EXPECT_EQ(sim.pending_count(), 1u);

    sim.reset();
    EXPECT_EQ(sim.pending_count(), 0u);
}

TEST(NetworkSimulation, ConfigEnabled) {
    SimulationConfig cfg;
    EXPECT_FALSE(cfg.is_enabled());

    cfg.latency_ms = 50.0f;
    EXPECT_TRUE(cfg.is_enabled());
}

TEST(NetworkSimulation, ZeroLatencyPassthrough) {
    NetworkSimulation sim;
    SimulationConfig cfg;
    cfg.packet_loss = 0.0f;
    cfg.latency_ms = 0.0f;
    // jitter and duplicate are 0 but loss being 0 means config is disabled
    sim.set_config(cfg);

    Packet pkt;
    pkt.payload = {99};
    auto result = sim.process(std::move(pkt), 0.0f);
    EXPECT_EQ(result.size(), 1u);
}
