#!/bin/bash
set -e

cd /home/conor/repos/mandate-of-war

# Ensure we have C++ compiler
g++ --version

# Compile GDExtension wrapper
g++ -std=c++20 -fPIC -shared \
    -Ivendor/godot-cpp/include \
    -Ivendor/godot-cpp/gen/include \
    -I/home/conor/repos/mandate-of-war/src \
    -I/home/conor/repos/mandate-of-war/vendor/glm \
    gdextension/gd_extension.cpp \
    -o godot/lib/librts_gdextension.so \
    -Lbuild -lrts_simulation -lstdc++
