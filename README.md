# minifetch v3

`minifetch` is a third-generation refresh of the tiny fetch utility: a single-purpose system info reporter written in strict ANSI C (C89) with zero runtime dependencies beyond libc/POSIX. The program renders a static UTF-8 logo beside the data table, ships a lean collector set that works everywhere, and layers a Linux-only build for `/proc` niceties.

## Ethos
- **Portable first**: core build sticks to POSIX.1-2008 APIs and avoids `popen`, JSON parsers, or external binaries.
- **Predictable output**: UTF-8 logo is embedded at compile time; collectors return `-1` to drop unavailable lines gracefully.
- **Small & auditable**: ~970 lines of C/H/sh code (see `wc -l` above) spread across focused modules under `src/` and `include/`.

## Quick Start
### Requirements
- C compiler with C89 support (GCC/Clang work well)
- POSIX make **or** CMake ≥ 3.16
- `python3` (used by the logo embedding script)

### Make Build
```sh
make                # portable core binary ./minifetch
make minifetch-linux # extended build with MINIFETCH_LINUX_EXT
make clean          # remove binaries and build artefacts
```

### CMake Build
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```
CMake generates both executables and wires the smoke test through CTest. Variable `MINIFETCH_BIN`/`MINIFETCH_LINUX_BIN` is forwarded automatically.

## Usage
```
./minifetch [-a] [-c] [-q] [-h]
```
- `-a` include every collector (core + Linux extras when compiled with `MINIFETCH_LINUX_EXT`).
- `-c` force monochrome output even when stdout is a TTY.
- `-q` quiet mode; prints values only, one per line.
- `-h` show usage text.

Example (Linux build with `-a`):
```
████  ████  OS:        Arch Linux
██  ██  ██  Kernel:    6.16.7-arch1-1
██      ██  Host:      archlinux
██  ██  ██  CPU:       32
████  ████  Shell:     bash
            Disk:      10.8 GiB / 931 GiB (1%)
            Memory:    4.3 GiB / 126 GiB
            Uptime:    1h 14m
```
Piping the output (e.g., `./minifetch | cat`) suppresses colour automatically while preserving the value column.

## Configuration & Logo Workflow
- Compile-time toggles live in `include/config.h`. Adjust `CFG_SHOW_*`, colour ANSI escapes, or `CFG_LABEL_WIDTH` and rebuild (e.g., `make CFLAGS+="-DCFG_LABEL_WIDTH=12"`).
- Static art is sourced from `frames/logo.txt`. Edit the UTF-8 logo, then rebuild; the Makefile/CMake scripts regenerate `build/logo_data.c` through `tools/embed_logo.sh`.
- Embedding script escapes non-ASCII bytes and records display width so multi-byte glyphs keep the info column aligned.

## Tests
Run the bundled smoke test after each build:
```sh
./tests/smoke.sh
```
It verifies logo presence, colour suppression on pipes, and that Linux extras appear when available. CMake’s `ctest` target wraps the same script.

## Further Reading
- `PORTABILITY.md` documents supported platforms, fallbacks, and known gaps.
- `CONTRIBUTING.md` outlines coding style (C89, 4-space indent), testing expectations, and how to add new collectors.

minifetch v3 stays intentionally small, dependency-free, and friendly to both humans and automation. Enjoy the fetch.
