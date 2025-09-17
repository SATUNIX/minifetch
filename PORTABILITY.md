# Portability Notes

`minifetch` targets ANSI C (C89) with POSIX.1-2008 interfaces and keeps the core binary free of Linux-specific dependencies.

## Supported Toolchains
- GCC or Clang on glibc systems (Debian, Ubuntu, Arch Linux, etc.).
- Clang + libc on macOS.
- GCC or Clang on musl-based distributions (Alpine) and BSDs.

Compile with:
```
make                # core, portable build
make minifetch-linux # adds Linux-only collectors
```

## Runtime Expectations
- Core collectors rely on `uname(2)`, `statvfs(3)`, `sysconf(3)`, and environment variables. They should succeed on any POSIX-like OS.
- Linux extras are guarded by `#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)`. When compiled elsewhere, they return `-1` so the caller skips the field.
- The renderer detects TTYs with `isatty(3)` and suppresses colour when output is redirected.
- The static logo is stored as a table of UTF-8 strings; rendering logic measures display width per codepoint to remain terminal-agnostic.

## Optional Linux Dependencies
When building with `MINIFETCH_LINUX_EXT`, the following files are read if present:
- `/etc/os-release` (distro name)
- `/proc/meminfo` (memory totals, using `MemAvailable` fallback logic)
- `/proc/uptime` (uptime formatting)

Absent files or parse errors degrade gracefully, causing the line to be omitted.

## Known Gaps & Follow-Up
- No Windows support (requires POSIX shims).
- Wide-character column width detection is byte-counting; terminals that double-width CJK characters may misalign. Future improvements could integrate `wcwidth` with a permissive licence.
- CI matrix should exercise glibc, musl, and at least one BSD runner (see TODO).
