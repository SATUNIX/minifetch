#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${MINIFETCH_BIN:-${ROOT_DIR}/minifetch}"
LINUX_BIN="${MINIFETCH_LINUX_BIN:-${ROOT_DIR}/minifetch-linux}"
ANSI_ESC="$(printf '\033')"

if [ ! -x "$BIN" ]; then
    echo "error: build minifetch before running smoke tests" >&2
    exit 1
fi

# Basic run should include non-empty logo column and kernel line
output="$("$BIN")"
printf '%s\n' "$output" | head -n 1 | grep "[^[:space:]]" >/dev/null 2>&1 || {
    echo "error: expected logo characters in first line" >&2
    exit 1
}
printf '%s\n' "$output" | grep "Kernel:" >/dev/null 2>&1 || {
    echo "error: expected Kernel line in default output" >&2
    exit 1
}

# Piped run should suppress ANSI escapes
piped_output="$("$BIN" | cat)"
if printf '%s\n' "$piped_output" | LC_ALL=C grep "$ANSI_ESC" >/dev/null 2>&1; then
    echo "error: expected no ANSI escapes when output is piped" >&2
    exit 1
fi

# Hidden mode should still emit something when run in non-TTY (falls back to plain output)
hidden_output="$("$BIN" --hidden)"
printf '%s\n' "$hidden_output" | grep "Kernel:" >/dev/null 2>&1 || {
    echo "error: hidden fallback should still print kernel info" >&2
    exit 1
}

if [ -x "$LINUX_BIN" ]; then
    extras="$("$LINUX_BIN" -a)"
    printf '%s\n' "$extras" | grep "Memory:" >/dev/null 2>&1 || {
        echo "error: expected Memory line when -a with linux build" >&2
        exit 1
    }
    printf '%s\n' "$extras" | grep "Uptime:" >/dev/null 2>&1 || {
        echo "error: expected Uptime line when -a with linux build" >&2
        exit 1
    }
fi

echo "smoke: ok"
