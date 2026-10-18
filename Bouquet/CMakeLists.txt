# Bouquet: a granular instrument. Built twice from the same sources:
#   Bouquet     - instrument (MIDI in, plays the Sound Lab voices)
#   Bouquet FX  - audio effect with MIDI input (granulates the track it sits on)
#
# Both use the assets in assets/: the instrument recordings (samples/, tools/samples)
# and the rendered flowers (flowers/, tools/flowers). On a Mac the installer puts them
# once in /Library/Application Support/Bouquet; elsewhere they are copied into each
# bundle (or next to the app). Builds also find them in this source tree.

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
    Source/Assets.cpp
    Source/dsp/SoundLab.cpp
    Source/dsp/SampleBank.cpp
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
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    BOUQUET_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

function(bouquet_configure target)
    target_sources(${target} PRIVATE ${BOUQUET_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC ${BOUQUET_DEFINITIONS})
    target_link_libraries(${target}
        PRIVATE
            BouquetAssets
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

# Copies the samples and flower renders next to each built plugin (not on a Mac,
# where the installer shares one copy between all formats).
function(bouquet_ship_assets target)
    if(APPLE)
        return()
    endif()
    foreach(format VST3 Standalone)
        if(TARGET ${target}_${format})
            if(format STREQUAL "VST3")
                get_target_property(artefact ${target}_${format} JUCE_PLUGIN_ARTEFACT_FILE)
                set(dest "${artefact}/Contents/Resources")
            else()
                set(dest "$<TARGET_FILE_DIR:${target}_${format}>")
            endif()
            add_custom_command(TARGET ${target}_${format} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}/Flowers" "${dest}/Samples"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/flowers" "${dest}/Flowers"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/samples" "${dest}/Samples"
                VERBATIM)
        endif()
    endforeach()
endfunction()

juce_add_plugin(Bouquet
    COMPANY_NAME "Bouquet"
    BUNDLE_ID com.bouquet.instrument
    PLUGIN_MANUFACTURER_CODE Bqut
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
bouquet_ship_assets(Bouquet)

juce_add_plugin(BouquetFX
    COMPANY_NAME "Bouquet"
    BUNDLE_ID com.bouquet.fx
    PLUGIN_MANUFACTURER_CODE Bqut
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
bouquet_ship_assets(BouquetFX)

if(VST3TEST_BUILD_TESTS)
    juce_add_console_app(BouquetTest PRODUCT_NAME "BouquetTest")
    target_sources(BouquetTest PRIVATE tests/BouquetTest.cpp)
    target_compile_definitions(BouquetTest PRIVATE JucePlugin_Name="Bouquet" JucePlugin_IsSynth=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    bouquet_configure(BouquetTest)
    add_test(NAME bouquet_render COMMAND BouquetTest)
endif()
