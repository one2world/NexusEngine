add_executable(nexus-test-renderer
    test_camera.cpp
    test_mesh_generators.cpp
    test_material.cpp
    test_surface_material.cpp
    test_advanced_rendering.cpp
    test_new_systems.cpp
    test_shadow_system.cpp
)

target_link_libraries(nexus-test-renderer
    PRIVATE
        Nexus::Core
        Nexus::RHI
        Nexus::Renderer
        Nexus::Scene
        gtest
        gtest_main
)

nexus_set_compiler_flags(nexus-test-renderer)

include(GoogleTest)
gtest_discover_tests(nexus-test-renderer)
