#!/usr/bin/env bash
set -euo pipefail

# Build and install minifetch
# Usage:
#   ./install.sh              # build to ./minifetch
#   sudo ./install.sh install # build + install to /usr/local/bin/minifetch

cc=${CC:-gcc}
cflags="-O2 -pipe -s -Wall -Wextra"
out="minifetch"

echo "[*] Building $out"
$cc $cflags -o "$out" minifetch.c

if [[ "${1:-}" == "install" ]]; then
  echo "[*] Installing to /usr/local/bin/minifetch"
  install -m 0755 "$out" /usr/local/bin/minifetch
  echo "[+] Done. Try: minifetch"
else
  echo "[+] Built ./minifetch"
fi

