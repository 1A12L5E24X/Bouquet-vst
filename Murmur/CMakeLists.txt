# Murmur: a granular instrument. Built twice from the same sources:
#   Murmur     - instrument (MIDI in, plays the Sound Lab voices)
#   Murmur FX  - audio effect with MIDI input (granulates the track it sits on)

juce_add_binary_data(MurmurAssets
    NAMESPACE MurmurAssets
    SOURCES
        assets/fonts/Newsreader-Light.ttf
        assets/fonts/Newsreader-LightItalic.ttf
        assets/fonts/Newsreader-Italic.ttf
        assets/fonts/Inter-Regular.ttf
        assets/fonts/Inter-Medium.ttf)

set(MURMUR_SOURCES
    Source/PluginProcessor.cpp
    Source/Parameters.cpp
    Source/Spells.cpp
    Source/dsp/SoundLab.cpp
    Source/dsp/GranularEngine.cpp
    Source/ui/MurmurLookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/AuraPad.cpp
    Source/ui/PluginEditor.cpp)

set(MURMUR_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

function(murmur_configure target)
    target_sources(${target} PRIVATE ${MURMUR_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC ${MURMUR_DEFINITIONS})
    target_link_libraries(${target}
        PRIVATE
            MurmurAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(Murmur
    COMPANY_NAME "VST3Test"
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE Mrmr
    PRODUCT_NAME "Murmur"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
murmur_configure(Murmur)

juce_add_plugin(MurmurFX
    COMPANY_NAME "VST3Test"
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE Mrfx
    PRODUCT_NAME "Murmur FX"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb)
murmur_configure(MurmurFX)

if(VST3TEST_BUILD_TESTS)
    juce_add_console_app(MurmurTest PRODUCT_NAME "MurmurTest")
    target_sources(MurmurTest PRIVATE tests/MurmurTest.cpp)
    target_compile_definitions(MurmurTest PRIVATE JucePlugin_Name="Murmur" JucePlugin_IsSynth=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    murmur_configure(MurmurTest)
    add_test(NAME murmur_render COMMAND MurmurTest)
endif()
