#include <gtest/gtest.h>
#include "nexus/net/replication.h"

using namespace nexus;
using namespace nexus::net;

// =============================================================================
// NetworkId
// =============================================================================

TEST(NetworkId, Default) {
    NetworkId id;
    EXPECT_FALSE(id.valid());
    EXPECT_EQ(id.value, 0u);
}

TEST(NetworkId, Explicit) {
    NetworkId id(42);
    EXPECT_TRUE(id.valid());
    EXPECT_EQ(id.value, 42u);
}

TEST(NetworkId, Equality) {
    NetworkId a(1), b(1), c(2);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// =============================================================================
// ReplicatedProperty
// =============================================================================

TEST(ReplicatedProperty, ReadWriteValue) {
    ReplicatedProperty prop;
    prop.name = "health";
    prop.type = PropertyType::I32;
    prop.offset = 4;
    prop.size = 4;

    u8 data[16] = {};
    i32 value = 100;
    std::memcpy(data + 4, &value, sizeof(value));

    auto bytes = prop.read_value(data);
    EXPECT_EQ(bytes.size(), 4u);

    i32 read_back = 0;
    std::memcpy(&read_back, bytes.data(), sizeof(read_back));
    EXPECT_EQ(read_back, 100);

    // Write a different value
    i32 new_val = 50;
    std::vector<u8> new_bytes(4);
    std::memcpy(new_bytes.data(), &new_val, sizeof(new_val));
    prop.write_value(data, new_bytes);

    i32 result = 0;
    std::memcpy(&result, data + 4, sizeof(result));
    EXPECT_EQ(result, 50);
}

// =============================================================================
// ReplicatedComponent
// =============================================================================

TEST(ReplicatedComponent, AddAndFind) {
    ReplicatedComponent comp;
    comp.component_name = "Transform";
    comp.add_property("x", PropertyType::F32, 0, 4);
    comp.add_property("y", PropertyType::F32, 4, 4);
    comp.add_property("z", PropertyType::F32, 8, 4);

    EXPECT_EQ(comp.properties.size(), 3u);
    EXPECT_NE(comp.find("x"), nullptr);
    EXPECT_NE(comp.find("y"), nullptr);
    EXPECT_NE(comp.find("z"), nullptr);
    EXPECT_EQ(comp.find("w"), nullptr);
}

TEST(ReplicatedComponent, DirtyTracking) {
    ReplicatedComponent comp;
    comp.component_name = "Health";
    comp.add_property("hp", PropertyType::I32, 0, 4);
    comp.add_property("max_hp", PropertyType::I32, 4, 4);

    EXPECT_FALSE(comp.has_dirty());

    comp.find("hp")->dirty = true;
    EXPECT_TRUE(comp.has_dirty());

    comp.clear_dirty();
    EXPECT_FALSE(comp.has_dirty());
}

// =============================================================================
// ReplicationManager
// =============================================================================

TEST(ReplicationManager, RegisterComponent) {
    ReplicationManager mgr;

    ReplicatedComponent comp;
    comp.component_name = "Position";
    comp.add_property("x", PropertyType::F32, 0, 4);
    comp.add_property("y", PropertyType::F32, 4, 4);

    mgr.register_component(comp);
    EXPECT_EQ(mgr.schema_count(), 1u);

    auto* schema = mgr.find_schema("Position");
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->properties.size(), 2u);
}

TEST(ReplicationManager, RegisterEntity) {
    ReplicationManager mgr;

    auto id1 = mgr.register_entity(100, 0);
    EXPECT_TRUE(id1.valid());
    EXPECT_EQ(mgr.entity_count(), 1u);

    auto id2 = mgr.register_entity(200, 1);
    EXPECT_NE(id1, id2);
    EXPECT_EQ(mgr.entity_count(), 2u);
}

TEST(ReplicationManager, RegisterDuplicate) {
    ReplicationManager mgr;
    auto id1 = mgr.register_entity(100);
    auto id2 = mgr.register_entity(100); // Same entity
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(mgr.entity_count(), 1u);
}

TEST(ReplicationManager, UnregisterEntity) {
    ReplicationManager mgr;
    auto id = mgr.register_entity(100);
    EXPECT_EQ(mgr.entity_count(), 1u);

    mgr.unregister_entity(id);
    EXPECT_EQ(mgr.entity_count(), 0u);
    EXPECT_EQ(mgr.find_entity(id), nullptr);
}

TEST(ReplicationManager, FindEntity) {
    ReplicationManager mgr;
    auto id = mgr.register_entity(42, 1);

    auto* ent = mgr.find_entity(id);
    ASSERT_NE(ent, nullptr);
    EXPECT_EQ(ent->entity_id, 42u);
    EXPECT_EQ(ent->owner_id, 1u);
}

TEST(ReplicationManager, FindByEntityId) {
    ReplicationManager mgr;
    mgr.register_entity(42);

    auto* ent = mgr.find_by_entity_id(42);
    ASSERT_NE(ent, nullptr);
    EXPECT_EQ(ent->entity_id, 42u);

    EXPECT_EQ(mgr.find_by_entity_id(999), nullptr);
}

TEST(ReplicationManager, ComponentData) {
    ReplicationManager mgr;
    auto id = mgr.register_entity(1);

    std::vector<u8> data = {1, 2, 3, 4};
    mgr.set_component_data(id, "Position", data);

    auto result = mgr.get_component_data(id, "Position");
    EXPECT_EQ(result, data);

    auto empty = mgr.get_component_data(id, "Nonexistent");
    EXPECT_TRUE(empty.empty());
}

TEST(ReplicationManager, AllEntities) {
    ReplicationManager mgr;
    mgr.register_entity(1);
    mgr.register_entity(2);
    mgr.register_entity(3);

    auto all = mgr.all_entities();
    EXPECT_EQ(all.size(), 3u);
}

TEST(ReplicationManager, Snapshot) {
    ReplicationManager mgr;
    auto id = mgr.register_entity(42, 0);
    mgr.set_component_data(id, "Health", {100, 0, 0, 0});

    auto snapshot = mgr.create_snapshot();
    EXPECT_FALSE(snapshot.empty());
}

TEST(ReplicationManager, UpdateDuplicate) {
    ReplicationManager mgr;

    ReplicatedComponent comp;
    comp.component_name = "Pos";
    comp.add_property("x", PropertyType::F32, 0, 4);
    mgr.register_component(comp);

    comp.add_property("y", PropertyType::F32, 4, 4);
    mgr.register_component(comp); // Update
    EXPECT_EQ(mgr.schema_count(), 1u);

    auto* schema = mgr.find_schema("Pos");
    EXPECT_EQ(schema->properties.size(), 2u);
}

// =============================================================================
// NetworkEntity serialization
// =============================================================================

TEST(NetworkEntity, SerializeDeserialize) {
    NetworkEntity ent;
    ent.net_id = NetworkId(5);
    ent.entity_id = 100;
    ent.owner_id = 2;
    ent.component_data["Health"] = {100, 0, 0, 0};
    ent.component_data["Position"] = {0, 0, 0, 0, 0, 0, 128, 63}; // 1.0f

    std::vector<ReplicatedComponent> schemas;
    auto serialized = ent.serialize_state(schemas);
    EXPECT_FALSE(serialized.empty());

    NetworkEntity ent2;
    ent2.deserialize_state(serialized, schemas);
    EXPECT_EQ(ent2.net_id, ent.net_id);
    EXPECT_EQ(ent2.entity_id, ent.entity_id);
    EXPECT_EQ(ent2.owner_id, ent.owner_id);
    EXPECT_EQ(ent2.component_data.size(), 2u);
}
