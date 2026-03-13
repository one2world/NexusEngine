add_library(nexus-core STATIC
    src/log.cpp
    src/timer.cpp
    src/event_bus.cpp
    src/vfs.cpp
)

target_include_directories(nexus-core
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-core
    PUBLIC  spdlog::spdlog glm::glm nlohmann_json::nlohmann_json
)

nexus_set_compiler_flags(nexus-core)
nexus_set_build_defines(nexus-core)

add_library(Nexus::Core ALIAS nexus-core)
