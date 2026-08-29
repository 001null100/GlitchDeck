cmake_minimum_required(VERSION 3.24)
project(GlitchDeck VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

# Pin host plumbing to the exact null-clap revision under Bitwig validation for
# explicit MIDI-controlled audio-effect role metadata, raw MIDI process delivery,
# physical-pixel GUI sizing, process context, and state.
FetchContent_Declare(
    null_clap
    GIT_REPOSITORY https://github.com/001null100/null-clap.git
    GIT_TAG 04519c11d6cd0f311cad55be91e534366cbc9b8d
    GIT_SHALLOW FALSE
)
set(NULLCLAP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NULLCLAP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(null_clap)

# JUCE is a drawing/window toolkit only. GlitchDeck does not use JUCE's audio
# processor, parameter, MIDI, state, VST3, or CLAP wrapper layers.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

nullclap_add_plugin(GlitchDeck
    OUTPUT_NAME GlitchDeck
    SOURCES
        Source/Entry.cpp
        Source/GlitchDeckPlugin.cpp
        Source/GlitchDeckPlugin.hpp
        Source/ParameterIds.hpp
        Source/GlitchEngine.cpp
        Source/GlitchEngine.h
        Source/GlitchDeckEditor.cpp
        Source/GlitchDeckEditor.hpp
        Source/JuceGuiDelegate.cpp
        Source/JuceGuiDelegate.hpp
)

target_link_libraries(GlitchDeck PRIVATE
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

target_compile_definitions(GlitchDeck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if(MSVC)
    target_compile_options(GlitchDeck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_executable(GlitchEngineTests
        tests/GlitchEngineTests.cpp
        Source/GlitchEngine.cpp
        Source/GlitchEngine.h
    )
    target_include_directories(GlitchEngineTests PRIVATE Source)
    add_test(NAME GlitchEngineTests COMMAND GlitchEngineTests)
endif()
