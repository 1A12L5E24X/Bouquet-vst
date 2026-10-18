cmake_minimum_required(VERSION 3.22)
project(SimpleSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SimpleSynth
    COMPANY_NAME "VST3Test"
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE SmSy
    PRODUCT_NAME "Simple Synth"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(SimpleSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthVoice.cpp)

target_compile_definitions(SimpleSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SimpleSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Headless smoke test: renders notes through the processor and checks the output.
option(SIMPLESYNTH_BUILD_TESTS "Build the offline render test" ON)
if(SIMPLESYNTH_BUILD_TESTS)
    juce_add_console_app(SimpleSynthTest PRODUCT_NAME "SimpleSynthTest")
    # Compile the plugin sources directly so the test doesn't depend on a plugin wrapper.
    target_sources(SimpleSynthTest PRIVATE
        tests/RenderTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SynthVoice.cpp)
    target_compile_definitions(SimpleSynthTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Simple Synth")
    target_link_libraries(SimpleSynthTest PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    enable_testing()
    add_test(NAME render_test COMMAND SimpleSynthTest)
endif()
