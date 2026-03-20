add_executable(nexus-test-network
    test_transport.cpp
    test_serialization.cpp
    test_replication.cpp
    test_rpc.cpp
    test_prediction.cpp
)

target_link_libraries(nexus-test-network
    PRIVATE
        Nexus::Core
        Nexus::Network
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-network)

include(GoogleTest)
gtest_discover_tests(nexus-test-network)
