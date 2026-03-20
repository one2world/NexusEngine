#include "nexus/net/rpc.h"
#include "nexus/core/log.h"

namespace nexus::net {

// ── RPCRegistry ─────────────────────────────────────────────────────────────

u32 RPCRegistry::hash_name(const std::string& name) {
    // FNV-1a 32-bit
    u32 hash = 2166136261u;
    for (char c : name) {
        hash ^= static_cast<u32>(static_cast<unsigned char>(c));
        hash *= 16777619u;
    }
    return hash;
}

void RPCRegistry::register_rpc(const std::string& name, RPCTarget target,
                                 RPCHandler handler) {
    RPCDefinition def;
    def.name = name;
    def.id = hash_name(name);
    def.target = target;
    def.handler = std::move(handler);

    u32 index = static_cast<u32>(rpcs_.size());
    rpcs_.push_back(std::move(def));
    id_to_index_[def.id] = index;
}

const RPCDefinition* RPCRegistry::find(const std::string& name) const {
    u32 id = hash_name(name);
    return find(id);
}

const RPCDefinition* RPCRegistry::find(u32 id) const {
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) return nullptr;
    return &rpcs_[it->second];
}

void RPCRegistry::call(const std::string& name, u32 sender_id,
                        const RPCArgs& args) {
    u32 id = hash_name(name);
    auto* def = find(id);
    if (!def) {
        NX_ERROR("RPC not found: {}", name);
        return;
    }

    RPCCall rpc_call;
    rpc_call.rpc_id = id;
    rpc_call.sender_id = sender_id;
    rpc_call.target = def->target;
    rpc_call.args = args;

    outgoing_.push_back(std::move(rpc_call));
}

bool RPCRegistry::dispatch(const RPCCall& call) {
    auto* def = find(call.rpc_id);
    if (!def) {
        NX_ERROR("RPC dispatch: unknown id {}", call.rpc_id);
        return false;
    }

    if (def->handler) {
        def->handler(call.sender_id, call.args);
        return true;
    }

    return false;
}

std::vector<RPCCall> RPCRegistry::drain_outgoing() {
    std::vector<RPCCall> result;
    while (!outgoing_.empty()) {
        result.push_back(std::move(outgoing_.front()));
        outgoing_.pop_front();
    }
    return result;
}

void RPCRegistry::push_incoming(RPCCall call) {
    incoming_.push_back(std::move(call));
}

u32 RPCRegistry::process_incoming() {
    u32 processed = 0;
    while (!incoming_.empty()) {
        auto call = std::move(incoming_.front());
        incoming_.pop_front();
        if (dispatch(call)) {
            processed++;
        }
    }
    return processed;
}

} // namespace nexus::net
