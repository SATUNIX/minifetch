# Repository Guidelines

## Project Structure & Module Organization
Runtime sources live in `src/` with matching headers in `include/`; keep shared declarations in headers to avoid duplication. Generated assets (notably `build/logo_data.c`) come from the ASCII art in `frames/logo.txt` via `tools/embed_logo.sh`. Place new scripts in `tools/` and shell-based checks in `tests/`. The default build output directory is `build/`; do not commit its contents.

## Build, Test, and Development Commands
- `make` – builds `minifetch` with the default feature set; outputs binaries at the repo root.
- `make minifetch-linux` – builds with Linux-only collectors enabled.
- `cmake -S . -B build && cmake --build build` – preferred multi-configuration build, mirrors Makefile targets.
- `ctest --test-dir build --output-on-failure` – runs the CTest smoke suite after a CMake build.
- `tests/smoke.sh` – executes the same smoke checks directly; set `MINIFETCH_BIN` if testing a non-default binary.

## Coding Style & Naming Conventions
Target ISO C99 with warnings treated seriously (`-Wall -Wextra -pedantic`); compile cleanly before sending patches. Use four-space indentation, brace-on-new-line style, and `snake_case` for variables and functions. Reserve UpperCamelCase for types and ALL_CAPS for macros or compile-time constants. Keep lines under ~100 characters and prefer static helpers in `.c` files unless they are shared. Run `clang-format` if available locally but do not check in formatting configs without discussion.

## Testing Guidelines
All contributions should keep `tests/smoke.sh` passing; extend it when adding collectors or CLI flags. Name new shell tests descriptively (e.g., `tests/uptime_smoke.sh`) and gate them behind CTest targets via `add_test`. If functionality depends on environment detection, include a fake fixture or guard to ensure deterministic output.

## Commit & Pull Request Guidelines
Recent history uses short imperative summaries (e.g., “Update README.md”); follow that pattern while providing enough context. Group related changes per commit and note generated files in the body. PRs should describe observable behavior changes, list test commands run, and attach screenshots when output formatting shifts. Link tracking issues or TODO entries and call out platform assumptions (Linux vs. portable builds).

## Configuration Tips
Default colors and field toggles live in `include/config.h`. When adjusting defaults, document the rationale in the PR and mention any corresponding CLI flags so downstream packagers can opt out.
