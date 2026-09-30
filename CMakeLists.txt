cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(PedalCues VERSION 0.4.10 LANGUAGES C CXX)

option(PEDALCUES_COPY_PLUGIN "Install plugins into the system plug-in folders after building" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if present (external/JUCE), otherwise download it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PedalCues
    COMPANY_NAME "Thanasis Kostopoulos"
    COMPANY_WEBSITE "https://github.com/thankost/pedal-cues"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Thanasis Kostopoulos. MIT License."
    BUNDLE_ID "com.athkost.pedalcues"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    PLUGIN_MANUFACTURER_CODE Athk
    PLUGIN_CODE Pdcu
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "PedalCues"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${PEDALCUES_COPY_PLUGIN})

target_sources(PedalCues PRIVATE
    Source/CueModel.cpp
    Source/State.cpp
    Source/Theme.cpp
    Source/Tile.cpp
    Source/EditorCommon.cpp
    Source/QcPage.cpp
    Source/WhammyPage.cpp
    Source/SettingsPage.cpp
    Source/Tour.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PedalCues PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PedalCues
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(PEDALCUES_BUILD_TESTS "Build the CueModel tests" ON)
if(PEDALCUES_BUILD_TESTS)
    juce_add_console_app(PedalCuesTests PRODUCT_NAME "PedalCuesTests")
    target_sources(PedalCuesTests PRIVATE Tests/CueModelTests.cpp Source/CueModel.cpp)
    target_compile_definitions(PedalCuesTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(PedalCuesTests PRIVATE juce::juce_audio_basics juce::juce_graphics
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# Renders the screenshots used in docs/ (cmake -DPEDALCUES_BUILD_DOCS=ON, then run DocShots <outputDir>).
option(PEDALCUES_BUILD_DOCS "Build the documentation screenshot tool" OFF)
if(PEDALCUES_BUILD_DOCS)
    juce_add_console_app(DocShots PRODUCT_NAME "DocShots")
    target_sources(DocShots PRIVATE
        Tools/DocShots.cpp
        Source/CueModel.cpp
        Source/State.cpp
        Source/Theme.cpp
        Source/Tile.cpp
        Source/EditorCommon.cpp
        Source/QcPage.cpp
        Source/WhammyPage.cpp
        Source/SettingsPage.cpp
        Source/Tour.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(DocShots PRIVATE
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
        JucePlugin_Name="PedalCues"
        JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(DocShots PRIVATE juce::juce_audio_utils
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
