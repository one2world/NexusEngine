add_executable(nexus-test-audio
    test_audio.cpp
    test_reverb_zone.cpp
)

target_link_libraries(nexus-test-audio
    PRIVATE Nexus::Audio GTest::gtest_main
)

nexus_set_compiler_flags(nexus-test-audio)

include(GoogleTest)
gtest_discover_tests(nexus-test-audio)
