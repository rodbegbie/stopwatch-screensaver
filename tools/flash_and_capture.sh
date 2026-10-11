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
pio_status=$?
unset PLATFORMIO_BUILD_FLAGS
verified=$(grep -c 'Hash of data verified' "$out/flash_$hack.txt")
echo "flash verified: $verified" "$(grep -E 'Flash:' "$out/flash_$hack.txt")"
grep -E "error:|overflowed" "$out/flash_$hack.txt" | head -3
# A failed flash leaves the old firmware running, and the capture would measure
# that: stop rather than report numbers for the wrong build.
if [ "$pio_status" -ne 0 ] || [ "$verified" -eq 0 ]; then
  echo "flash failed (pio exit $pio_status, $verified verified); see $out/flash_$hack.txt" >&2
  exit 1
fi
echo "note: the board now starts on $hack with rotation off; flash a normal build when done"
"$repo/.venv/bin/python" -I "$repo/tools/serial_capture.py" "$out/$hack.log" "$seconds"
grep -a "^run" "$out/$hack.log" | tail -"$lines" | cut -c1-125
