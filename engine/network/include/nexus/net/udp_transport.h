#pragma once

#include "nexus/net/transport.h"
#include "nexus/core/types.h"

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <functional>

namespace nexus::net {

// ─────────────────────────────────────────────────────────────────────────────
// UdpTransport — game-level UDP transport with connection management,
//                reliable delivery, and keep-alive built on top of UDPSocket.
// ─────────────────────────────────────────────────────────────────────────────

using ConnectionId = u32;

class UdpTransport {
public:
    UdpTransport();
    ~UdpTransport();

    // Non-copyable, non-movable
    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;
    UdpTransport(UdpTransport&&) = delete;
    UdpTransport& operator=(UdpTransport&&) = delete;

    // Server mode: bind to port and listen for connections
    bool listen(u16 port);

    // Client mode: connect to a remote server
    bool connect(const std::string& host, u16 port);

    // Close the socket and all connections
    void close();

    // Send raw data to a specific connection
    bool send(ConnectionId conn, const void* data, u32 size,
              PacketFlag flags = PacketFlag::None);

    // Send raw data to all connections
    void broadcast(const void* data, u32 size,
                   PacketFlag flags = PacketFlag::None);

    // Poll for incoming packets and process events (call each frame)
    void poll();

    // State queries
    [[nodiscard]] bool is_listening() const { return listening_; }
    [[nodiscard]] bool is_connected() const { return connected_; }
    [[nodiscard]] u32 connection_count() const;

    // Event callbacks
    using ConnectCallback    = std::function<void(ConnectionId)>;
    using DisconnectCallback = std::function<void(ConnectionId)>;
    using ReceiveCallback    = std::function<void(ConnectionId, const void*, u32)>;

    void on_connect(ConnectCallback cb)       { on_connect_ = std::move(cb); }
    void on_disconnect(DisconnectCallback cb)  { on_disconnect_ = std::move(cb); }
    void on_receive(ReceiveCallback cb)        { on_receive_ = std::move(cb); }

private:
    // ── Internal protocol ────────────────────────────────────────────────
    static constexpr u32 PROTOCOL_ID       = 0x4E584E54; // "NXNT"
    static constexpr u32 MAX_PACKET_SIZE   = 1400;       // MTU-safe
    static constexpr f32 CONNECTION_TIMEOUT = 10.0f;     // seconds
    static constexpr f32 RETRANSMIT_INTERVAL = 0.1f;     // seconds
    static constexpr u32 MAX_RECV_PER_POLL = 256;

    enum class PacketType : u8 {
        ConnectionRequest = 1,
        ConnectionAccept  = 2,
        ConnectionDeny    = 3,
        Disconnect        = 4,
        Data              = 5,
        DataReliable      = 6,
        Ack               = 7,
        Ping              = 8,
        Pong              = 9,
    };

    // Wire header: [protocol_id(4)][type(1)][sequence(4)][ack(4)][ack_bits(4)]
    // Total: 17 bytes
    struct ProtocolHeader {
        u32 protocol_id;
        PacketType type;
        u32 sequence;
        u32 ack;
        u32 ack_bits;
    };
    static constexpr u32 HEADER_SIZE = 17; // Serialized size on the wire

    // ── Per-peer state ───────────────────────────────────────────────────
    struct PendingPacket {
        u32 sequence;
        std::vector<u8> data;
        std::chrono::steady_clock::time_point send_time;
        u32 retransmit_count{0};
    };

    struct RemotePeer {
        Address address;
        ConnectionId id{0};
        std::chrono::steady_clock::time_point last_recv;
        // Sequence tracking
        u32 local_sequence{0};
        u32 remote_sequence{0};
        u32 ack_bits{0};
        // Reliable delivery
        std::vector<PendingPacket> pending_reliable;
    };

    // ── Core state ───────────────────────────────────────────────────────
    UDPSocket socket_;
    bool listening_{false};
    bool connected_{false};

    // Peer tracking: key = packed address+port
    std::unordered_map<u64, RemotePeer> peers_;
    ConnectionId next_conn_id_{1};
    std::mutex mutex_;

    // For client mode: the server peer key
    u64 server_key_{0};

    // Callbacks
    ConnectCallback    on_connect_;
    DisconnectCallback on_disconnect_;
    ReceiveCallback    on_receive_;

    // ── Helpers ──────────────────────────────────────────────────────────
    static u64 make_peer_key(const Address& addr);
    RemotePeer* find_peer(u64 key);
    RemotePeer* find_peer_by_id(ConnectionId id);

    void send_raw(const Address& dest, const void* data, u32 size);
    void send_protocol(const Address& dest, PacketType type,
                       u32 sequence, u32 ack, u32 ack_bits,
                       const void* payload = nullptr, u32 payload_size = 0);

    void process_packet(const Address& from, const u8* data, u32 size);
    void handle_connection_request(const Address& from, const ProtocolHeader& header);
    void handle_connection_accept(const ProtocolHeader& header);
    void handle_disconnect(const Address& from);
    void handle_data(RemotePeer& peer, const u8* payload, u32 size, bool reliable,
                     const ProtocolHeader& header);
    void handle_ack(RemotePeer& peer, const ProtocolHeader& header);
    void handle_ping(const Address& from, const ProtocolHeader& header);

    void update_remote_ack(RemotePeer& peer, u32 ack, u32 ack_bits);
    void check_timeouts();
    void retransmit_reliable();

    // Header serialization
    static void write_header(u8* buf, const ProtocolHeader& hdr);
    static bool read_header(const u8* buf, u32 size, ProtocolHeader& hdr);
};

} // namespace nexus::net
