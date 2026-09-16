# Project agent memory

This file is the project's committed home for project-intrinsic agent knowledge: build, test, release, architecture, and sharp-edge notes that should travel with the code.

- Add durable project-specific notes here as they are discovered through real work.

## Static analysis

- `.clang-tidy` (checks + suppressed-check rationale) and `.cppcheck-suppressions` live at repo root, scoped to `src/` only — never `_deps` (vendored whisper.cpp).
- CI runs cppcheck only, as a blocking `static-analysis` job in `.github/workflows/build.yml` (no Qt install or compile database needed — `--library=qt` covers Qt's macros/ownership well enough).
- clang-tidy needs a real compile database (`cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, Qt installed) so it isn't wired into CI — run it locally/in an IDE against `src/` when doing a deeper pass.

## Maintaining this file

Keep this file for knowledge useful to almost every future agent session in this project.
Do not repeat what the codebase already shows; point to the authoritative file or command instead.
Prefer rewriting or pruning existing entries over appending new ones.
When updating this file, preserve this bar for all agents and keep entries concise.
