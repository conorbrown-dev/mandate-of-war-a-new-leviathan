#!/bin/bash
set -e

GODOT_SDK="/home/conor/repos/near-future-rts-game/vendor/Godot_v4.3-stable_linux.x86_64"
GODOT_INCLUDE="$GODOT_SDK/godot_headers"

cd /home/conor/repos/near-future-rts-game

# Ensure we have C++ compiler
g++ --version

# Compile GDExtension wrapper
g++ -std=c++17 -fPIC -shared \
    -I"$GODOT_INCLUDE" \
    -Isrc/ecs \
    -Isrc/spatial \
    -Isrc/simulation \
    gdextension/gd_extension.cpp \
    -o godot/lib/librts_gdextension.so \
    -Lbuild -lrts_simulation
