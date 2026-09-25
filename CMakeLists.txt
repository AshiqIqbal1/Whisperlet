cmake_minimum_required(VERSION 3.19)

# Single source of truth for the app version: reused for the macOS bundle
# metadata below and baked into a generated header so the running app can
# report its own version (e.g. for the update checker) without a second
# hardcoded copy drifting out of sync.
#
# A CACHE variable so CI can override it: build.yml passes the release tag
# (minus its leading "v") as -DWHISPERLET_VERSION when building a tag, so
# released binaries report the version they were tagged with. The value here
# is only the fallback for local/dev builds and untagged CI runs.
set(WHISPERLET_VERSION "0.7.1" CACHE STRING "Application version (CI sets this from the release tag)")

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

# ASan + UBSan for our own code (the app and every test), used by the
# sanitizers CI job. Added after FetchContent_MakeAvailable on purpose so
# whisper.cpp/ggml stay uninstrumented: they are vendored, and the ASan
# runtime still catches heap misuse of buffers we hand across that
# boundary. UBSan is non-recoverable so a finding fails the test instead of
# only printing a warning.
option(WHISPERLET_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(WHISPERLET_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "WHISPERLET_SANITIZE needs Clang or GCC")
    endif()
    set(WHISPERLET_SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
    )
    add_compile_options(${WHISPERLET_SANITIZER_FLAGS})
    add_link_options(${WHISPERLET_SANITIZER_FLAGS})
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
        src/platform/globalhotkey_mac_keycodes.h
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

# Ids stored on AudioClipStore end up on disk (transcripts.json) and get fed
# straight back into path building, so this locks down that an id trying to
# escape the audio directory (`../..`, an absolute path, separators) is
# rejected rather than resolving outside it.
qt_add_executable(audioclipstore_security_test
    tests/audioclipstore_security_test.cpp
    src/core/audioclipstore.h
    src/core/audioclipstore.cpp
)
target_include_directories(audioclipstore_security_test PRIVATE src/core)
target_link_libraries(audioclipstore_security_test PRIVATE Qt::Core)
add_test(NAME AudioClipStoreSecurityTest COMMAND audioclipstore_security_test)

# Drives the real ModelManager download path against a local stand-in
# HTTP server: a model is only installed once the bytes on disk match the
# pinned hash and QSaveFile::commit() succeeds, and only counts as
# downloaded with a matching ".verified" sidecar. Also asserts every
# catalog hash is well formed.
qt_add_executable(modelmanager_verification_test
    tests/modelmanager_verification_test.cpp
    src/core/modelmanager.h
    src/core/modelmanager.cpp
    src/core/modelcatalog.h
    src/core/modelcatalog.cpp
)
target_include_directories(modelmanager_verification_test PRIVATE src/core)
target_compile_definitions(modelmanager_verification_test PRIVATE WHISPERLET_TESTING)
target_link_libraries(modelmanager_verification_test PRIVATE
    Qt::Core
    Qt::Network
    Qt::Test
)
add_test(NAME ModelManagerVerificationTest COMMAND modelmanager_verification_test)

# Pure logic, no CGEvent/AX calls: the chunk-boundary math the mac text
# injector uses to keep long dictated passages under
# CGEventKeyboardSetUnicodeString's per-event limit without splitting a
# surrogate pair (e.g. an emoji) across two events.
if(APPLE)
    add_executable(textinjector_mac_chunking_test
        tests/textinjector_mac_chunking_test.cpp
        src/platform/textinjector_mac_chunking.h
    )
    target_include_directories(textinjector_mac_chunking_test PRIVATE src/platform)
    target_link_libraries(textinjector_mac_chunking_test PRIVATE Qt::Core)
    add_test(NAME TextInjectorMacChunkingTest COMMAND textinjector_mac_chunking_test)
endif()

# AudioRecorder::process() carries the denoise/condition/resample pipeline
# that stop() used to run inline on the UI thread; this locks down that
# running it via QtConcurrent::run() (as MainWindow now does) still produces
# byte-identical output to running it synchronously.
qt_add_executable(audiorecorder_process_test
    tests/audiorecorder_process_test.cpp
    src/core/audiorecorder.h
    src/core/audiorecorder.cpp
    src/core/audioutil.h
    src/core/audioutil.cpp
)
target_include_directories(audiorecorder_process_test PRIVATE src/core)
target_link_libraries(audiorecorder_process_test PRIVATE
    Qt::Core
    Qt::Multimedia
    Qt::Concurrent
)
add_test(NAME AudioRecorderProcessTest COMMAND audiorecorder_process_test)

# No large sample-to-sample jump at the very start/end of a processed clip;
# see AudioUtil::fadeEdges for the click/pop fix this locks in.
qt_add_executable(audioutil_fade_test
    tests/audioutil_fade_test.cpp
    src/core/audioutil.h
    src/core/audioutil.cpp
)
target_include_directories(audioutil_fade_test PRIVATE src/core)
add_test(NAME AudioUtilFadeTest COMMAND audioutil_fade_test)

# Builds the input-device picker menu exactly as MainWindow's mic button
# does, under the app's global stylesheet, and checks the popup paints a
# panel with every device name contained in it. Covers #27, where the
# sheet's transparent QWidget background applied to QMenu as well and the
# names were drawn with nothing behind them.
qt_add_executable(devicemenu_test
    tests/devicemenu_test.cpp
    src/ui/theme.h
    src/ui/theme.cpp
)
target_include_directories(devicemenu_test PRIVATE src/ui)
target_link_libraries(devicemenu_test PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
)
add_test(NAME DeviceMenuTest COMMAND devicemenu_test)
set_tests_properties(DeviceMenuTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Single-modifier tap keys (#28): each platform lists the right keys with
# distinct picker labels, saved values keep their meaning, and on macOS each
# key maps to the side-specific keycode the event tap compares against.
qt_add_executable(globalhotkey_modkeys_test
    tests/globalhotkey_modkeys_test.cpp
    src/platform/globalhotkey.h
    src/platform/globalhotkey_common.cpp
    src/platform/globalhotkey_mac_keycodes.h
)
target_include_directories(globalhotkey_modkeys_test PRIVATE src/platform)
target_link_libraries(globalhotkey_modkeys_test PRIVATE Qt::Core Qt::Gui)
if(APPLE)
    target_sources(globalhotkey_modkeys_test PRIVATE src/platform/globalhotkey_mac.cpp)
    target_link_libraries(globalhotkey_modkeys_test PRIVATE
        "-framework Carbon"
        "-framework ApplicationServices"
    )
elseif(WIN32)
    target_sources(globalhotkey_modkeys_test PRIVATE src/platform/globalhotkey_win.cpp)
endif()
add_test(NAME GlobalHotkeyModKeysTest COMMAND globalhotkey_modkeys_test)

# Every *_test executable above, so CI can build just the tests without
# compiling the app and vendored whisper.cpp. Collected rather than listed
# so a new test is picked up without touching this.
get_property(whisperlet_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(FILTER whisperlet_targets INCLUDE REGEX "_test$")
add_custom_target(whisperlet_tests DEPENDS ${whisperlet_targets})

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
