#!/bin/bash
# Build dcc (C compiler) and dccpeep (peephole optimizer) using gcc.
# Equivalent of m.bat on Linux/macOS.

set -e

echo "Building dcc..."
gcc -O2 -o dcc dcc.c -static
# cp dcc /mnt/c/users/david/onedrive/ntvcm/dcc

echo "Building dccpeep..."
gcc -O2 -o dccpeep dccpeep.c -static
# cp dccpeep /mnt/c/users/david/onedrive/ntvcm/dcc

echo "Building dccrtlstrip..."
gcc -O2 -o dccrtlstrip dccrtlstrip.c -static
# cp dccrtlstrip /mnt/c/users/david/onedrive/ntvcm/dcc

echo "Done."
