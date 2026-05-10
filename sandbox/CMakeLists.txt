add_executable(nexus-sandbox src/sandbox_main.cpp)

target_include_directories(nexus-sandbox
    PRIVATE src
)

target_link_libraries(nexus-sandbox
    PRIVATE
        Nexus::Core
        Nexus::Platform
        Nexus::RHI
        Nexus::Renderer
        Nexus::Scene
        Nexus::Scripting
)

nexus_set_compiler_flags(nexus-sandbox)
nexus_set_build_defines(nexus-sandbox)
