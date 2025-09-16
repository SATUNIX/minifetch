# minifetch

`minifetch` is a tiny, pure-C system information fetcher with optional ASCII logo animation and layout switching. The binary builds cleanly on modern Linux distributions using only libc and POSIX APIs.

## Repository Layout
- `minifetch.c` — main program containing collectors, renderers, CLI parsing, and animation loop.
- `config.h` — compile-time defaults for feature toggles, colors, layout, and animation behaviour; treated as the public configuration surface.
- `embed_logo.sh` — regenerates `logo_embedded.inc` by embedding frames from `frames/` or `logo.txt` into C arrays.
- `frames/` — sample animated logo frames (`logo0.txt`, `logo1.txt`, `logo2.txt`). Use this directory to add or replace animation frames.
- `logo.txt` — fallback static logo for single-frame builds.
- `install.sh` — convenience wrapper that runs the embed script and compiles `minifetch`; optionally installs to `/usr/local/bin`.
- `logo_embedded.inc` — generated header providing the embedded frames. Committed so the repo runs out-of-the-box, but regenerated on every build.

## Build & Installation
```sh
./install.sh        # refresh embedded frames and build ./minifetch
./install.sh install
```
Passing `install` copies the binary to `/usr/local/bin/minifetch` (uses `install(1)`; run with the privileges your system requires). Manual builds mirror the script:
```sh
./embed_logo.sh
cc -O2 -pipe -s -Wall -Wextra -Wpedantic -Wno-unused-parameter -std=c99 -o minifetch minifetch.c
```

## Runtime Usage
```
minifetch [--layout=side|top] [--animate=off|left|top]
          [--mode=loop|pingpong|once] [--fps=N] [--duration=SECONDS]
          [-h|--help]
```
- `--layout` switches between logo-left/info-right (`side`, default) and logo-above/info-below (`top`).
- `--animate` controls whether the logo region animates and where the frames draw; incompatible requests are coerced to a valid area with a warning.
- `--mode` selects an animation sequence pattern.
- `--fps` and `--duration` clamp to safe ranges (1–60 FPS, max 24h). `--duration=0` runs indefinitely for loop/pingpong.
- When stdout is not a TTY and `CFG_ANIM_PLAY_ON_TTY_ONLY` is true, animation is disabled automatically.

## Configuration Surface (`config.h`)
Key macros provide the default experience and can be toggled per-build:
- `CFG_SHOW_*` flags decide which collectors run (distro, host, kernel, uptime, packages, shell, resolution, WM, DE, CPU, GPU, memory, disk).
- Colors: `CFG_LOGO_COLOR`, `CFG_LABEL_COLOR`, `CFG_VALUE_COLOR` (ANSI escape sequences).
- Layout: `CFG_GAP_SPACES`, `CFG_LAYOUT` (`CFG_LAYOUT_SIDE` or `CFG_LAYOUT_TOP`).
- Animation: enable/disable (`CFG_ANIM_ENABLED`), area (`CFG_ANIM_LEFT` / `CFG_ANIM_TOP`), mode (`CFG_ANIM_OFF`, `_LOOP`, `_PINGPONG`, `_ONCE`), `CFG_ANIM_FPS`, `CFG_ANIM_DURATION_SEC`, `CFG_ANIM_PLAY_ON_TTY_ONLY`, `CFG_USE_ALT_SCREEN`, `CFG_HIDE_CURSOR_DURING_ANIM`.
Override macros at compile time (e.g. `cc ... -DCFG_ANIM_ENABLED=1`). Runtime flags can refine most animation choices.

## Logo & Animation Workflow
1. Drop static art in `logo.txt` **or** provide ordered frames under `frames/logo*.txt` (lexicographic order defines playback).
2. Run `./embed_logo.sh` (automatically done by `install.sh`).
3. Commit the regenerated `logo_embedded.inc` alongside any new assets so the repo remains runnable without extra steps.

## Quirks & Notes
- Collectors prefer `/proc`, `/sys`, and builtin syscalls; optional tools (`hyprctl`, `wlr-randr`, `xrandr`, `lspci`) are used when present and silently skipped otherwise.
- Animation uses ANSI control sequences only when stdout is a TTY; Ctrl+C restores cursor visibility and exits cleanly.
- Unequal frame sizes are padded to the widest frame width, preventing ghosting during animation.

## Developer TODO / Testing Guide
Use this checklist before pushing changes:
- [ ] `./embed_logo.sh` then `cc -O2 -pipe -s -Wall -Wextra -Wpedantic -Wno-unused-parameter -std=c99 -o minifetch minifetch.c` — verify a warning-free build.
- [ ] Run `./minifetch` on a TTY with default config (`CFG_ANIM_ENABLED=0`) and confirm layout spacing and color resets.
- [ ] Enable animation (`./minifetch --animate=left --mode=loop --fps=10`) and watch for clean cursor/alt-screen behaviour; interrupt with Ctrl+C to ensure state restoration.
- [ ] Test top layout (`./minifetch --layout=top --animate=top --mode=pingpong`) to confirm static info placement and animation area adjustment warnings.
- [ ] Pipe output (`./minifetch | head`) to verify animation suppression and graceful first-frame rendering.
- [ ] Spot-check collectors: distro/host/kernel/uptime on the current system; ensure missing optional tools fall back to `n/a` without delays.
- [ ] Regenerate frames after editing assets and confirm `logo_embedded.inc` changes are deterministic (repeat `./embed_logo.sh` twice and diff).
