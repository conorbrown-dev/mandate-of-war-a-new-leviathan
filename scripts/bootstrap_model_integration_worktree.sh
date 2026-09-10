#!/usr/bin/env bash
set -euo pipefail

# Bootstrap the ignored native dependencies for a model-integration worktree.
# The source checkout is explicit so this script never guesses or overwrites
# user-owned directories.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
vendor_source="${NEAR_FUTURE_RTS_VENDOR_SOURCE:-}"
descriptor_source="${NEAR_FUTURE_RTS_DESCRIPTOR_SOURCE:-}"

if [[ -z "${vendor_source}" || -z "${descriptor_source}" ]]; then
  echo "Set NEAR_FUTURE_RTS_VENDOR_SOURCE and NEAR_FUTURE_RTS_DESCRIPTOR_SOURCE." >&2
  echo "Example: point them at a checkout containing vendor/ and godot/project/rts.gdextension." >&2
  exit 2
fi
if [[ ! -d "${vendor_source}" ]]; then
  echo "Vendor source does not exist: ${vendor_source}" >&2
  exit 2
fi
if [[ ! -f "${descriptor_source}" ]]; then
  echo "GDExtension descriptor does not exist: ${descriptor_source}" >&2
  exit 2
fi

link_if_missing() {
  local target="$1"
  local link_path="$2"
  if [[ -e "${link_path}" || -L "${link_path}" ]]; then
    if [[ "$(readlink -f "${link_path}")" != "$(readlink -f "${target}")" ]]; then
      echo "Refusing to replace existing path: ${link_path}" >&2
      exit 3
    fi
    return
  fi
  ln -s "${target}" "${link_path}"
}

link_if_missing "${vendor_source}" "${repo_root}/vendor"
link_if_missing "${descriptor_source}" "${repo_root}/godot/project/rts.gdextension"
echo "Model-integration dependencies linked in ${repo_root}"
