#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
: "${ARDUINOJSON_DIR:?Set ARDUINOJSON_DIR to the ArduinoJson 7.x directory containing src/ArduinoJson.h}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 \
  -I"$here/stubs" -I"$ARDUINOJSON_DIR/src" \
  "$here/host-tests.cpp" -o "$build/host-tests"
"$build/host-tests"
bash "$here/run-background-tests.sh"
