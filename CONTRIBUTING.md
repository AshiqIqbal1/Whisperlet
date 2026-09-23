# Contributing

## Building

See the [Building section of the README](README.md#building) for
prerequisites and the CMake commands.

## Tests

Run `ctest` from the build directory. Tests are registered in
`CMakeLists.txt` alongside the targets they cover.

To run them under AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset sanitize
cmake --build build/sanitize --target whisperlet_tests -j
ctest --test-dir build/sanitize --output-on-failure
```

## Coding conventions

No enforced style yet (see [#18](https://github.com/AshiqIqbal1/Whisperlet/issues/18)
for adding a `.clang-format`). Match the style of the surrounding code.

## Pull requests

Branch off `main` and open your PR against `main`. CI runs on every PR:

- `static-analysis`: cppcheck over `src/`
- `macos`: configure and build with Qt 6 on macOS
- `windows`: configure and build with Qt 6 on Windows

All three must pass before a PR can be merged. A PR that only touches
prose (markdown, `LICENSE`, the README screenshot, issue templates) skips
all three, which GitHub reports as success so the PR is still mergeable.

A separate `sanitizers` workflow builds and runs the tests with ASan and
UBSan. It is not a required check, but a failure there is a real bug.
