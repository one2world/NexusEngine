add_library(nexus-audio STATIC
    src/audio_buffer.cpp
    src/audio_engine.cpp
    src/audio_streaming.cpp
)

target_include_directories(nexus-audio
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(nexus-audio
    PUBLIC Nexus::Core
)

nexus_set_compiler_flags(nexus-audio)
nexus_set_build_defines(nexus-audio)

add_library(Nexus::Audio ALIAS nexus-audio)
