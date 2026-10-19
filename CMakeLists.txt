cmake_minimum_required(VERSION 3.22)
project(Pitchly VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime: the .vst3 has no dependency on the VC++ redistributable (handy on fresh ARM64 installs)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PITCHLY_BUILD_TESTS "Build the DSP engine unit tests" ON)
set(PITCHLY_JUCE_DIR "" CACHE PATH "Optional: path to an existing JUCE checkout (otherwise it is downloaded)")

if (PITCHLY_BUILD_TESTS)
    add_executable(pitchly_engine_test tests/engine_test.cpp)
endif()

if (PITCHLY_JUCE_DIR)
    add_subdirectory(${PITCHLY_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Pitchly
    COMPANY_NAME "Ethan"
    PLUGIN_MANUFACTURER_CODE Ethn
    PLUGIN_CODE Pchy
    FORMATS VST3
    PRODUCT_NAME "Pitchly"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Pitchly PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PitchEngine.h)

target_compile_definitions(Pitchly PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Pitchly
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Optional developer tool: renders the editor to a PNG without a display (cmake -DPITCHLY_BUILD_UI_SNAPSHOT=ON)
option(PITCHLY_BUILD_UI_SNAPSHOT "Build the headless UI snapshot tool" OFF)
if (PITCHLY_BUILD_UI_SNAPSHOT)
    juce_add_console_app(PitchlyUISnapshot PRODUCT_NAME "PitchlyUISnapshot")
    target_sources(PitchlyUISnapshot PRIVATE tests/ui_snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(PitchlyUISnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JucePlugin_Name="Pitchly" JucePlugin_Build_VST3=0)
    target_link_libraries(PitchlyUISnapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
