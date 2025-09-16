#!/usr/bin/env sh
# install.sh — build (and optionally install) minifetch
set -euo pipefail

# Always refresh the embedded frames before compile
./embed_logo.sh

CFLAGS="-O2 -pipe -s -Wall -Wextra -Wpedantic -Wno-unused-parameter -std=c99"
cc ${CFLAGS} -o minifetch minifetch.c

if [ "${1-}" = "install" ]; then
  dst="/usr/local/bin/minifetch"
  echo "Installing to ${dst} (requires sudo if not root)..."
  install -m 0755 minifetch "${dst}"
  echo "Installed ${dst}"
else
  echo "Built ./minifetch"
fi

