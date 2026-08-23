#!/usr/bin/env bash
set -euo pipefail

root=${1:?usage: verify-linux-architecture.sh ROOT EXPECTED-ELF-MACHINE}
expected=${2:?usage: verify-linux-architecture.sh ROOT EXPECTED-ELF-MACHINE}
failures=0
elf_count=0

while IFS= read -r -d '' candidate; do
  if file -b "$candidate" | grep -q '^ELF '; then
    elf_count=$((elf_count + 1))
    machine=$(readelf -h "$candidate" | sed -n 's/^ *Machine: *//p')
    printf '%s: %s\n' "${candidate#"$root"/}" "$machine"
    if [[ "$machine" != "$expected" ]]; then
      printf 'Unexpected ELF machine for %s: expected %s, found %s\n' \
        "$candidate" "$expected" "$machine" >&2
      failures=$((failures + 1))
    fi
  fi
done < <(find "$root" -type f -print0)

if (( elf_count == 0 )); then
  echo "No ELF files found below $root" >&2
  exit 1
fi
if (( failures != 0 )); then
  exit 1
fi
printf 'Verified %d ELF files as %s.\n' "$elf_count" "$expected"

