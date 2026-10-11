#!/bin/bash
# Flashes a build that starts on one hack with rotation off, then reads the
# serial log. Needs the board, and Rod's say-so in the session (AGENTS.md:
# never flash without asking, unless he has said it is allowed).
#
#   bash tools/flash_and_capture.sh <hack> <seconds> [extra build flags] [lines]
#
# <hack> is a name from g_hacks[] in any case; extra flags are things like
# "-DFIREWORKX_SCALE=2"; the last `run` lines (default 6) are printed. The full
# log and the flash output go to $OUT_DIR (default: ./flash-capture, which
# should be somewhere git ignores, such as your scratchpad).
set -u
[ $# -ge 2 ] || { sed -n '2,12p' "$0"; exit 2; }
hack=$1 seconds=$2 extra=${3:-} lines=${4:-6}
repo=$(cd "$(dirname "$0")/.." && pwd)
out=${OUT_DIR:-./flash-capture}
mkdir -p "$out"
out=$(cd "$out" && pwd)

source "$repo/tools/env.sh"
cd "$repo/firmware" || exit 1
PLATFORMIO_BUILD_FLAGS="-DSTART_HACK=\\\"$hack\\\" -DROTATE_SECONDS=0 $extra" \
  NO_COLOR=1 pio run -e stopwatch -t upload > "$out/flash_$hack.txt" 2>&1
echo "flash verified: $(grep -c 'Hash of data verified' "$out/flash_$hack.txt")" \
     "$(grep -E 'Flash:' "$out/flash_$hack.txt")"
grep -E "error:|overflowed" "$out/flash_$hack.txt" | head -3
unset PLATFORMIO_BUILD_FLAGS
"$repo/.venv/bin/python" -I "$repo/tools/serial_capture.py" "$out/$hack.log" "$seconds"
grep -a "^run" "$out/$hack.log" | tail -"$lines" | cut -c1-125
