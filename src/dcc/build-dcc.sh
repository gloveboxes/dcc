#!/bin/sh
#
# build-dcc.sh - build the modularised dcc compiler (separate compilation).
#
# Compiles each module translation unit (dcc.c and the dcc_*.c files, all of
# which include the umbrella header dcc.h) and links them into the `dcc`
# executable at the repo root, matching the flags used for the monolithic
# build so the produced binary and its generated assembly are equivalent.
#
# Usage (from anywhere):
#   sh src/dcc/build-dcc.sh            # build ./dcc at the repo root
#   sh src/dcc/build-dcc.sh -o out/dcc # build to a custom path
#
# The companion peephole/runtime tools (dccpeep, dccrtlstrip) still live at the
# repo root and are built by the existing mmacos.sh / m.sh scripts; this script
# only builds the modular dcc front end.

set -e

# Resolve the directory this script lives in, so it works from any CWD.
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)

if [ -z "$CC" ]; then
    case "$(uname)" in
        Darwin) CC=clang ;;
        *)      CC=gcc ;;
    esac
fi
CFLAGS=${CFLAGS:--std=c89 -Wall -Wextra -O2}
OUT="$REPO_ROOT/dcc"

# Allow "-o <path>" to override the output location.
if [ "$1" = "-o" ] && [ -n "$2" ]; then
    OUT="$2"
fi

echo "Building modular dcc -> $OUT"
# All .c files in this directory are module translation units linked together.
( cd "$SCRIPT_DIR" && $CC $CFLAGS -I . -o "$OUT" ./*.c )
echo "Done."
