#!/bin/bash

set -ex

clang MouseCircle_standalone.c -o MouseCircle_standalone \
    -I$CONDA_PREFIX/include \
    -L$CONDA_PREFIX/lib \
    -lSDL3 \
    -Wl,-rpath,$CONDA_PREFIX/lib \
    -framework Metal -framework CoreGraphics -framework AppKit

./MouseCircle_standalone
