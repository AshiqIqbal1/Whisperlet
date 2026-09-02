cmake_minimum_required(VERSION 3.19)
project(Whisperlet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg Network Multimedia Concurrent LinguistTools)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# whisper.cpp — vendored via FetchContent, pinned to a tagged release so the
# C API can't drift out from under mainwindow/whisperengine.
# Metal/CUDA/BLAS backends are left off: CPU-only keeps the exact same build
# working unmodified on the Windows laptop, at the cost of raw speed. Turn
# GGML_METAL back on for Mac-only builds later if you want it faster.
# ---------------------------------------------------------------------------
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GGML_BLAS OFF CACHE BOOL "" FORCE)

# Metal on Apple Silicon: transcription runs on the GPU — several times
# faster AND cheaper on battery than pegging the CPU cores (race-to-sleep).
# Embedding the shader library keeps the .app self-contained. Windows stays
# CPU-only; the tree still builds there unchanged.
if(APPLE)
    set(GGML_METAL ON CACHE BOOL "" FORCE)
    set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
else()
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
endif()

# Windows binaries are DISTRIBUTED (built on CI, run elsewhere), so never
# bake the build machine's CPU flags in (GGML_NATIVE). Pin an AVX2+FMA
# baseline instead: fast SIMD kernels, runs on every x86-64 CPU since ~2013.
if(WIN32)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set(GGML_AVX ON CACHE BOOL "" FORCE)
    set(GGML_AVX2 ON CACHE BOOL "" FORCE)
    set(GGML_FMA ON CACHE BOOL "" FORCE)
    set(GGML_F16C ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    whisper_cpp
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    # v1.9.2, pinned by commit — a tag can be moved upstream, a hash can't.
    GIT_TAG        306c88f4d1286aec1bf96e544632897886af5501
)
FetchContent_MakeAvailable(whisper_cpp)

# ggml kernels are 10-50x slower unoptimized, which makes a Debug build of
# the app feel broken (a 5s clip taking a minute on Tiny). We never debug
# INTO whisper, so force optimization on it even in Debug configs.
# MSVC is excluded: /O2 clashes with the /RTC1 flag CMake adds to Debug —
# on the Windows laptop just build the Release config for real use.
if(NOT MSVC)
    foreach(ggml_target IN ITEMS whisper ggml ggml-base ggml-cpu)
        if(TARGET ${ggml_target})
            target_compile_options(${ggml_target} PRIVATE $<$<CONFIG:Debug>:-O2>)
        endif()
    endforeach()
endif()

qt_add_executable(Whisperlet
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/theme.h
    src/ui/theme.cpp
    src/ui/icons.h
    src/ui/icons.cpp
    src/ui/recordbutton.h
    src/ui/recordbutton.cpp
    src/ui/transcriptcard.h
    src/ui/transcriptcard.cpp
    src/ui/settingsdialog.h
    src/ui/settingsdialog.cpp
    src/core/modelcatalog.h
    src/core/modelcatalog.cpp
    src/core/modelmanager.h
    src/core/modelmanager.cpp
    src/core/whisperengine.h
    src/core/whisperengine.cpp
    src/core/audiorecorder.h
    src/core/audiorecorder.cpp
    src/core/audioutil.h
    src/core/audioutil.cpp
    src/core/audioclipstore.h
    src/core/audioclipstore.cpp
    src/core/audiofiledecoder.h
    src/core/audiofiledecoder.cpp
    src/core/transcriptstore.h
    src/core/transcriptstore.cpp
    src/ui/titlebar.h
    src/ui/titlebar.cpp
    src/ui/recordingpill.h
    src/ui/recordingpill.cpp
    src/platform/globalhotkey.h
    src/platform/globalhotkey_common.cpp
    src/platform/textinjector.h
)

# In-app logo (title bar); bundle/EXE icons are handled per-platform below.
qt_add_resources(Whisperlet "appassets"
    PREFIX "/"
    FILES assets/icon-64.png
)

# Headers include each other by bare name ("theme.h") regardless of layer.
target_include_directories(Whisperlet PRIVATE
    src
    src/ui
    src/core
    src/platform
)

# Global hotkey needs a native, OS-specific backend. Carbon's hotkey API is
# plain C — no Objective-C needed — and is what most macOS menu-bar utilities
# still use for a systemwide shortcut that needs no Accessibility permission.
if(APPLE)
    target_sources(Whisperlet PRIVATE
        src/platform/globalhotkey_mac.cpp
        src/platform/textinjector_mac.cpp
        assets/whisperlet.icns
    )
    # Carbon: global hotkey. ApplicationServices: AX* trust check + CGEvent
    # keystroke synthesis for dictation paste.
    target_link_libraries(Whisperlet PRIVATE
        "-framework Carbon"
        "-framework ApplicationServices"
    )
    set_source_files_properties(assets/whisperlet.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    # Custom Info.plist: NSMicrophoneUsageDescription is mandatory for mic
    # capture — without it macOS never even shows the permission prompt.
    set_target_properties(Whisperlet PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ashiqiqbal.whisperlet"
        MACOSX_BUNDLE_BUNDLE_NAME "Whisperlet"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.3.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_ICON_FILE "whisperlet.icns"
    )
elseif(WIN32)
    target_sources(Whisperlet PRIVATE
        src/platform/globalhotkey_win.cpp
        src/platform/textinjector_win.cpp
        resources/whisperlet.rc
    )
endif()

qt_add_translations(
    TARGETS Whisperlet
    TS_FILES i18n/Whisperlet_en_AU.ts
)

target_link_libraries(Whisperlet
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Svg
        Qt::Network
        Qt::Multimedia
        Qt::Concurrent
        whisper
)

install(TARGETS Whisperlet
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Whisperlet
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
