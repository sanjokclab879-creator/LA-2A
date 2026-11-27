cmake_minimum_required(VERSION 3.22)
project(LA2A_Clone VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

if(EXISTS "D:/Plugin Analysis/JUCE/CMakeLists.txt")
    add_subdirectory("D:/Plugin Analysis/JUCE" JUCE_build)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(LA2A_Clone
    COMPANY_NAME "SanjokLab"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE SjLb
    PLUGIN_CODE La2a
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "LA2A Clone")

juce_add_binary_data(LA2A_Assets SOURCES
    assets/LA2A_3D_Front_Panel.png
    assets/LA2A_3D_Knob_Strip64.png
    assets/LA2A_3D_R37_Strip64.png
    assets/LA2A_3D_VU_Body_ON.png
    assets/LA2A_3D_VU_Body_OFF.png
    assets/LA2A_3D_VU_Needle.png
    assets/LA2A_3D_Toggle_UP.png
    assets/LA2A_3D_Toggle_DOWN.png
    assets/LA2A_3D_Pilot_ON.png
    assets/LA2A_3D_Pilot_OFF.png)

juce_generate_juce_header(LA2A_Clone)

target_sources(LA2A_Clone PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(LA2A_Clone PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LA2A_Clone PRIVATE
    LA2A_Assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
