cmake_minimum_required(VERSION 3.24)

project(ProQ3Clone VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE setup: pass a local source tree, or let CMake fetch it.
# ---------------------------------------------------------------------------
if(NOT JUCE_DIR)
    include(FetchContent)
    set(JUCE_VERSION "8.0.4" CACHE STRING "JUCE version fetched when JUCE_DIR is not set")
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${JUCE_VERSION}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(juce)
    set(JUCE_DIR ${juce_SOURCE_DIR})
endif()

add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/juce)

# ---------------------------------------------------------------------------
# Plugin target.
# ---------------------------------------------------------------------------
juce_add_plugin(ProQ3Clone
    PRODUCT_NAME             "Crunch EQ"
    COMPANY_NAME             "Kilo"
    BUNDLE_ID                "com.kilo.cruncheq"
    VERSION                  1.0.0
    PLUGIN_MANUFACTURER_CODE Kilo
    PLUGIN_CODE              CREQ
    FORMATS                  VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    EDITOR_NAME              "ProQ3CloneAudioProcessorEditor"
)

juce_generate_juce_header(ProQ3Clone)

# Embedded UI typefaces (Poppins, SIL OFL 1.1) so the plugin renders the same
# rounded font as the Crunch Compressor UI on any host/machine.
juce_add_binary_data(ProQ3CloneData SOURCES
    src/ui/Fonts/Poppins-Regular.ttf
    src/ui/Fonts/Poppins-Medium.ttf
    src/ui/Fonts/Poppins-SemiBold.ttf
    src/ui/Fonts/Poppins-Bold.ttf
)

target_sources(ProQ3Clone PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Parameters.cpp
    src/dsp/EQBand.cpp
    src/dsp/EqualizerDSP.cpp
    src/dsp/LinearPhaseFilter.cpp
    src/dsp/SpectrumAnalyzer.cpp
    src/dsp/SpectrumEngine.cpp
    src/ui/DisplayView.cpp
    src/ui/BandControlPanel.cpp
)

target_link_libraries(ProQ3Clone PRIVATE
    ProQ3CloneData
    juce::juce_audio_plugin_client
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

if(MSVC)
    target_compile_options(ProQ3Clone PRIVATE /utf-8 /permissive-)
else()
    target_compile_options(ProQ3Clone PRIVATE -Wall -Wextra)
endif()
