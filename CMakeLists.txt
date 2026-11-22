cmake_minimum_required(VERSION 3.22)
project(LYBLiveVocal VERSION 0.1.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(formats VST3 Standalone)
if(APPLE)
  list(APPEND formats AU)
endif()
juce_add_plugin(LYBLiveVocal
  COMPANY_NAME "LYB" BUNDLE_ID "com.lyb.livevocal"
  PLUGIN_MANUFACTURER_CODE Lybe PLUGIN_CODE Lyvl
  FORMATS ${formats} PRODUCT_NAME "LYB Live Vocal"
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(LYBLiveVocal PRIVATE Source/Plugin.cpp)
target_compile_definitions(LYBLiveVocal PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LYBLiveVocal PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

include(CTest)
if(BUILD_TESTING)
  juce_add_console_app(LYBSmokeTest PRODUCT_NAME "LYB Smoke Test")
  target_sources(LYBSmokeTest PRIVATE tests/SmokeTest.cpp Source/Plugin.cpp)
  target_compile_definitions(LYBSmokeTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(LYBSmokeTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
  add_test(NAME LYBAudioSmoke COMMAND LYBSmokeTest)
endif()
