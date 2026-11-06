#!/bin/bash

set -euo pipefail

before=$(ls backups/executables)
./recompile.sh --jobs 4
after=$(ls backups/executables)
mapfile -t new_binaries < <(comm -13 <(printf '%s\n' "$before" | sort) <(printf '%s\n' "$after" | sort) | grep '\.bin$')
[[ ${#new_binaries[@]} -eq 1 ]] || { printf 'Expected one new executable backup, found %d.\n' "${#new_binaries[@]}" >&2; exit 1; }
cp -- "backups/executables/${new_binaries[0]}" "$HOME/server/bin"
