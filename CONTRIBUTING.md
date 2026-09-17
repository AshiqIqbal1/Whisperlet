# Contributing

## Building

See the [Building section of the README](README.md#building) for
prerequisites and the CMake commands.

## Tests

There are no automated tests yet.

## Coding conventions

No enforced style yet (see [#18](https://github.com/AshiqIqbal1/Whisperlet/issues/18)
for adding a `.clang-format`). Match the style of the surrounding code.

## Pull requests

Branch off `main` and open your PR against `main`. CI runs on every PR:

- `static-analysis`: cppcheck over `src/`
- `macos`: configure and build with Qt 6 on macOS
- `windows`: configure and build with Qt 6 on Windows

All three must pass before a PR can be merged.
