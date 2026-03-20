add_executable(nexus-test-perf
    test_profiler.cpp
    test_memory_tracker.cpp
    test_render_stats.cpp
    test_occlusion_culling.cpp
    test_lod_system.cpp
    test_instancing.cpp
    test_thread_pool.cpp
)

target_link_libraries(nexus-test-perf
    PRIVATE
        Nexus::Core
        Nexus::Perf
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-perf)

include(GoogleTest)
gtest_discover_tests(nexus-test-perf)
