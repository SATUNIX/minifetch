# Contributing Guidelines

## Workflow
1. Fork the repository and create a topic branch.
2. Regenerate the embedded logo (`make` regenerates `build/logo_data.c`) if you touched `frames/logo.txt`.
3. Build both variants (`make` and `make minifetch-linux`) and run `./tests/smoke.sh` before submitting a pull request.
4. Keep pull requests focused; describe behavioural changes, manual tests, and any portability considerations in the PR body.

## Coding Style
- Stick to ANSI C (C99). Declarations go at the top of each block; avoid mixed declarations and statements.
- Use 4-space indentation, no tabs. Braces stay on the same line as statements.
- Prefer `static` for internal functions; use `snake_case` for functions and variables, `UPPER_CASE` for macros.
- Copy strings with bounded helpers (`mf_strlcpy`) and sanitise inputs before printing. Avoid `strcpy`/`strcat`.
- Guard Linux-only code with `#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)`; return `-1` on failure so the caller can skip the field gracefully.

## Commit Messages
- Use imperative, concise subjects (`Add FreeBSD statvfs guard`).
- Mention regenerated artefacts explicitly (e.g., "Regenerate build/logo_data.c").
- Include notes about testing and platforms exercised when relevant.

## Testing
- `make`, `make minifetch-linux`, and `./tests/smoke.sh` must pass locally.
- When possible, compile on multiple libc/OS combinations (glibc, musl, BSD, macOS) and document the results in the PR.

## Adding Collectors
1. Decide whether the collector is portable (`src/core.c`) or Linux-only (`src/linux_extras.c`).
2. Expose a function prototype in the matching header.
3. Return `0` on success, `-1` on failure/unavailable.
4. Update `g_fields` in `src/main.c` and default toggles in `include/config.h`.
5. Extend tests or docs to cover the new field.

Thank you for helping keep `minifetch` portable and tidy.
