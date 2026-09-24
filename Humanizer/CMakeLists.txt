cmake_minimum_required(VERSION 3.22)

project(Humanizer VERSION 0.1.0)

include(FetchContent)





add_subdirectory("C:/Users/neoqu/Desktop/Music/plugin/JUCE" JUCE)


juce_add_plugin(Humanizer
    COMPANY_NAME "myplugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Humn
    PLUGIN_CODE Huma
    FORMATS VST3 Standalone
    PRODUCT_NAME "Humanizer"
   VST3_CATEGORIES "MIDI Effect"
    
)
target_compile_definitions(Humanizer
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
)
juce_generate_juce_header(Humanizer)

target_sources(Humanizer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(Humanizer
    PRIVATE
        Source
)

target_link_libraries(Humanizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
)
