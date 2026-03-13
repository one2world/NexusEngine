add_executable(nexus-editor src/editor_main.cpp)

target_include_directories(nexus-editor
    PRIVATE include src
)

target_link_libraries(nexus-editor
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
        Nexus::Assets
        Nexus::Network
)

nexus_set_compiler_flags(nexus-editor)
nexus_set_build_defines(nexus-editor)
