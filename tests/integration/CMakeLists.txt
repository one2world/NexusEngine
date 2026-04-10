add_executable(nexus-test-integration
    test_full_pipeline.cpp
)

target_link_libraries(nexus-test-integration
    PRIVATE
        Nexus::Core
        Nexus::Scene
        Nexus::Scripting
        Nexus::Assets
        Nexus::Animation
        Nexus::Physics
        Nexus::Network
        Nexus::Audio
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-integration)

include(GoogleTest)
gtest_discover_tests(nexus-test-integration)
