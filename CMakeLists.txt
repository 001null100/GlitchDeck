cmake_minimum_required(VERSION 3.24)
project(GlitchDeck VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GLITCHDECK_JUCE_PATH "" CACHE PATH "Path to a JUCE checkout")

if(GLITCHDECK_JUCE_PATH)
    add_subdirectory("${GLITCHDECK_JUCE_PATH}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(GlitchDeck
    COMPANY_NAME "Null Exo"
    BUNDLE_ID "dev.nullexo.glitchdeck"
    PLUGIN_MANUFACTURER_CODE NuEx
    PLUGIN_CODE GlDc
    FORMATS VST3
    PRODUCT_NAME "GlitchDeck"
    DESCRIPTION "Playable performance-first glitch processor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(GlitchDeck)

target_sources(GlitchDeck PRIVATE
    Source/GlitchEngine.cpp
    Source/GlitchEngine.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(GlitchDeck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

if(MSVC)
    target_compile_options(GlitchDeck PRIVATE /utf-8)
endif()

target_link_libraries(GlitchDeck PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
