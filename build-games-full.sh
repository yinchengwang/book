#!/bin/bash
set -e
EMSDK_DIR="D:/code/book/emsdk"
PROJECT_ROOT="D:/code/book"
echo "=== Step 1: Install Emscripten ==="
cd "$EMSDK_DIR"
if [ ! -d "$EMSDK_DIR/upstream/emscripten" ]; then
    echo "Installing Emscripten (may take a few minutes)..."
    python emsdk.py install latest
    python emsdk.py activate latest
else
    echo "Emscripten already installed"
fi
echo "=== Step 2: Activate ==="
source "$EMSDK_DIR/emsdk_env.sh"
echo "=== Step 3: Build games WASM ==="
bash "$PROJECT_ROOT/engineering/scripts/build-games-wasm.sh"
echo "=== Step 4: Verify ==="
ls -la "$PROJECT_ROOT/engineering/apps/games/web/games.js" && echo "OK" || echo "FAIL"
echo "=== Done ==="
