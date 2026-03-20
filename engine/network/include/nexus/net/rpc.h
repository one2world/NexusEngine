#pragma once

#include "nexus/core/types.h"
#include "nexus/net/serialization.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <deque>

namespace nexus::net {

// ─────────────────────────────────────────────────────────────────────────────
// RPC — Remote Procedure Call system
// ─────────────────────────────────────────────────────────────────────────────

enum class RPCTarget : u8 {
    Server,    // Called on server only
    Client,    // Called on specific client
    AllClients, // Called on all clients
    Owner      // Called on the owning client only
};

/// Serialized RPC arguments
struct RPCArgs {
    std::vector<u8> data;

    RPCArgs() = default;

    /// Construct with serialized data
    explicit RPCArgs(std::vector<u8> d) : data(std::move(d)) {}

    bool empty() const { return data.empty(); }
    u32 size() const { return static_cast<u32>(data.size()); }
};

/// An RPC invocation (pending or incoming)
struct RPCCall {
    u32 rpc_id{0};           // Hash of the function name
    u32 sender_id{0};        // Connection that sent this RPC
    u32 target_entity{0};    // Entity the RPC targets (0 = global)
    RPCTarget target{RPCTarget::Server};
    RPCArgs args;
};

/// RPC function handler
using RPCHandler = std::function<void(u32 sender_id, const RPCArgs& args)>;

/// Registered RPC definition
struct RPCDefinition {
    std::string name;
    u32 id{0};               // FNV hash of name
    RPCTarget target{RPCTarget::Server};
    RPCHandler handler;
    u32 min_args{0};
    u32 max_args{255};
};

// ─────────────────────────────────────────────────────────────────────────────
// RPCRegistry — registers and dispatches RPCs
// ─────────────────────────────────────────────────────────────────────────────

class RPCRegistry {
public:
    RPCRegistry() = default;

    /// Register an RPC function.
    void register_rpc(const std::string& name, RPCTarget target, RPCHandler handler);

    /// Find an RPC definition by name.
    const RPCDefinition* find(const std::string& name) const;

    /// Find an RPC definition by id.
    const RPCDefinition* find(u32 id) const;

    /// Call an RPC by name. Queues for sending or dispatches locally.
    void call(const std::string& name, u32 sender_id, const RPCArgs& args);

    /// Dispatch an incoming RPC call.
    bool dispatch(const RPCCall& call);

    /// Get pending outgoing RPC calls.
    std::vector<RPCCall> drain_outgoing();

    /// Push an incoming RPC (from network).
    void push_incoming(RPCCall call);

    /// Process all incoming RPCs.
    u32 process_incoming();

    /// Number of registered RPCs.
    u32 count() const { return static_cast<u32>(rpcs_.size()); }

    /// Hash a name to an RPC id.
    static u32 hash_name(const std::string& name);

private:
    std::vector<RPCDefinition> rpcs_;
    std::unordered_map<u32, u32> id_to_index_;  // rpc_id → index in rpcs_
    std::deque<RPCCall> incoming_;
    std::deque<RPCCall> outgoing_;
};

} // namespace nexus::net
