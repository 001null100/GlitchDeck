cmake_minimum_required(VERSION 3.24)
project(GlitchDeck VERSION 0.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)
option(GLITCHDECK_BUILD_PLUGIN "Build the JUCE editor and CLAP module" ON)

# Immutable shared-framework revision: backpressure-safe automation, bounded
# transactional state loading, numeric/event validation, and Release-active tests.
# GlitchDeck keeps its explicit MIDI/audio roles and existing DSP independently.
FetchContent_Declare(
    null_clap
    GIT_REPOSITORY https://github.com/001null100/null-clap.git
    GIT_TAG 9de3106360f324ee4b08906595c54a3e138f6cb7
    GIT_SHALLOW FALSE
)
set(NULLCLAP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# A consumer build should test the exact host plumbing it ships, not just the DSP.
set(NULLCLAP_BUILD_TESTS ${BUILD_TESTING} CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(null_clap)

if(GLITCHDECK_BUILD_PLUGIN)
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

endif()

if(BUILD_TESTING)
    add_executable(GlitchEngineTests
        tests/GlitchEngineTests.cpp
        Source/GlitchEngine.cpp
        Source/GlitchEngine.h
    )
    target_include_directories(GlitchEngineTests PRIVATE Source)
    add_test(NAME GlitchEngineTests COMMAND GlitchEngineTests)
    add_executable(GlitchDeckContractTests
        tests/GlitchDeckContractTests.cpp
        Source/GlitchDeckPlugin.cpp
        Source/GlitchEngine.cpp
    )
    target_include_directories(GlitchDeckContractTests PRIVATE Source)
    target_compile_definitions(GlitchDeckContractTests PRIVATE GLITCHDECK_HEADLESS_TEST=1)
    target_link_libraries(GlitchDeckContractTests PRIVATE nullclap::nullclap)
    if(MSVC)
        target_compile_options(GlitchDeckContractTests PRIVATE /utf-8)
    endif()
    add_test(NAME GlitchDeckContracts COMMAND GlitchDeckContractTests)
    if(GLITCHDECK_BUILD_PLUGIN)
        add_executable(GlitchDeckEditorTests
            tests/GlitchDeckEditorTests.cpp
            Source/GlitchDeckEditor.cpp
            Source/GlitchDeckPlugin.cpp
            Source/GlitchEngine.cpp
        )
        target_include_directories(GlitchDeckEditorTests PRIVATE Source)
        target_compile_definitions(GlitchDeckEditorTests PRIVATE
            GLITCHDECK_HEADLESS_TEST=1 JUCE_STANDALONE_APPLICATION=1
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        target_link_libraries(GlitchDeckEditorTests PRIVATE nullclap::nullclap juce::juce_gui_extra)
        if(MSVC)
            target_compile_options(GlitchDeckEditorTests PRIVATE /utf-8)
        endif()
        add_test(NAME GlitchDeckEditor COMMAND GlitchDeckEditorTests)
    endif()
endif()
