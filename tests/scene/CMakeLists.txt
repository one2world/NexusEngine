add_executable(nexus-test-scene test_ecs.cpp)

target_link_libraries(nexus-test-scene
    PRIVATE
        Nexus::Core
        Nexus::Scene
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-scene)

include(GoogleTest)
gtest_discover_tests(nexus-test-scene)
