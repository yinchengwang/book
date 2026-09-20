#!/usr/bin/env bash
# Linux/macOS launcher for multimodal_rag server.
# Equivalent to start_server.bat on Windows.
#
# Usage:
#   ./start_server.sh                 # use default config
#   ./start_server.sh --port 18080    # custom port
#   ./start_server.sh --config PATH   # custom config
#
# Environment variables (all optional):
#   RAG_PROJECT_ROOT    path to engineering project root (auto-detected)
#   RAG_BUILD_DIR       path to the build directory containing bin/ (auto-detected)
#   RAG_DATA_DIR        override data directory (passed to server)
#
# Auto-detects the build directory by walking up from this script's location.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Locate build directory ---
# Convention: $RAG_BUILD_DIR/bin/multimodal_rag_server OR a sibling build/bin/.
if [[ -z "${RAG_BUILD_DIR:-}" ]]; then
    # Try a few common locations.
    CANDIDATES=(
        "$SCRIPT_DIR/../../../build/bin/multimodal_rag_server"
        "$SCRIPT_DIR/../../build/bin/multimodal_rag_server"
        "$SCRIPT_DIR/../build/bin/multimodal_rag_server"
        "$(pwd)/build/bin/multimodal_rag_server"
    )
    FOUND=""
    for cand in "${CANDIDATES[@]}"; do
        if [[ -x "$cand" ]]; then
            FOUND="$(dirname "$(dirname "$cand")")"
            break
        fi
    done
    if [[ -z "$FOUND" ]]; then
        echo "ERROR: cannot find build/bin/multimodal_rag_server." >&2
        echo "       Set RAG_BUILD_DIR or run cmake --build first." >&2
        exit 1
    fi
    RAG_BUILD_DIR="$FOUND"
fi

SERVER_BIN="$RAG_BUILD_DIR/bin/multimodal_rag_server"

if [[ ! -x "$SERVER_BIN" ]]; then
    echo "ERROR: server binary not found or not executable: $SERVER_BIN" >&2
    exit 1
fi

# --- Default config ---
DEFAULT_CONFIG="$SCRIPT_DIR/config/default.yaml"
if [[ ! -f "$DEFAULT_CONFIG" ]]; then
    echo "WARN: default config not found at $DEFAULT_CONFIG" >&2
    DEFAULT_CONFIG=""
fi

# --- Forward args ---
echo "Starting multimodal_rag_server:"
echo "  binary: $SERVER_BIN"
echo "  config: ${DEFAULT_CONFIG:-<none>}"
echo "  args:   $*"

cd "$SCRIPT_DIR"
exec "$SERVER_BIN" ${DEFAULT_CONFIG:+--config "$DEFAULT_CONFIG"} "$@"