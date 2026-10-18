juce_add_plugin(SimpleSynth
    COMPANY_NAME "VST3Test"
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE SmSy
    PRODUCT_NAME "Simple Synth"
    FORMATS AU VST3 Standalone
    BUNDLE_ID com.vst3test.simplesynth
    HARDENED_RUNTIME_ENABLED TRUE
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
if(VST3TEST_BUILD_TESTS)
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
    add_test(NAME simplesynth_render COMMAND SimpleSynthTest)
endif()
