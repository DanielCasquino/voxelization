#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$ROOT/stanford-bunny"
TMP_DIR="$OUT_DIR/tmp"
URL="https://graphics.stanford.edu/pub/3Dscanrep/bunny.tar.gz"

mkdir -p "$TMP_DIR"
curl -L "$URL" -o "$TMP_DIR/bunny.tar.gz"
tar -xzf "$TMP_DIR/bunny.tar.gz" -C "$TMP_DIR"
find "$TMP_DIR" -name 'bun_zipper.ply' -exec cp {} "$OUT_DIR/bun_zipper.ply" \;
rm -rf "$TMP_DIR"

echo "$OUT_DIR/bun_zipper.ply"
