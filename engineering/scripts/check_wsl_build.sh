#!/usr/bin/env bash
# T12/WS-F DoD-5 WSL Linux build verification script.
#
# Usage (from the host, in Git Bash):
#   ! wsl -e bash -lc "bash /mnt/d/code/book/engineering/scripts/check_wsl_build.sh"
#
# What it does:
#   1. cmake -B build-linux -S . -DBUILD_TESTING=ON
#   2. cmake --build build-linux --parallel 4
#   3. ctest --test-dir build-linux --timeout 60
#
# Pre-requisites in WSL:
#   - sudo apt-get install -y cmake build-essential
#
# Note: this script is meant to be run inside WSL; it cd's to the engineering
# tree under /mnt/d. If your repo lives elsewhere, edit ENGINEERING_DIR below.

set -euo pipefail

ENGINEERING_DIR="${ENGINEERING_DIR:-/mnt/d/code/book/engineering}"

if [ ! -d "$ENGINEERING_DIR" ]; then
    echo "ERROR: engineering directory not found at $ENGINEERING_DIR" >&2
    echo "       set ENGINEERING_DIR=/path/to/engineering and re-run" >&2
    exit 1
fi

cd "$ENGINEERING_DIR"

echo "=== [1/3] cmake configure (BUILD_TESTING=ON) ==="
cmake -B build-linux -S . -DBUILD_TESTING=ON 2>&1 | tail -3

echo ""
echo "=== [2/3] cmake --build build-linux --parallel 4 ==="
cmake --build build-linux --parallel 4 2>&1 | tail -5

echo ""
echo "=== [3/3] ctest --test-dir build-linux --timeout 60 ==="
ctest --test-dir build-linux --timeout 60 2>&1 | tail -5

echo ""
echo "WSL-DOD5-OK"
