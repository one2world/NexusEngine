#include <gtest/gtest.h>
#include "nexus/net/rpc.h"

using namespace nexus;
using namespace nexus::net;

// =============================================================================
// RPCRegistry
// =============================================================================

TEST(RPCRegistry, RegisterAndFind) {
    RPCRegistry reg;
    reg.register_rpc("SpawnPlayer", RPCTarget::Server,
        [](u32, const RPCArgs&) {});

    EXPECT_EQ(reg.count(), 1u);
    EXPECT_NE(reg.find("SpawnPlayer"), nullptr);
    EXPECT_EQ(reg.find("NonExistent"), nullptr);
}

TEST(RPCRegistry, FindById) {
    RPCRegistry reg;
    reg.register_rpc("Shoot", RPCTarget::Server,
        [](u32, const RPCArgs&) {});

    u32 id = RPCRegistry::hash_name("Shoot");
    EXPECT_NE(reg.find(id), nullptr);
    EXPECT_EQ(reg.find(0u), nullptr);
}

TEST(RPCRegistry, HashDeterministic) {
    u32 h1 = RPCRegistry::hash_name("TestRPC");
    u32 h2 = RPCRegistry::hash_name("TestRPC");
    EXPECT_EQ(h1, h2);
}

TEST(RPCRegistry, HashDifferentNames) {
    u32 h1 = RPCRegistry::hash_name("Alpha");
    u32 h2 = RPCRegistry::hash_name("Beta");
    EXPECT_NE(h1, h2);
}

TEST(RPCRegistry, CallQueuesOutgoing) {
    RPCRegistry reg;
    reg.register_rpc("Ping", RPCTarget::Server,
        [](u32, const RPCArgs&) {});

    RPCArgs args;
    args.data = {1, 2, 3};
    reg.call("Ping", 1, args);

    auto outgoing = reg.drain_outgoing();
    EXPECT_EQ(outgoing.size(), 1u);
    EXPECT_EQ(outgoing[0].sender_id, 1u);
    EXPECT_EQ(outgoing[0].args.data.size(), 3u);
}

TEST(RPCRegistry, DrainClearsOutgoing) {
    RPCRegistry reg;
    reg.register_rpc("Test", RPCTarget::Server,
        [](u32, const RPCArgs&) {});

    reg.call("Test", 0, RPCArgs{});
    reg.drain_outgoing();

    auto second = reg.drain_outgoing();
    EXPECT_TRUE(second.empty());
}

TEST(RPCRegistry, Dispatch) {
    RPCRegistry reg;
    u32 received_sender = 0;
    bool called = false;

    reg.register_rpc("Greet", RPCTarget::AllClients,
        [&](u32 sender, const RPCArgs&) {
            called = true;
            received_sender = sender;
        });

    RPCCall call;
    call.rpc_id = RPCRegistry::hash_name("Greet");
    call.sender_id = 42;

    EXPECT_TRUE(reg.dispatch(call));
    EXPECT_TRUE(called);
    EXPECT_EQ(received_sender, 42u);
}

TEST(RPCRegistry, DispatchUnknown) {
    RPCRegistry reg;
    RPCCall call;
    call.rpc_id = 99999;
    EXPECT_FALSE(reg.dispatch(call));
}

TEST(RPCRegistry, IncomingProcessing) {
    RPCRegistry reg;
    int call_count = 0;

    reg.register_rpc("Tick", RPCTarget::Server,
        [&](u32, const RPCArgs&) { call_count++; });

    RPCCall c1, c2;
    c1.rpc_id = RPCRegistry::hash_name("Tick");
    c2.rpc_id = RPCRegistry::hash_name("Tick");

    reg.push_incoming(c1);
    reg.push_incoming(c2);

    u32 processed = reg.process_incoming();
    EXPECT_EQ(processed, 2u);
    EXPECT_EQ(call_count, 2);
}

TEST(RPCRegistry, RPCTargetStored) {
    RPCRegistry reg;
    reg.register_rpc("ServerOnly", RPCTarget::Server,
        [](u32, const RPCArgs&) {});
    reg.register_rpc("ClientOnly", RPCTarget::Client,
        [](u32, const RPCArgs&) {});

    auto* s = reg.find("ServerOnly");
    auto* c = reg.find("ClientOnly");
    ASSERT_NE(s, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(s->target, RPCTarget::Server);
    EXPECT_EQ(c->target, RPCTarget::Client);
}

TEST(RPCRegistry, RPCArgsEmpty) {
    RPCArgs args;
    EXPECT_TRUE(args.empty());
    EXPECT_EQ(args.size(), 0u);
}

TEST(RPCRegistry, RPCArgsWithData) {
    RPCArgs args(std::vector<u8>{1, 2, 3});
    EXPECT_FALSE(args.empty());
    EXPECT_EQ(args.size(), 3u);
}

TEST(RPCRegistry, MultipleRegistrations) {
    RPCRegistry reg;
    reg.register_rpc("A", RPCTarget::Server, [](u32, const RPCArgs&) {});
    reg.register_rpc("B", RPCTarget::Client, [](u32, const RPCArgs&) {});
    reg.register_rpc("C", RPCTarget::AllClients, [](u32, const RPCArgs&) {});
    EXPECT_EQ(reg.count(), 3u);
}
