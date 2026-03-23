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

} // namespace nexus::net (temporarily close for platform includes)

// ── Platform socket includes ────────────────────────────────────────────────

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SOCKET_ERROR_CODE WSAGetLastError()
    using socklen_t = int;
    static bool s_winsock_initialized = false;
    static void init_winsock() {
        if (!s_winsock_initialized) {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
            s_winsock_initialized = true;
        }
    }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define CLOSE_SOCKET(s) ::close(s)
    #define SOCKET_ERROR_CODE errno
    static void init_winsock() {}
#endif

namespace nexus::net {

// ── UDPSocket ───────────────────────────────────────────────────────────────

static struct sockaddr_in to_sockaddr(const nexus::net::Address& addr) {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(addr.port);
    inet_pton(AF_INET, addr.host.c_str(), &sa.sin_addr);
    return sa;
}

static nexus::net::Address from_sockaddr(const struct sockaddr_in& sa) {
    char buf[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &sa.sin_addr, buf, sizeof(buf));
    return nexus::net::Address(buf, ntohs(sa.sin_port));
}

UDPSocket::UDPSocket() { init_winsock(); }

UDPSocket::~UDPSocket() { close(); }

UDPSocket::UDPSocket(UDPSocket&& other) noexcept : socket_fd_(other.socket_fd_) {
    other.socket_fd_ = -1;
}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_fd_ = other.socket_fd_;
        other.socket_fd_ = -1;
    }
    return *this;
}

bool UDPSocket::open() {
    close();
    socket_fd_ = static_cast<int>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    return socket_fd_ >= 0;
}

bool UDPSocket::bind(const Address& local) {
    if (socket_fd_ < 0) return false;
    auto sa = to_sockaddr(local);
    return ::bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == 0;
}

void UDPSocket::close() {
    if (socket_fd_ >= 0) {
        CLOSE_SOCKET(socket_fd_);
        socket_fd_ = -1;
    }
}

i32 UDPSocket::send_to(const Address& dest, const void* data, u32 size) {
    if (socket_fd_ < 0) return -1;
    auto sa = to_sockaddr(dest);
    auto result = ::sendto(socket_fd_, static_cast<const char*>(data), size, 0,
                           reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa));
    return static_cast<i32>(result);
}

i32 UDPSocket::recv_from(Address& sender, void* buffer, u32 buffer_size) {
    if (socket_fd_ < 0) return -1;
    struct sockaddr_in sa{};
    socklen_t sa_len = sizeof(sa);
    auto result = ::recvfrom(socket_fd_, static_cast<char*>(buffer), buffer_size, 0,
                              reinterpret_cast<struct sockaddr*>(&sa), &sa_len);
    if (result >= 0) {
        sender = from_sockaddr(sa);
    }
    return static_cast<i32>(result);
}

bool UDPSocket::set_non_blocking(bool enabled) {
    if (socket_fd_ < 0) return false;
#ifdef _WIN32
    u_long mode = enabled ? 1 : 0;
    return ioctlsocket(socket_fd_, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0) return false;
    flags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(socket_fd_, F_SETFL, flags) == 0;
#endif
}

bool UDPSocket::set_send_buffer_size(u32 size) {
    if (socket_fd_ < 0) return false;
    int sz = static_cast<int>(size);
    return setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF,
                      reinterpret_cast<const char*>(&sz), sizeof(sz)) == 0;
}

bool UDPSocket::set_recv_buffer_size(u32 size) {
    if (socket_fd_ < 0) return false;
    int sz = static_cast<int>(size);
    return setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF,
                      reinterpret_cast<const char*>(&sz), sizeof(sz)) == 0;
}

u16 UDPSocket::local_port() const {
    if (socket_fd_ < 0) return 0;
    struct sockaddr_in sa{};
    socklen_t sa_len = sizeof(sa);
    if (getsockname(socket_fd_, reinterpret_cast<struct sockaddr*>(&sa), &sa_len) == 0) {
        return ntohs(sa.sin_port);
    }
    return 0;
}

// ── TCPSocket ───────────────────────────────────────────────────────────────

TCPSocket::TCPSocket() { init_winsock(); }
TCPSocket::TCPSocket(int fd) : socket_fd_(fd) {}
TCPSocket::~TCPSocket() { close(); }

TCPSocket::TCPSocket(TCPSocket&& other) noexcept : socket_fd_(other.socket_fd_) {
    other.socket_fd_ = -1;
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_fd_ = other.socket_fd_;
        other.socket_fd_ = -1;
    }
    return *this;
}

bool TCPSocket::open() {
    close();
    socket_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    return socket_fd_ >= 0;
}

bool TCPSocket::bind(const Address& local) {
    if (socket_fd_ < 0) return false;
    int opt = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
    auto sa = to_sockaddr(local);
    return ::bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == 0;
}

bool TCPSocket::listen(i32 backlog) {
    if (socket_fd_ < 0) return false;
    return ::listen(socket_fd_, backlog) == 0;
}

TCPSocket TCPSocket::accept(Address& remote) {
    struct sockaddr_in sa{};
    socklen_t sa_len = sizeof(sa);
    int new_fd = static_cast<int>(::accept(socket_fd_,
                                            reinterpret_cast<struct sockaddr*>(&sa), &sa_len));
    if (new_fd >= 0) {
        remote = from_sockaddr(sa);
    }
    return TCPSocket(new_fd);
}

bool TCPSocket::connect(const Address& remote) {
    if (socket_fd_ < 0) return false;
    auto sa = to_sockaddr(remote);
    return ::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == 0;
}

i32 TCPSocket::send(const void* data, u32 size) {
    if (socket_fd_ < 0) return -1;
    return static_cast<i32>(::send(socket_fd_, static_cast<const char*>(data), size, 0));
}

i32 TCPSocket::recv(void* buffer, u32 buffer_size) {
    if (socket_fd_ < 0) return -1;
    return static_cast<i32>(::recv(socket_fd_, static_cast<char*>(buffer), buffer_size, 0));
}

void TCPSocket::close() {
    if (socket_fd_ >= 0) {
        CLOSE_SOCKET(socket_fd_);
        socket_fd_ = -1;
    }
}

bool TCPSocket::set_non_blocking(bool enabled) {
    if (socket_fd_ < 0) return false;
#ifdef _WIN32
    u_long mode = enabled ? 1 : 0;
    return ioctlsocket(socket_fd_, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0) return false;
    flags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(socket_fd_, F_SETFL, flags) == 0;
#endif
}

bool TCPSocket::set_no_delay(bool enabled) {
    if (socket_fd_ < 0) return false;
    int flag = enabled ? 1 : 0;
    return setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}

} // namespace nexus::net
