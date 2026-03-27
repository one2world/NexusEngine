add_executable(nexus-runtime src/runtime_main.cpp)

target_include_directories(nexus-runtime
    PRIVATE src
)

target_link_libraries(nexus-runtime
    PRIVATE
        Nexus::Core
        Nexus::Platform
        Nexus::RHI
        Nexus::Renderer
        Nexus::Scene
        Nexus::Physics
        Nexus::Audio
        Nexus::Animation
        Nexus::UI
        Nexus::Scripting
        Nexus::Perf
        Nexus::Assets
        Nexus::Network
)

nexus_set_compiler_flags(nexus-runtime)
nexus_set_build_defines(nexus-runtime)
