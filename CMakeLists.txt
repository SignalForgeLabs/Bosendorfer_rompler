cmake_minimum_required(VERSION 3.22)

project(BosendorferImperial VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Fetch or link JUCE 7/8
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE juce)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# 2. Declare Binary Assets (Embedded Fallback + BinaryData)
juce_add_binary_data(PluginAssets
    SOURCES
        assets/stage_grand.png
        assets/piano_harp.png
        assets/Prototype_look_and_feel.png
        # 4 Selectable Microphones (Left & Right Floating Graphics + WAV IRs)
        assets/Neumann_KM184_L.png
        assets/Neuman_KM184_R.png
        assets/Mic_Neumann_KM184.wav
        assets/Neuman_TLM_L.png
        assets/Neuman_TLM_R.png
        assets/Mic_Neumann_TLM103.wav
        assets/Neuman_U87-L.png
        assets/Neuman_U87-R.png
        assets/Mic_Neumann_U87.wav
        assets/RCA_Ribbon_L.png
        assets/RCA_Ribbon_R.png
        "assets/RCA 44BX NEVE.wav"
        # SoundFont Banks & Room IRs
        assets/BOSENDRF.SF2
        assets/BOSENBRI.SF2
        assets/IRs/Room_ViennaKonzerthaus.wav
        assets/IRs/Room_MapleStage40ft.wav
        assets/IRs/Room_ScoringStudioHall.wav
)

# 3. Create the Plugin Target
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

if(DEFINED ENV{CI})
    set(AUTO_COPY_AFTER_BUILD FALSE)
else()
    set(AUTO_COPY_AFTER_BUILD TRUE)
endif()

juce_add_plugin(BosendorferImperial
    COMPANY_NAME "SignalForgeLabs"
    BUNDLE_ID "com.SignalForgeLabs.BosendorferImperial290"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${AUTO_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Sfal
    PLUGIN_CODE Bsd9
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Bosendorfer Imperial 290"
)

# 4. Source Files & Include Directories
target_sources(BosendorferImperial
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/BsendorferMicDSP.cpp
        src/BsendorferMicDSP.h
        src/MicAssetLoader.h
        src/PianoHarpViewComponent.h
        src/Parameters.h
        src/SoundFontEngine.h
        src/PianoAcousticEngine.h
        src/DamperClunkModel.h
        src/LicenseManager.h
        src/CenterMonitorView.h
        libs/tsf.h
)

target_include_directories(BosendorferImperial
    PRIVATE
        src
        libs
)

# Pass ASSETS_DIRECTORY macro so the GUI knows the local relative path in debug builds
target_compile_definitions(BosendorferImperial
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        ASSETS_DIR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(BosendorferImperial
        PUBLIC
            JUCE_ALSA=1
            JUCE_JACK=1
    )
endif()

# 5. Link JUCE Modules & Assets
target_link_libraries(BosendorferImperial
    PRIVATE
        PluginAssets
        ${CMAKE_DL_LIBS}
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_cryptography
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
