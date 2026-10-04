cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(PedalCues VERSION 0.8.2 LANGUAGES C CXX)

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

# hidapi (BSD licence option): read-only USB access to the Quad Cortex for "Sync from QC".
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG hidapi-0.14.0
    GIT_SHALLOW TRUE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)   # hidapi 0.14 declares an older minimum than current CMake accepts
FetchContent_MakeAvailable(hidapi)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# Plugin formats per platform: AU on macOS, LV2 on Linux; VST3 and the standalone app everywhere.
if(APPLE)
    set(PEDALCUES_FORMATS VST3 AU Standalone)
elseif(UNIX)
    set(PEDALCUES_FORMATS VST3 LV2 Standalone)
else()
    set(PEDALCUES_FORMATS VST3 Standalone)
endif()

juce_add_plugin(PedalCues
    COMPANY_NAME "Thanasis Kostopoulos"
    COMPANY_WEBSITE "https://github.com/thankost/pedal-cues"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Thanasis Kostopoulos. MIT License."
    BUNDLE_ID "com.athkost.pedalcues"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    PLUGIN_MANUFACTURER_CODE Athk
    PLUGIN_CODE Pdcu
    FORMATS ${PEDALCUES_FORMATS}
    LV2URI "https://github.com/thankost/pedal-cues"
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
    Source/DeviceTemplates.cpp
    Source/Modellers.cpp
    Source/Theme.cpp
    Source/Tile.cpp
    Source/EditorCommon.cpp
    Source/QcPage.cpp
    Source/WhammyPage.cpp
    Source/MovesPanel.cpp
    Source/QcExpression.cpp
    Source/KemperPage.cpp
    Source/CustomPage.cpp
    Source/ModellerPage.cpp
    Source/UnitPicker.cpp
    Source/SettingsPage.cpp
    Source/Tour.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Update.cpp
    Source/QcUsb.cpp
    Source/QcSyncDialog.cpp
    Source/StandaloneApp.cpp)

target_compile_definitions(PedalCues PUBLIC
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PedalCues
    PRIVATE
        juce::juce_audio_utils
        hidapi::hidapi
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(PEDALCUES_BUILD_TESTS "Build the CueModel tests" ON)
if(PEDALCUES_BUILD_TESTS)
    juce_add_console_app(PedalCuesTests PRODUCT_NAME "PedalCuesTests")
    target_sources(PedalCuesTests PRIVATE Tests/CueModelTests.cpp Source/CueModel.cpp Source/State.cpp Source/DeviceTemplates.cpp Source/Modellers.cpp Source/Update.cpp Source/QcUsb.cpp)
    target_compile_definitions(PedalCuesTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
        JucePlugin_VersionString="${PROJECT_VERSION}"
        PEDALCUES_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures")
    target_link_libraries(PedalCuesTests PRIVATE juce::juce_audio_basics juce::juce_graphics juce::juce_data_structures hidapi::hidapi
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
    Source/DeviceTemplates.cpp
    Source/Modellers.cpp
        Source/Theme.cpp
        Source/Tile.cpp
        Source/EditorCommon.cpp
        Source/QcPage.cpp
        Source/WhammyPage.cpp
        Source/MovesPanel.cpp
        Source/QcExpression.cpp
    Source/KemperPage.cpp
    Source/CustomPage.cpp
    Source/ModellerPage.cpp
    Source/UnitPicker.cpp
        Source/SettingsPage.cpp
        Source/Tour.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Update.cpp
        Source/QcUsb.cpp
        Source/QcSyncDialog.cpp)
    target_compile_definitions(DocShots PRIVATE
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
        JucePlugin_Name="PedalCues"
        JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(DocShots PRIVATE juce::juce_audio_utils hidapi::hidapi
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
