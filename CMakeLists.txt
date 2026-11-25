cmake_minimum_required(VERSION 3.22)

project(LoopDeck VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "17.0" CACHE STRING "Minimum iOS version")
    set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "17.0" CACHE STRING "Minimum iOS version")
else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target")
endif()

option(LOOPDECK_BUILD_TESTS "Build the engine unit tests" ON)
option(LOOPDECK_COPY_PLUGIN "Copy the built plug-in into the system folders" OFF)

# ------------------------------------------------------------------------------
# JUCE
# ------------------------------------------------------------------------------
if (NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
endif()

if (EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "JUCE not found in ${JUCE_DIR}, fetching it from GitHub")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
# The loop library is packed into a single binary blob that is compiled into
# the plug-in, so the AUv3 app extension carries every loop with it.
# ------------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter)
if (NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Python 3 is required to pack the loop library (tools/build_loop_library.py)")
endif()

set(LOOPDECK_LIBRARY_BIN "${CMAKE_BINARY_DIR}/generated/loopdeck_library.bin")

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_loop_library.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/Loops"
            --output "${LOOPDECK_LIBRARY_BIN}"
    RESULT_VARIABLE LOOPDECK_PACK_RESULT)

if (NOT LOOPDECK_PACK_RESULT EQUAL 0)
    message(FATAL_ERROR "Packing the loop library failed")
endif()

juce_add_binary_data(LoopDeckData SOURCES "${LOOPDECK_LIBRARY_BIN}")

# ------------------------------------------------------------------------------
# Plug-in
# ------------------------------------------------------------------------------
set(LOOPDECK_FORMATS AUv3 Standalone)

if (APPLE)
    set(LOOPDECK_FORMATS Standalone AUv3)
else()
    # AUv3 only exists on Apple platforms. Elsewhere the standalone build is
    # what gets compiled, which is enough to develop and test the engine.
    set(LOOPDECK_FORMATS Standalone)
endif()

set(LOOPDECK_PLATFORM_FLAGS)

# The app-sandbox entitlement is macOS-only; adding it to an iOS bundle makes
# code signing fail against the provisioning profile. iOS apps are sandboxed by
# the OS already, so iOS needs no entitlements at all here.
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND LOOPDECK_PLATFORM_FLAGS
        APP_SANDBOX_ENABLED TRUE
        HARDENED_RUNTIME_ENABLED TRUE
        APP_SANDBOX_FILE_ACCESS_HOME_RO TRUE)
endif()

juce_add_plugin(LoopDeck
    COMPANY_NAME "Miiro Audio"
    BUNDLE_ID "com.miiroaudio.loopdeck"
    PLUGIN_MANUFACTURER_CODE Mira
    PLUGIN_CODE Lpdk
    PRODUCT_NAME "LoopDeck"
    DESCRIPTION "MIDI loop player for external synths and drum machines"
    FORMATS ${LOOPDECK_FORMATS}
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.png"
    MICROPHONE_PERMISSION_ENABLED FALSE
    COPY_PLUGIN_AFTER_BUILD ${LOOPDECK_COPY_PLUGIN}
    TARGETED_DEVICE_FAMILY "1,2"
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft
                               UIInterfaceOrientationLandscapeRight
                               UIInterfaceOrientationPortrait
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft
                             UIInterfaceOrientationLandscapeRight
                             UIInterfaceOrientationPortrait
                             UIInterfaceOrientationPortraitUpsideDown
    ${LOOPDECK_PLATFORM_FLAGS})

target_sources(LoopDeck PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/engine/LoopLibrary.cpp
    Source/engine/MidiEngine.cpp
    Source/engine/DrumMap.cpp
    Source/model/Parameters.cpp
    Source/ui/Theme.cpp
    Source/ui/Widgets.cpp
    Source/ui/SlotRow.cpp
    Source/ui/BrowserPanel.cpp
    Source/ui/Panels.cpp)

target_include_directories(LoopDeck PRIVATE Source)

target_compile_definitions(LoopDeck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(LoopDeck PRIVATE
    LoopDeckData
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
# iOS builds get no test target: the tests are host-side tooling.
if (LOOPDECK_BUILD_TESTS AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    juce_add_console_app(LoopDeckTests PRODUCT_NAME "LoopDeckTests")

    target_sources(LoopDeckTests PRIVATE
        Tests/EngineTests.cpp
        Source/engine/MidiEngine.cpp
        Source/engine/DrumMap.cpp
        Source/engine/LoopLibrary.cpp)

    target_include_directories(LoopDeckTests PRIVATE Source)

    target_compile_definitions(LoopDeckTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=0)

    target_link_libraries(LoopDeckTests PRIVATE
        LoopDeckData
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_core
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
