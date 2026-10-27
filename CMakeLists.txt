cmake_minimum_required(VERSION 3.22)
project(VocLyrics VERSION 1.3.1 LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE)
FetchContent_Declare(CivetWeb
    GIT_REPOSITORY https://github.com/civetweb/civetweb.git
    # Exact upstream commit containing the v1.16 URI parser security fix.
    GIT_TAG d7ba35bbb649209c66e582d5a0244ba988a15159
    GIT_SHALLOW TRUE)
FetchContent_Declare(qrcodegen
    GIT_REPOSITORY https://github.com/nayuki/QR-Code-generator.git
    GIT_TAG v1.8.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
FetchContent_GetProperties(civetweb)
if(NOT civetweb_POPULATED)
    FetchContent_Populate(CivetWeb)
endif()
add_library(civetweb-c-library STATIC ${civetweb_SOURCE_DIR}/src/civetweb.c)
target_include_directories(civetweb-c-library PUBLIC ${civetweb_SOURCE_DIR}/include PRIVATE ${civetweb_SOURCE_DIR}/src)
target_compile_definitions(civetweb-c-library PUBLIC USE_WEBSOCKET PRIVATE NO_SSL NO_CGI NO_CACHING)
find_package(Threads REQUIRED)
target_link_libraries(civetweb-c-library PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(civetweb-c-library PUBLIC ws2_32)
endif()
FetchContent_GetProperties(qrcodegen)
if(NOT qrcodegen_POPULATED)
    FetchContent_Populate(qrcodegen)
endif()
add_library(qrcodegen STATIC ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp)
target_include_directories(qrcodegen PUBLIC ${qrcodegen_SOURCE_DIR}/cpp)
target_compile_features(qrcodegen PUBLIC cxx_std_11)

juce_add_binary_data(VocLyricsWeb
    NAMESPACE VocLyricsWeb
    HEADER_NAME VocLyricsWebData.h
    SOURCES WebRemote/index.html WebRemote/app.js WebRemote/key-logic.js WebRemote/style.css WebRemote/manifest.json)

juce_add_plugin(VocLyrics
    COMPANY_NAME "VocLyrics"
    BUNDLE_ID "com.voclyrics.plugin"
    PLUGIN_MANUFACTURER_CODE VocL
    PLUGIN_CODE VLy1
    PRODUCT_NAME "VocLyrics"
    DESCRIPTION "Standalone shared lyrics and chords teleprompter"
    FORMATS Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx)
juce_generate_juce_header(VocLyrics)

target_sources(VocLyrics PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/Lyrics/LrcParser.cpp Source/Lyrics/SongModel.cpp
    Source/Lyrics/SetlistModel.cpp Source/Lyrics/LiveTagger.cpp
    Source/Network/ServerManager.cpp Source/Network/NetworkUtils.cpp
    Source/Network/QrCode.cpp Source/Network/SyncClock.cpp
    Source/UI/SetlistPanel.cpp Source/UI/EditorPanel.cpp
    Source/UI/DisplayView.cpp Source/UI/RemotePanel.cpp
    Source/UI/LookAndFeel.cpp)
target_include_directories(VocLyrics PRIVATE Source Source/Lyrics Source/Network Source/UI)
target_compile_features(VocLyrics PRIVATE cxx_std_17)
target_compile_definitions(VocLyrics PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    VOC_LYRICS_BUILD_VERSION="${PROJECT_VERSION}")
target_link_libraries(VocLyrics PRIVATE
    VocLyricsWeb civetweb-c-library qrcodegen
    juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

include(CTest)
if(BUILD_TESTING)
    add_executable(VocLyricsTests
        Tests/TestMain.cpp
        Source/Lyrics/LrcParser.cpp Source/Lyrics/SongModel.cpp
        Source/Lyrics/SetlistModel.cpp Source/Lyrics/LiveTagger.cpp
        Source/Network/SyncClock.cpp Source/Network/ServerManager.cpp
        Source/Network/NetworkUtils.cpp)
    target_include_directories(VocLyricsTests PRIVATE Source Source/Lyrics Source/Network)
    target_compile_features(VocLyricsTests PRIVATE cxx_std_17)
    target_link_libraries(VocLyricsTests PRIVATE
        VocLyricsWeb civetweb-c-library juce::juce_core juce::juce_data_structures
        juce::juce_events juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME VocLyricsTests COMMAND VocLyricsTests)
endif()
