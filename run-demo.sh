#!/usr/bin/env bash
set -euo pipefail

DEMO="${1:-demo_video.lua}"
CONFIG="Debug"
BUILD_DIR="build/windows-debug/${CONFIG}"
EXE="${BUILD_DIR}/bbfx.exe"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
DEMO_PATH="$REPO_ROOT/lua/demos/$DEMO"
EXE_PATH="$REPO_ROOT/$EXE"

if [[ ! -f "$EXE_PATH" ]]; then
  echo "Executable not found: $EXE_PATH" >&2
  exit 1
fi

if [[ ! -f "$DEMO_PATH" ]]; then
  echo "Demo script not found: $DEMO_PATH" >&2
  exit 1
fi

cd "$REPO_ROOT"
"$EXE_PATH" "$DEMO_PATH"
