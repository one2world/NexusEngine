# Defer gtest test enumeration to ctest run-time. POST_BUILD mode invokes the
# test binary with --gtest_list_tests during the build, which deadlocks with
# sanitizer-instrumented binaries on some macOS toolchains and causes the
# linker to delete the just-built executable on timeout.
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

add_subdirectory(core)
add_subdirectory(scene)
add_subdirectory(rhi)
add_subdirectory(renderer)
add_subdirectory(physics)
add_subdirectory(audio)
add_subdirectory(animation)
add_subdirectory(ui)
add_subdirectory(scripting)
add_subdirectory(assets)
add_subdirectory(network)
add_subdirectory(editor)
add_subdirectory(perf)
add_subdirectory(platform)
add_subdirectory(integration)
