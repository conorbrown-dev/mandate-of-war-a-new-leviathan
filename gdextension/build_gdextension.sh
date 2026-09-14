#!/usr/bin/env sh
# Compatibility entry point. The root CMake build owns generated bindings,
# pinned dependencies, and the copy into godot/project/bin.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" --target rts_gdextension
