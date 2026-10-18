# Bouquet: a granular instrument. Built twice from the same sources:
#   Bouquet     - instrument (MIDI in, plays the Sound Lab voices)
#   Bouquet FX  - audio effect with MIDI input (granulates the track it sits on)

juce_add_binary_data(BouquetAssets
    NAMESPACE BouquetAssets
    SOURCES
        assets/fonts/Newsreader-Light.ttf
        assets/fonts/Newsreader-LightItalic.ttf
        assets/fonts/Newsreader-Italic.ttf
        assets/fonts/Inter-Regular.ttf
        assets/fonts/Inter-Medium.ttf)

set(BOUQUET_SOURCES
    Source/PluginProcessor.cpp
    Source/Parameters.cpp
    Source/Spells.cpp
    Source/Presets.cpp
    Source/dsp/SoundLab.cpp
    Source/dsp/GranularEngine.cpp
    Source/ui/BouquetLookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/AuraPad.cpp
    Source/ui/Botany.cpp
    Source/ui/RealFlowers.cpp
    Source/ui/Garden.cpp
    Source/ui/Meadow.cpp
    Source/ui/PluginEditor.cpp)

set(BOUQUET_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

function(bouquet_configure target)
    target_sources(${target} PRIVATE ${BOUQUET_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC ${BOUQUET_DEFINITIONS})
    target_link_libraries(${target}
        PRIVATE
            BouquetAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(Bouquet
    COMPANY_NAME "VST3Test"
    BUNDLE_ID com.vst3test.bouquet
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE Bqt1
    PRODUCT_NAME "Bouquet"
    FORMATS AU VST3 Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    HARDENED_RUNTIME_ENABLED TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Bouquet can granulate live input from your microphone."
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
bouquet_configure(Bouquet)

juce_add_plugin(BouquetFX
    COMPANY_NAME "VST3Test"
    PLUGIN_MANUFACTURER_CODE Vst3
    BUNDLE_ID com.vst3test.bouquetfx
    PLUGIN_CODE Bqfx
    PRODUCT_NAME "Bouquet FX"
    FORMATS AU VST3
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    HARDENED_RUNTIME_ENABLED TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb)
bouquet_configure(BouquetFX)

# --- Bouquet Real: the same instrument with pre-rendered, photoreal flowers.
# The renders (tools/flowers) are copied into each bundle's Resources/Flowers.
function(bouquet_ship_flowers target)
    foreach(format VST3 AU Standalone)
        if(TARGET ${target}_${format})
            get_target_property(artefact ${target}_${format} JUCE_PLUGIN_ARTEFACT_FILE)
            if(APPLE OR NOT format STREQUAL "Standalone")
                set(dest "${artefact}/Contents/Resources/Flowers")
            else()
                get_filename_component(dir "${artefact}" DIRECTORY)
                set(dest "${dir}/Flowers")
            endif()
            add_custom_command(TARGET ${target}_${format} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/flowers" "${dest}"
                VERBATIM)
        endif()
    endforeach()
endfunction()

juce_add_plugin(BouquetReal
    COMPANY_NAME "VST3Test"
    BUNDLE_ID com.vst3test.bouquetreal
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE Bqr1
    PRODUCT_NAME "Bouquet Real"
    FORMATS AU VST3 Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    HARDENED_RUNTIME_ENABLED TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Bouquet can granulate live input from your microphone."
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
bouquet_configure(BouquetReal)
target_compile_definitions(BouquetReal PUBLIC BOUQUET_REAL_FLOWERS=1)
bouquet_ship_flowers(BouquetReal)

juce_add_plugin(BouquetRealFX
    COMPANY_NAME "VST3Test"
    BUNDLE_ID com.vst3test.bouquetrealfx
    PLUGIN_MANUFACTURER_CODE Vst3
    PLUGIN_CODE Bqrx
    PRODUCT_NAME "Bouquet Real FX"
    FORMATS AU VST3
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    HARDENED_RUNTIME_ENABLED TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb)
bouquet_configure(BouquetRealFX)
target_compile_definitions(BouquetRealFX PUBLIC BOUQUET_REAL_FLOWERS=1)
bouquet_ship_flowers(BouquetRealFX)

if(VST3TEST_BUILD_TESTS)
    juce_add_console_app(BouquetTest PRODUCT_NAME "BouquetTest")
    target_sources(BouquetTest PRIVATE tests/BouquetTest.cpp)
    target_compile_definitions(BouquetTest PRIVATE JucePlugin_Name="Bouquet" JucePlugin_IsSynth=1 JUCE_MODAL_LOOPS_PERMITTED=1
        BOUQUET_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/flowers")
    bouquet_configure(BouquetTest)
    add_test(NAME bouquet_render COMMAND BouquetTest)
endif()
