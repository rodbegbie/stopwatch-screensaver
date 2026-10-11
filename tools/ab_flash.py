"""Flash builds of the firmware in turn and compare their serial logs.

    .venv/bin/python -I tools/ab_flash.py --hack CubicGrid --seconds 60 \\
        --rounds 2 --out /tmp/ab base= iram=-DGL_IRAM "prof=-DGL_PROFILE -DGL_IRAM"

Each `name=flags` is a variant. Every variant is built once, pinned to the
hack with rotation off (`START_HACK`, `ROTATE_SECONDS=0`), and its image kept.
Then, round by round, each image is flashed and the serial log captured for
`--seconds`. A boot is deterministic, so alternating builds and comparing
window by window is how this repo measures a change (AGENTS.md).

This flashes the device: run it only with Rod's go-ahead. It never touches
the partition table or `ffat`; `pio run -t upload` writes what a normal
upload writes.
"""

import argparse
import os
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIRMWARE = ROOT / "firmware"

RUN = re.compile(r"^run (?P<hack>\S+) (?P<fields>.*)$")
GLPROF = re.compile(r"^glprof (?P<hack>\S+) (?P<fields>.*)$")
FIELD = re.compile(r"(\w+)=([-\d.]+)")


def parse_fields(text: str) -> dict[str, float]:
    """`fps=12.3 step=45.6ms rows=466` to {"fps": 12.3, "step": 45.6, ...}."""
    return {k: float(v) for k, v in FIELD.findall(text)}


@dataclass
class Capture:
    runs: list[dict[str, float]] = field(default_factory=list)
    glprof: list[dict[str, float]] = field(default_factory=list)


def parse_log(text: str, hack: str) -> Capture:
    """The `run` and `glprof` windows a log has for `hack` (any case)."""
    out = Capture()
    for raw in text.splitlines():
        line = raw.strip()
        for pattern, bucket in ((RUN, out.runs), (GLPROF, out.glprof)):
            m = pattern.match(line)
            if m and m.group("hack").lower() == hack.lower():
                bucket.append(parse_fields(m.group("fields")))
    return out


def summarise(windows: list[dict[str, float]], skip: int = 1) -> dict[str, float]:
    """Mean of each field, leaving out the first `skip` windows: the first
    holds the boot and, for push, the name overlay (AGENTS.md)."""
    kept = windows[skip:]
    if not kept:
        return {}
    keys = sorted({k for w in kept for k in w})
    return {k: statistics.fmean(w[k] for w in kept if k in w) for k in keys}


def render(results: dict[str, list[Capture]], keys: list[str]) -> str:
    lines = [
        "| Variant | Round | Windows | " + " | ".join(keys) + " |",
        "| --- | --- | --- | " + " | ".join("---" for _ in keys) + " |",
    ]
    for name, captures in results.items():
        for i, cap in enumerate(captures, 1):
            merged = summarise(cap.runs)
            merged.update(summarise(cap.glprof))
            cells = [f"{merged[k]:.2f}" if k in merged else "-" for k in keys]
            lines.append(
                f"| {name} | {i} | {max(len(cap.runs) - 1, 0)} | "
                + " | ".join(cells)
                + " |"
            )
    return "\n".join(lines) + "\n"


def pio(*args: str, flags: str, build_dir: Path) -> subprocess.CompletedProcess:
    """`pio run` for the stopwatch. Each variant has its own `build_dir`:
    PlatformIO cleans a build directory, image and all, when the flags it
    was built with change, so variants sharing one wipe each other."""
    env = {
        **os.environ,
        "PLATFORMIO_CORE_DIR": str(ROOT / ".platformio"),
        "PLATFORMIO_BUILD_DIR": str(build_dir),
        "NO_COLOR": "1",
        "PLATFORMIO_BUILD_FLAGS": flags,
    }
    return subprocess.run(
        [
            str(ROOT / ".venv" / "bin" / "pio"),
            "run",
            "-d",
            str(FIRMWARE),
            "-e",
            "stopwatch",
            *args,
        ],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


def pinned(hack: str, flags: str) -> str:
    return f'-DSTART_HACK=\\"{hack}\\" -DROTATE_SECONDS=0 {flags}'.strip()


def build(name: str, flags: str, out: Path) -> None:
    result = pio(flags=flags, build_dir=out / f"pio-{name}")
    (out / f"build-{name}.log").write_text(result.stdout + result.stderr)
    if result.returncode != 0:
        sys.exit(f"build of {name} failed; see {out / f'build-{name}.log'}")


def flash(name: str, flags: str, out: Path, log: Path) -> None:
    """Upload the image `build` made, without rebuilding."""
    build_dir = out / f"pio-{name}"
    result = pio("-t", "nobuild", "-t", "upload", flags=flags, build_dir=build_dir)
    log.write_text(result.stdout + result.stderr)
    if result.returncode != 0 or "Hash of data verified" not in result.stdout:
        sys.exit(f"flash of {name} did not verify; see {log}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--hack", required=True, help="a name from g_hacks[]")
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--keys", default="fps,step,push,rows")
    parser.add_argument("variants", nargs="+", help="name=flags")
    args = parser.parse_args(argv)
    sys.path.insert(0, str(ROOT / "tools"))
    import serial_capture

    args.out.mkdir(parents=True, exist_ok=True)
    variants = {
        name: pinned(args.hack, flags)
        for name, flags in (v.split("=", 1) for v in args.variants)
    }
    for name, flags in variants.items():
        build(name, flags, args.out)
    results: dict[str, list[Capture]] = {name: [] for name in variants}
    for r in range(1, args.rounds + 1):
        for name, flags in variants.items():
            flash(name, flags, args.out, args.out / f"flash-{r}-{name}.log")
            log = args.out / f"serial-{r}-{name}.log"
            serial_capture.capture(str(log), args.seconds)
            results[name].append(parse_log(log.read_text(errors="replace"), args.hack))
    table = render(results, args.keys.split(","))
    (args.out / "summary.md").write_text(table)
    print(table, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
