add_library(nexus-network STATIC
    src/transport.cpp
    src/udp_transport.cpp
    src/serialization.cpp
    src/replication.cpp
    src/rpc.cpp
    src/prediction.cpp
)

target_include_directories(nexus-network
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-network
    PUBLIC Nexus::Core
)

nexus_set_compiler_flags(nexus-network)
nexus_set_build_defines(nexus-network)

add_library(Nexus::Network ALIAS nexus-network)
