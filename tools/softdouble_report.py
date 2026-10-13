"""Lists the software-double calls in the device build's objects, by function.

The ESP32-S3's FPU is single-precision only, so `double` arithmetic and libm
calls become library calls (`__muldf3`, `sqrt`, ...) that cost tens to hundreds
of cycles. A wrapper that still shows them in a per-frame function has a
literal or a macro the single-precision header could not reach (`frand`,
`MAXRAND`, `M_PI`). This reads what the compiler produced, so it needs a device
build: `pio run -e stopwatch` from `firmware/`.

    uv run tools/softdouble_report.py              # every hack object
    uv run tools/softdouble_report.py galaxy_single x11shim/stroke
"""

import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OBJDUMP = next(
    (ROOT / ".platformio/packages/toolchain-xtensa-esp32s3/bin").glob(
        "xtensa-esp32s3-elf-objdump"
    ),
    None,
)
BUILD = ROOT / "firmware/.pio/build/stopwatch/src"

SOFT = re.compile(
    r"R_XTENSA_\w+\s+(__\w*df\w*|sqrt|floor|ceil|sin|cos|pow|atan2?|log|exp)\s*$"
)
FUNCTION = re.compile(r"^[0-9a-f]+ <(.+)>:")


def soft_calls(obj: Path) -> dict[str, Counter]:
    """Soft-double references per function in one object file."""
    assert OBJDUMP is not None, "toolchain not found: link .platformio into the repo"
    listing = subprocess.run(
        [str(OBJDUMP), "-dr", "--no-show-raw-insn", str(obj)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    found: dict[str, Counter] = defaultdict(Counter)
    function = None
    for line in listing.splitlines():
        if m := FUNCTION.match(line):
            function = m.group(1)
        elif (
            function
            and not function.startswith(".literal")
            and (m := SOFT.search(line))
        ):
            found[function][m.group(1)] += 1
    return found


def main(argv: list[str]) -> int:
    if not BUILD.is_dir():
        raise SystemExit(f"no device build at {BUILD}: run pio run -e stopwatch")
    wanted = argv or [""]
    objects = sorted(
        p
        for p in BUILD.rglob("*.o")
        if any(w in str(p.relative_to(BUILD)) for w in wanted)
        and (argv or "hacks/" in str(p))
    )
    for obj in objects:
        found = soft_calls(obj)
        total = sum(sum(c.values()) for c in found.values())
        print(f"{obj.relative_to(BUILD)}: {total}")
        for function, calls in sorted(
            found.items(), key=lambda kv: -sum(kv[1].values())
        ):
            print(f"    {function}: {dict(calls)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
