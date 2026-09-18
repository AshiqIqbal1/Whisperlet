cmake_minimum_required(VERSION 3.19)

# Single source of truth for the app version: reused for the macOS bundle
# metadata below and baked into a generated header so the running app can
# report its own version (e.g. for the update checker) without a second
# hardcoded copy drifting out of sync.
set(WHISPERLET_VERSION "0.7.1")

# OBJCXX: the macOS overlay backend is Objective-C++ (AppKit).
if(APPLE)
    project(Whisperlet VERSION ${WHISPERLET_VERSION} LANGUAGES CXX OBJCXX)
else()
    project(Whisperlet VERSION ${WHISPERLET_VERSION} LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg Network Multimedia Concurrent LinguistTools Test)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# whisper.cpp, vendored with FetchContent and pinned to a commit so the C API
# cannot drift out from under whisperengine.
#
# Both platforms transcribe on the GPU: Metal on Apple silicon, Vulkan on
# Windows. Each falls back to CPU when the hardware or SDK is missing.
# ---------------------------------------------------------------------------
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GGML_BLAS OFF CACHE BOOL "" FORCE)

# Metal on Apple silicon: transcription runs on the GPU, which is several
# times faster and cheaper on battery than pegging the CPU cores. Embedding
# the shader library keeps the .app self contained.
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

    # GPU on Windows. macOS gets Metal, and without an equivalent the same
    # model runs an order of magnitude slower here on CPU alone. Vulkan is
    # the portable choice: Intel, AMD and NVIDIA drivers all ship it, unlike
    # CUDA which would be NVIDIA only and a huge toolchain.
    #
    # Enabled only when the SDK is present at build time, and ggml still
    # falls back to CPU at runtime on a machine with no Vulkan device, so
    # neither a plain local build nor an old GPU is left broken.
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Vulkan found: enabling GPU acceleration for Windows")
        set(GGML_VULKAN ON CACHE BOOL "" FORCE)
    else()
        message(STATUS "Vulkan SDK not found: Windows build will be CPU only")
    endif()
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

