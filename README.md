# Whisperlet

[![build](https://github.com/AshiqIqbal1/Whisperlet/actions/workflows/build.yml/badge.svg)](https://github.com/AshiqIqbal1/Whisperlet/actions/workflows/build.yml)

Dictate anywhere on your machine. Press a shortcut, talk, press it again, and
the text is typed straight into whatever you had focused: a chat box, an
editor, a terminal. Transcription runs locally with
[whisper.cpp](https://github.com/ggml-org/whisper.cpp), so no audio ever
leaves the machine.

C++17 and Qt 6. One tree builds for Windows and macOS.

## Download

Prebuilt binaries are on the [releases page](https://github.com/AshiqIqbal1/Whisperlet/releases):
a `.zip` for Windows x64 and a `.dmg` for macOS on Apple silicon. Each release
also ships `SHA256SUMS.txt`.

Neither binary is code signed, so the OS warns on first run:

- Windows: SmartScreen appears, choose More info then Run anyway
- macOS: right click the app, Open, then Open again

On macOS only, dictation needs Accessibility permission (Privacy & Security,
then Accessibility) so the app can type into other applications. Windows
needs no permission for this.

## Using it

Open Settings with the gear icon and download a model. Then pick a shortcut,
either a key combination or a single tap of a right hand modifier key.

Press the shortcut, a small capsule appears at the top of the screen showing
that it is listening, and the dot moves with your voice. Click into any text
field while it records. Press again and the transcript is typed there.

You can also record with the button in the window, or drop an audio file
(mp3, m4a, wav and anything else the platform decodes) onto it. Transcripts
are searchable and kept between sessions. The recording itself is deleted
once the text exists, unless you turn on "Keep recordings after transcribing",
which also enables replay and re-running a clip through a different model.

## Models

| Model | Download | Notes |
|---|---|---|
| Tiny | 78 MB | fastest, least accurate, fine for quick notes |
| Base | 148 MB | default |
| Small | 488 MB | the sweet spot on a laptop |
| Medium | 1.5 GB | slower, more accurate |
| Large v3 turbo | 1.6 GB | best accuracy, heavy on modest hardware |

Downloaded on demand from the
[whisper.cpp Hugging Face repo](https://huggingface.co/ggerganov/whisper.cpp),
verified against a pinned SHA256, and cached in the app data directory.

## Audio handling

Recording quality drives accuracy more than anything except model choice, so
the app conditions every take automatically, with no settings to tune:

- captures at the microphone's native rate, downsampling for the model only
- 80 Hz high pass to remove rumble, hum and DC offset
- background noise suppression by spectral subtraction, estimating the room
  from the gaps between words (measured on a noisy test recording: 19 dB less
  background, speech within 0.3 dB, SNR 18.6 dB to 37.7 dB)
- automatic gain so a quiet microphone and a hot one both land at the same
  level, then a limiter
- silence trimmed before transcription, since whisper costs time per second
  of audio and dictation is largely pauses

## Building

Needs CMake 3.19+, git, and Qt 6.5+ with the Widgets, Svg, Network,
Multimedia and Concurrent modules. The first configure clones whisper.cpp,
so it needs network once.

```sh
cmake --preset release
cmake --build --preset release -j
```

Or open `CMakeLists.txt` in Qt Creator. If Qt is not found, point CMake at it
with `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<arch>`.

GPU acceleration is on by default where it is available: Metal on Apple
silicon, and Vulkan on Windows when the Vulkan SDK is present at build time.
Both fall back to CPU rather than failing.

On Windows, run `windeployqt` on the built exe before moving it to another
machine.

## Layout

```
src/ui        window, transcript cards, recording pill, settings
src/core      whisper engine, model downloads, audio capture and DSP
src/platform  global shortcut, text injection, overlay window (per OS)
tools         icon regeneration
```

## License

MIT, see [LICENSE](LICENSE).
