#!/usr/bin/env bash
set -euo pipefail
app="$(cd "$(dirname "$0")/.." && pwd)"
temp="$(mktemp -d)"
trap 'rm -rf "$temp"' EXIT
kotlinc "$app/modules/beeline-android/android/src/main/java/expo/modules/beeline/RouteSummary.kt" \
  "$app/tests/RouteSummaryChecks.kt" -include-runtime -d "$temp/route-checks.jar"
java -jar "$temp/route-checks.jar"
