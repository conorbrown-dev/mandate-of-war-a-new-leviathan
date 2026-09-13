#!/usr/bin/env bash
set -euo pipefail

demo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
godot_bin=${GODOT_BIN:-"$demo_root/Godot_v4.7.2-stable_linux.x86_64"}

if [[ ! -x "$godot_bin" ]]; then
  echo "Godot 4.7.2 was not found at: $godot_bin" >&2
  echo "Set GODOT_BIN to your Godot executable and run this script again." >&2
  exit 1
fi

echo "Killing existing Godot instances..." >&2
pkill -f "Godot.*--path $demo_root/godot/project" 2>/dev/null || true
sleep 1

cmake --build "$demo_root/build"
exec env RTS_AUTO_START_SKIRMISH=1 RTS_PROTOTYPE_VISUALS=1 "$godot_bin" --path "$demo_root/godot/project"
