cmake_minimum_required(VERSION 3.22)

project(SDVerb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(SDVerb
    COMPANY_NAME "SD-Audio"
    PLUGIN_MANUFACTURER_CODE Sdau
    PLUGIN_CODE Sdvb
    FORMATS VST3 Standalone
    PRODUCT_NAME "SD-Verb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Reverb"
)

juce_add_binary_data(SDVerb_Assets
    SOURCES
        Resources/Fonts/EncodeSansExpanded-Black.ttf
        Resources/Fonts/ChakraPetch-SemiBold.ttf
)

target_sources(SDVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/FdnReverb.h
    Source/dsp/GateDucker.h
    Source/ui/RetroLookAndFeel.h
    Source/ui/VerbVisualizer.h
)

target_include_directories(SDVerb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(SDVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SDVerb
    PRIVATE
        SDVerb_Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
