#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
firmware="$(cd "$here/.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$here/stubs" "$here/background-tests.cpp" -o "$build/background-tests"
"$build/background-tests"
echo "PASS: RGB565 wallpaper rendering, clipping, and direct/normal/swapped color diagnostic"
