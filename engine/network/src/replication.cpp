#include "nexus/net/replication.h"
#include <algorithm>
#include <cstring>

namespace nexus::net {

// ── ReplicatedProperty ──────────────────────────────────────────────────────

std::vector<u8> ReplicatedProperty::read_value(const u8* component_data) const {
    std::vector<u8> result(size);
    std::memcpy(result.data(), component_data + offset, size);
    return result;
}

void ReplicatedProperty::write_value(u8* component_data,
                                      const std::vector<u8>& value) const {
    u32 copy_size = std::min(size, static_cast<u32>(value.size()));
    std::memcpy(component_data + offset, value.data(), copy_size);
}

// ── ReplicatedComponent ─────────────────────────────────────────────────────

ReplicatedProperty* ReplicatedComponent::find(const std::string& name) {
    for (auto& p : properties) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

const ReplicatedProperty* ReplicatedComponent::find(const std::string& name) const {
    for (auto& p : properties) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

void ReplicatedComponent::add_property(const std::string& name, PropertyType type,
                                        u32 offset, u32 size) {
    ReplicatedProperty prop;
    prop.name = name;
    prop.type = type;
    prop.offset = offset;
    prop.size = size;
    properties.push_back(std::move(prop));
}

void ReplicatedComponent::clear_dirty() {
    for (auto& p : properties) {
        p.dirty = false;
    }
}

bool ReplicatedComponent::has_dirty() const {
    for (auto& p : properties) {
        if (p.dirty) return true;
    }
    return false;
}

// ── NetworkEntity ───────────────────────────────────────────────────────────

std::vector<u8> NetworkEntity::serialize_state(
        const std::vector<ReplicatedComponent>& schema) const {
    BitWriter writer;
    writer.write_u32(net_id.value);
    writer.write_u32(entity_id);
    writer.write_u32(owner_id);

    // Write component count
    u32 comp_count = static_cast<u32>(component_data.size());
    writer.write_u32(comp_count);

    for (auto& [comp_name, data] : component_data) {
        writer.write_string(comp_name);
        writer.write_bytes(data.data(), static_cast<u32>(data.size()));
    }

    writer.flush();
    return writer.data();
}

void NetworkEntity::deserialize_state(const std::vector<u8>& data,
                                        const std::vector<ReplicatedComponent>&) {
    BitReader reader(data);
    net_id = NetworkId(reader.read_u32());
    entity_id = reader.read_u32();
    owner_id = reader.read_u32();

    u32 comp_count = reader.read_u32();
    component_data.clear();

    for (u32 i = 0; i < comp_count && !reader.has_error(); i++) {
        std::string name = reader.read_string();
        u32 size = reader.read_u32();
        auto bytes = reader.read_bytes(size);
        component_data[name] = std::move(bytes);
    }
}

// ── ReplicationManager ──────────────────────────────────────────────────────

void ReplicationManager::register_component(const ReplicatedComponent& schema) {
    // Check if already registered
    for (auto& s : schemas_) {
        if (s.component_name == schema.component_name) {
            s = schema;
            return;
        }
    }
    schemas_.push_back(schema);
}

const ReplicatedComponent* ReplicationManager::find_schema(
        const std::string& name) const {
    for (auto& s : schemas_) {
        if (s.component_name == name) return &s;
    }
    return nullptr;
}

NetworkId ReplicationManager::register_entity(u32 entity_id, u32 owner_id) {
    // Check if already registered
    auto it = entity_to_net_.find(entity_id);
    if (it != entity_to_net_.end()) {
        return NetworkId(it->second);
    }

    NetworkId id = next_id();
    NetworkEntity ent;
    ent.net_id = id;
    ent.entity_id = entity_id;
    ent.owner_id = owner_id;

    entities_[id.value] = std::move(ent);
    entity_to_net_[entity_id] = id.value;
    return id;
}

void ReplicationManager::unregister_entity(NetworkId id) {
    auto it = entities_.find(id.value);
    if (it != entities_.end()) {
        entity_to_net_.erase(it->second.entity_id);
        entities_.erase(it);
    }
}

NetworkEntity* ReplicationManager::find_entity(NetworkId id) {
    auto it = entities_.find(id.value);
    return it != entities_.end() ? &it->second : nullptr;
}

const NetworkEntity* ReplicationManager::find_entity(NetworkId id) const {
    auto it = entities_.find(id.value);
    return it != entities_.end() ? &it->second : nullptr;
}

NetworkEntity* ReplicationManager::find_by_entity_id(u32 entity_id) {
    auto it = entity_to_net_.find(entity_id);
    if (it == entity_to_net_.end()) return nullptr;
    return find_entity(NetworkId(it->second));
}

void ReplicationManager::set_component_data(NetworkId id,
                                              const std::string& component,
                                              const std::vector<u8>& data) {
    auto* ent = find_entity(id);
    if (ent) {
        ent->component_data[component] = data;
    }
}

std::vector<u8> ReplicationManager::get_component_data(
        NetworkId id, const std::string& component) const {
    auto* ent = find_entity(id);
    if (!ent) return {};

    auto it = ent->component_data.find(component);
    if (it == ent->component_data.end()) return {};
    return it->second;
}

std::vector<u8> ReplicationManager::create_snapshot() const {
    BitWriter writer;
    u32 count = static_cast<u32>(entities_.size());
    writer.write_u32(count);

    for (auto& [_, ent] : entities_) {
        auto state = ent.serialize_state(schemas_);
        writer.write_bytes(state.data(), static_cast<u32>(state.size()));
    }

    writer.flush();
    return writer.data();
}

void ReplicationManager::apply_snapshot(const std::vector<u8>& snapshot) {
    BitReader reader(snapshot);
    u32 count = reader.read_u32();

    for (u32 i = 0; i < count && !reader.has_error(); i++) {
        u32 state_size = reader.read_u32();
        auto state_bytes = reader.read_bytes(state_size);

        if (state_bytes.size() < 12) continue; // Need at least net_id + entity_id + owner_id

        BitReader state_reader(state_bytes);
        u32 net_id_val = state_reader.read_u32();
        NetworkId net_id(net_id_val);

        auto* ent = find_entity(net_id);
        if (ent) {
            ent->deserialize_state(state_bytes, schemas_);
        } else {
            // Create new entity from snapshot
            NetworkEntity new_ent;
            new_ent.deserialize_state(state_bytes, schemas_);
            entities_[net_id.value] = std::move(new_ent);
            entity_to_net_[new_ent.entity_id] = net_id.value;
        }
    }
}

std::vector<NetworkId> ReplicationManager::all_entities() const {
    std::vector<NetworkId> result;
    result.reserve(entities_.size());
    for (auto& [_, ent] : entities_) {
        result.push_back(ent.net_id);
    }
    return result;
}

std::vector<u8> ReplicationManager::create_delta_snapshot(
        const std::vector<u8>& baseline) const {
    // Parse baseline to get previous entity states
    std::unordered_map<u32, std::vector<u8>> baseline_states;
    if (!baseline.empty()) {
        BitReader reader(baseline);
        u32 count = reader.read_u32();
        for (u32 i = 0; i < count && !reader.has_error(); ++i) {
            u32 state_size = reader.read_u32();
            auto state_bytes = reader.read_bytes(state_size);
            if (state_bytes.size() >= 4) {
                BitReader sr(state_bytes);
                u32 net_id_val = sr.read_u32();
                baseline_states[net_id_val] = std::move(state_bytes);
            }
        }
    }

    // Build delta: only include entities whose serialized state differs from baseline
    BitWriter writer;
    std::vector<std::pair<u32, std::vector<u8>>> changed;

    for (auto& [net_id_val, ent] : entities_) {
        auto state = ent.serialize_state(schemas_);
        auto it = baseline_states.find(net_id_val);
        if (it == baseline_states.end() || it->second != state) {
            changed.emplace_back(net_id_val, std::move(state));
        }
    }

    writer.write_u32(static_cast<u32>(changed.size()));
    for (auto& [id, state] : changed) {
        writer.write_bytes(state.data(), static_cast<u32>(state.size()));
    }

    writer.flush();
    return writer.data();
}

void ReplicationManager::apply_delta_snapshot(const std::vector<u8>& baseline,
                                               const std::vector<u8>& delta) {
    // First apply baseline to restore full state
    if (!baseline.empty()) {
        apply_snapshot(baseline);
    }

    // Then apply delta on top (overwriting changed entities)
    if (!delta.empty()) {
        apply_snapshot(delta);
    }
}

NetworkId ReplicationManager::next_id() {
    return NetworkId(next_net_id_++);
}

} // namespace nexus::net
