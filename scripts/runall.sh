#!/usr/bin/env sh
# Compatibility launcher for the Python dcc regression runner.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$SCRIPT_DIR/runall.py" "$@"
