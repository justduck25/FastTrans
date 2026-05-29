cmake_minimum_required(VERSION 3.24)

project(JustDuckTranslator LANGUAGES CXX)

add_executable(JustDuckTranslator WIN32
    src/app.rc
    src/AppController.cpp
    src/CaptureManager.cpp
    src/HotkeyManager.cpp
    src/HistoryStore.cpp
    src/OcrEngine.cpp
    src/OverlayWindow.cpp
    src/RegionSelector.cpp
    src/SettingsStore.cpp
    src/TextUtil.cpp
    src/TranslationClient.cpp
    src/main.cpp
)

target_compile_features(JustDuckTranslator PRIVATE cxx_std_20)
target_compile_definitions(JustDuckTranslator PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

target_link_libraries(JustDuckTranslator PRIVATE
    ole32
    shell32
    shlwapi
    user32
    gdi32
    windowsapp
    winhttp
)
