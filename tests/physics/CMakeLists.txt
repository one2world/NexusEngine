add_executable(nexus-test-physics
    test_physics_2d.cpp
    test_physics_3d.cpp
    test_physics_system.cpp
)

target_link_libraries(nexus-test-physics
    PRIVATE Nexus::Physics GTest::gtest_main
)

nexus_set_compiler_flags(nexus-test-physics)

include(GoogleTest)
gtest_discover_tests(nexus-test-physics)
