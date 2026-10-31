#!/bin/sh
# Runs every desktop test. Uses the esphome on your PATH unless ESPHOME points at another one.
set -e
cd "$(dirname "$0")"

# Some macOS Command Line Tools installs keep a stale C++ folder that hides the SDK's headers.
STDLIB_FLAGS=""
if command -v xcrun >/dev/null 2>&1; then
  SDK="$(xcrun --sdk macosx --show-sdk-path)"
  if [ -f "$SDK/usr/include/c++/v1/cassert" ]; then
    STDLIB_FLAGS="-nostdinc++ -isystem $SDK/usr/include/c++/v1"
    export CPLUS_INCLUDE_PATH="$SDK/usr/include/c++/v1"
  fi
fi

OUT="$(mktemp -d)"
# shellcheck disable=SC2086
c++ -std=c++17 -Wall -Wextra $STDLIB_FLAGS -o "$OUT/test_pixel_math" test_pixel_math.cpp
"$OUT/test_pixel_math"
"${ESPHOME:-esphome}" compile host/scenario.yaml
host/.esphome/build/dual-ic-scenario/.pioenvs/dual-ic-scenario/program > "$OUT/scenario.txt"
python3 check_scenario.py "$OUT/scenario.txt"