configure_file(
    resources/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

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
    src/core/updatechecker.h
    src/core/updatechecker.cpp
    src/core/versioncompare.h
    src/core/versioncompare.cpp
    src/core/singleinstanceguard.h
    src/core/singleinstanceguard.cpp
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
    src/platform/overlaywindow.h
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
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Global hotkey needs a native, OS-specific backend. Carbon's hotkey API is
# plain C — no Objective-C needed — and is what most macOS menu-bar utilities
# still use for a systemwide shortcut that needs no Accessibility permission.
if(APPLE)
    target_sources(Whisperlet PRIVATE
        src/platform/globalhotkey_mac.cpp
        src/platform/textinjector_mac.cpp
        src/platform/overlaywindow_mac.mm
        assets/whisperlet.icns
    )
    # Carbon: global hotkey. ApplicationServices: AX* trust check + CGEvent
    # keystroke synthesis for dictation paste.
    target_link_libraries(Whisperlet PRIVATE
        "-framework Carbon"
        "-framework ApplicationServices"
        "-framework AppKit"
    )
    set_source_files_properties(assets/whisperlet.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    # Custom Info.plist: NSMicrophoneUsageDescription is mandatory for mic
    # capture — without it macOS never even shows the permission prompt.
    set_target_properties(Whisperlet PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ashiqiqbal.whisperlet"
        MACOSX_BUNDLE_BUNDLE_NAME "Whisperlet"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${WHISPERLET_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_ICON_FILE "whisperlet.icns"
    )
    # Ad-hoc sign every build. macOS ties Accessibility/Microphone grants to
    # the app's signature; an unsigned bundle looks like a brand-new app on
    # each rebuild, so the permission prompts come back every launch.
    add_custom_command(TARGET Whisperlet POST_BUILD
        COMMAND codesign --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:Whisperlet>"
        COMMENT "Ad-hoc signing bundle (keeps macOS permission grants)"
        VERBATIM
    )
elseif(WIN32)
    target_sources(Whisperlet PRIVATE
        src/platform/globalhotkey_win.cpp
        src/platform/textinjector_win.cpp
        src/platform/overlaywindow_win.cpp
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

enable_testing()

# Pure logic, no QObject/moc, no network, no event loop — just the version
# comparison the update checker relies on.
add_executable(versioncompare_test
    tests/versioncompare_test.cpp
    src/core/versioncompare.h
    src/core/versioncompare.cpp
)
target_include_directories(versioncompare_test PRIVATE src/core)
target_link_libraries(versioncompare_test PRIVATE Qt::Core)
add_test(NAME VersionCompareTest COMMAND versioncompare_test)

# Exercises the local-socket lock directly: second launch defers to the
# first and pings it to activate, and a stale socket file left by an
# unclean exit doesn't block a later launch from becoming primary.
qt_add_executable(singleinstanceguard_test
    tests/singleinstanceguard_test.cpp
    src/core/singleinstanceguard.h
    src/core/singleinstanceguard.cpp
)
target_include_directories(singleinstanceguard_test PRIVATE src/core)
target_link_libraries(singleinstanceguard_test PRIVATE
    Qt::Core
    Qt::Network
    Qt::Test
)
add_test(NAME SingleInstanceGuardTest COMMAND singleinstanceguard_test)

# Drives the real SettingsDialog "Check for Updates" button (checking/
# disabled state, success, and failure) against a local stand-in HTTP
# server — no real network, no macOS Accessibility/Screen Recording
# permissions needed since it builds the widget in-process rather than
# automating another window.
qt_add_executable(settingsdialog_update_test
    tests/settingsdialog_update_test.cpp
    src/ui/settingsdialog.h
    src/ui/settingsdialog.cpp
    src/ui/theme.h
    src/ui/theme.cpp
    src/core/modelcatalog.h
    src/core/modelcatalog.cpp
    src/core/modelmanager.h
    src/core/modelmanager.cpp
    src/core/updatechecker.h
    src/core/updatechecker.cpp
    src/core/versioncompare.h
    src/core/versioncompare.cpp
    src/platform/globalhotkey.h
    src/platform/globalhotkey_common.cpp
    src/platform/textinjector.h
)
target_include_directories(settingsdialog_update_test PRIVATE
    src
    src/ui
    src/core
    src/platform
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(settingsdialog_update_test PRIVATE
    Qt::Core
    Qt::Widgets
    Qt::Network
    Qt::Test
)
if(APPLE)
    target_sources(settingsdialog_update_test PRIVATE
        src/platform/globalhotkey_mac.cpp
        src/platform/textinjector_mac.cpp
    )
    target_link_libraries(settingsdialog_update_test PRIVATE
        "-framework Carbon"
        "-framework ApplicationServices"
        "-framework AppKit"
    )
elseif(WIN32)
    target_sources(settingsdialog_update_test PRIVATE
        src/platform/globalhotkey_win.cpp
        src/platform/textinjector_win.cpp
    )
endif()
add_test(NAME SettingsDialogUpdateTest COMMAND settingsdialog_update_test)
set_tests_properties(SettingsDialogUpdateTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Drives the play-button trigger path end to end: a real TranscriptCard
# with a clip on disk emits playRequested(id) when its play button is
# clicked, and that id resolves through AudioClipStore to a WAV file
# QMediaPlayer can actually load — the same two steps MainWindow::playClip
# chains together. Builds the card and AudioClipStore in-process rather
# than the full MainWindow (whose RecordingPill needs a real native window
# handle, unavailable under the offscreen QPA platform this test runs
# under) — no macOS Accessibility/Screen Recording permissions needed.
qt_add_executable(playback_trigger_test
    tests/playback_trigger_test.cpp
    src/ui/theme.h
    src/ui/theme.cpp
    src/ui/icons.h
    src/ui/icons.cpp
    src/ui/transcriptcard.h
    src/ui/transcriptcard.cpp
    src/core/audioclipstore.h
    src/core/audioclipstore.cpp
)
target_include_directories(playback_trigger_test PRIVATE
    src
    src/ui
    src/core
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(playback_trigger_test PRIVATE
    Qt::Core
    Qt::Widgets
    Qt::Svg
    Qt::Multimedia
    Qt::Test
)
add_test(NAME PlaybackTriggerTest COMMAND playback_trigger_test)
set_tests_properties(PlaybackTriggerTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
