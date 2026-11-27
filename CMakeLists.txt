cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

# Reggirizer v1 = 1.0.0, v2 = 2.0.0, ... (plugin hosts need a numeric version)
project(Reggirizer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(Reggirizer
    COMPANY_NAME "Reggie Audio"
    BUNDLE_ID "com.reggieaudio.reggirizer"
    PLUGIN_MANUFACTURER_CODE Regg
    PLUGIN_CODE Rgzr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Reggirizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Reggirizer Standalone needs the microphone to tune your voice."
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Reggirizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PitchEngine.cpp)

target_compile_definitions(Reggirizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# MSVC: read sources as UTF-8 (the Hebrew text is escaped, but be safe).
if(MSVC)
    target_compile_options(Reggirizer PRIVATE /utf-8)
endif()

target_link_libraries(Reggirizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tool (not shipped): renders the editor to a PNG for checking the UI.
option(REGGIRIZER_BUILD_SNAPSHOT "Build the UI snapshot tool" OFF)
if(REGGIRIZER_BUILD_SNAPSHOT)
    juce_add_console_app(UISnapshot PRODUCT_NAME "UISnapshot")
    target_sources(UISnapshot PRIVATE
        Tests/UISnapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PitchEngine.cpp)
    target_compile_definitions(UISnapshot PRIVATE
        JucePlugin_Name="Reggirizer"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(UISnapshot PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

# =============================================================================
# 07BASED v2: Reggirizer + Blu Pill + Grapecrusher + Trap City on one bus,
# with the CRT-TV interface (HTML in Source/v2/ui, built by tools/make_ui.py)
# running in the system web view (WebKit on macOS, WebView2 on Windows).
# =============================================================================
juce_add_binary_data(BasedUIData
    HEADER_NAME BasedUIData.h
    NAMESPACE BasedUIData
    SOURCES
        Source/v2/ui/index.html
        Source/v2/ui/bridge.js
        JUCE/modules/juce_gui_extra/native/javascript/index.js
        JUCE/modules/juce_gui_extra/native/javascript/check_native_interop.js
        Source/v2/ui/fonts/Orbitron.ttf
        Source/v2/ui/fonts/VT323.ttf
        Source/v2/ui/fonts/Michroma.ttf
        Source/v2/ui/fonts/CinzelDecorative-Bold.ttf
        Source/v2/ui/fonts/CourierPrime-Bold.ttf
        Source/v2/ui/fonts/LilitaOne.ttf
        Source/v2/ui/fonts/RobotoCondensed.ttf
        Source/v2/ui/studio.jpg
        Source/v2/ui/blue_metal_plate.jpg
        Source/v2/ui/rusty_metal_04.jpg
        Source/v2/ui/metal_plate.jpg)

juce_add_plugin(SevenBased
    VERSION 2.0.2
    COMPANY_NAME "Reggie Audio"
    BUNDLE_ID "com.reggieaudio.07based"
    PLUGIN_MANUFACTURER_CODE Regg
    PLUGIN_CODE Bs07
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "07BASED"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "07BASED Standalone needs the microphone to run your voice through the effects."
    VST3_CATEGORIES Fx Distortion Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SevenBased PRIVATE
    Source/v2/BasedProcessor.cpp
    Source/v2/BasedEditor.cpp
    Source/PitchEngine.cpp)

target_compile_definitions(SevenBased PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(SevenBased PRIVATE /utf-8)
endif()

target_link_libraries(SevenBased
    PRIVATE
        BasedUIData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tests for the v2 audio engines (not shipped).
option(BASED_BUILD_TESTS "Build the 07BASED engine tests" OFF)
if(BASED_BUILD_TESTS)
    juce_add_console_app(BasedTest PRODUCT_NAME "BasedTest")
    target_sources(BasedTest PRIVATE
        Tests/BasedTest.cpp
        Source/v2/BasedProcessor.cpp
        Source/PitchEngine.cpp)
    target_compile_definitions(BasedTest PRIVATE
        JucePlugin_Name="07BASED"
        BASED_NO_EDITOR=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(BasedTest PRIVATE
        BasedUIData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
