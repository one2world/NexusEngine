add_executable(nexus-test-animation
    test_tween.cpp
    test_sprite_animation.cpp
    test_skeleton.cpp
    test_particle_system.cpp
    test_animation_events.cpp
    test_ik_solver.cpp
    test_animator_system.cpp
)

target_link_libraries(nexus-test-animation
    PRIVATE Nexus::Animation GTest::gtest_main
)

nexus_set_compiler_flags(nexus-test-animation)

include(GoogleTest)
gtest_discover_tests(nexus-test-animation)
