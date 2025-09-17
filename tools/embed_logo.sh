#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input-logo.txt> <output.c>" >&2
    exit 1
fi

input="$1"
output="$2"
outdir="$(dirname "$output")"

mkdir -p "$outdir"

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

python3 - "$input" "$tmp" <<'PY'
import sys
from pathlib import Path

if len(sys.argv) != 3:
    sys.exit("expected input and output paths")

inp = Path(sys.argv[1])
out_path = Path(sys.argv[2])

try:
    data = inp.read_text(encoding="utf-8").splitlines()
except FileNotFoundError:
    sys.exit(f"missing logo source: {inp}")


def escape(line: str) -> str:
    parts = []
    for byte in line.encode("utf-8"):
        ch = chr(byte)
        if ch == '\\':
            parts.append('\\\\')
        elif ch == '"':
            parts.append('\\"')
        elif 32 <= byte <= 126:
            parts.append(ch)
        else:
            parts.append('\\x{:02x}'.format(byte))
    return ''.join(parts)


def logo_width(lines):
    width = 0
    for line in lines:
        length = len(line)
        if length > width:
            width = length
    return width


with out_path.open("w", encoding="utf-8") as f:
    f.write('#include <stddef.h>\n')
    f.write('#include "logo.h"\n\n')
    f.write('const char *const g_logo_lines[] = {\n')
    if data:
        for line in data:
            f.write('    "' + escape(line) + '",\n')
    else:
        f.write('    "",\n')
    f.write('};\n')
    f.write('const size_t g_logo_line_count = ' + str(len(data)) + ';\n')
    f.write('const size_t g_logo_width = ' + str(logo_width(data)) + ';\n')
PY

mv "$tmp" "$output"
