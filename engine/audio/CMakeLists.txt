add_library(nexus-audio STATIC
    src/audio_buffer.cpp
    src/audio_engine.cpp
    src/audio_device.cpp
    src/audio_streaming.cpp
    src/reverb_zone.cpp
)

target_include_directories(nexus-audio
    PUBLIC  include
    PRIVATE src
)

# miniaudio is a single-header library — expose its include directory
target_include_directories(nexus-audio
    PRIVATE ${miniaudio_SOURCE_DIR}
)

# stb_vorbis for OGG Vorbis decoding
target_include_directories(nexus-audio
    PRIVATE ${stb_SOURCE_DIR}
)

target_link_libraries(nexus-audio
    PUBLIC Nexus::Core
)

# Platform audio libraries required by miniaudio
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(nexus-audio PRIVATE Threads::Threads dl m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # miniaudio uses WASAPI/DirectSound/WinMM — all via system headers
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    target_link_libraries(nexus-audio PRIVATE
        ${COREAUDIO_FRAMEWORK} ${AUDIOUNIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

nexus_set_compiler_flags(nexus-audio)
nexus_set_build_defines(nexus-audio)

add_library(Nexus::Audio ALIAS nexus-audio)
