#pragma once

#include "nexus/core/types.h"
#include "nexus/net/serialization.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <any>

namespace nexus::net {

// ─────────────────────────────────────────────────────────────────────────────
// NetworkId — unique identifier for a networked entity
// ─────────────────────────────────────────────────────────────────────────────

struct NetworkId {
    u32 value{0};

    NetworkId() = default;
    explicit NetworkId(u32 v) : value(v) {}

    bool valid() const { return value != 0; }
    bool operator==(const NetworkId& o) const { return value == o.value; }
    bool operator!=(const NetworkId& o) const { return value != o.value; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ReplicatedProperty — a single property that is synced over the network
// ─────────────────────────────────────────────────────────────────────────────

enum class PropertyType : u8 {
    Bool, I32, U32, F32, String, Vec3, Custom
};

struct ReplicatedProperty {
    std::string name;
    PropertyType type{PropertyType::Custom};
    u32 offset{0};            // Byte offset within component data
    u32 size{0};              // Size in bytes
    bool dirty{false};        // Changed since last sync

    /// Serialize this property's value from a data block.
    std::vector<u8> read_value(const u8* component_data) const;

    /// Deserialize this property's value into a data block.
    void write_value(u8* component_data, const std::vector<u8>& value) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// ReplicatedComponent — defines which properties of a component are synced
// ─────────────────────────────────────────────────────────────────────────────

struct ReplicatedComponent {
    std::string component_name;
    std::vector<ReplicatedProperty> properties;

    /// Find a property by name.
    ReplicatedProperty* find(const std::string& name);
    const ReplicatedProperty* find(const std::string& name) const;

    /// Add a property.
    void add_property(const std::string& name, PropertyType type,
                      u32 offset, u32 size);

    /// Mark all properties as clean.
    void clear_dirty();

    /// Check if any property is dirty.
    bool has_dirty() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// NetworkEntity — a replicated entity with its component data
// ─────────────────────────────────────────────────────────────────────────────

struct NetworkEntity {
    NetworkId net_id;
    u32 entity_id{0};      // Local ECS entity id
    u32 owner_id{0};       // Connection id of the owner (0 = server)
    bool is_local{false};  // Do we have authority?

    std::unordered_map<std::string, std::vector<u8>> component_data;

    /// Serialize all dirty properties into a packet payload.
    std::vector<u8> serialize_state(const std::vector<ReplicatedComponent>& schema) const;

    /// Deserialize state from a packet payload.
    void deserialize_state(const std::vector<u8>& data,
                            const std::vector<ReplicatedComponent>& schema);
};

// ─────────────────────────────────────────────────────────────────────────────
// ReplicationManager — manages entity replication
// ─────────────────────────────────────────────────────────────────────────────

class ReplicationManager {
public:
    ReplicationManager() = default;

    /// Register a component schema for replication.
    void register_component(const ReplicatedComponent& schema);

    /// Find a registered component schema.
    const ReplicatedComponent* find_schema(const std::string& name) const;

    /// Register a new networked entity.
    NetworkId register_entity(u32 entity_id, u32 owner_id = 0);

    /// Unregister a networked entity.
    void unregister_entity(NetworkId id);

    /// Find a network entity by its NetworkId.
    NetworkEntity* find_entity(NetworkId id);
    const NetworkEntity* find_entity(NetworkId id) const;

    /// Find a network entity by local entity id.
    NetworkEntity* find_by_entity_id(u32 entity_id);

    /// Set component data for an entity.
    void set_component_data(NetworkId id, const std::string& component,
                             const std::vector<u8>& data);

    /// Get component data for an entity.
    std::vector<u8> get_component_data(NetworkId id,
                                        const std::string& component) const;

    /// Generate replication snapshot for all dirty entities.
    std::vector<u8> create_snapshot() const;

    /// Apply a received replication snapshot.
    void apply_snapshot(const std::vector<u8>& snapshot);

    /// Number of registered entities.
    u32 entity_count() const { return static_cast<u32>(entities_.size()); }

    /// Number of registered component schemas.
    u32 schema_count() const { return static_cast<u32>(schemas_.size()); }

    /// All network entity ids.
    std::vector<NetworkId> all_entities() const;

    /// Create a delta-compressed snapshot relative to a baseline.
    /// Only includes entities/components that changed since baseline.
    std::vector<u8> create_delta_snapshot(const std::vector<u8>& baseline) const;

    /// Apply a delta snapshot on top of a baseline to produce full state.
    void apply_delta_snapshot(const std::vector<u8>& baseline,
                              const std::vector<u8>& delta);

private:
    NetworkId next_id();

    u32 next_net_id_{1};
    std::unordered_map<u32, NetworkEntity> entities_; // net_id.value → entity
    std::unordered_map<u32, u32> entity_to_net_;      // entity_id → net_id.value
    std::vector<ReplicatedComponent> schemas_;
};

} // namespace nexus::net

template <>
struct std::hash<nexus::net::NetworkId> {
    std::size_t operator()(const nexus::net::NetworkId& id) const noexcept {
        return std::hash<nexus::u32>{}(id.value);
    }
};
