"""Ranks the hacks in a serial log of one rotation lap by where their time goes.

Flash a build with `-DROTATE_SECONDS=20`, log the serial port for the whole lap
(about 20 seconds a hack), then:

    uv run tools/triage_hacks.py lap.log
    uv run tools/triage_hacks.py lap.log --soft    # add soft-double call counts

A frame is `step` (the hack), `push` (the display) and `wait` (what is left of
the delay the hack asked for after the push is credited). Each hack is put in
the first class that fits:

- paced: `wait` is the biggest part, so the hack asked for its rate. A faster
  step helps by exactly what it saves, but the frame cannot get under the
  delay (Forest 500 ms, Vines 200 ms).
- push-bound: `push` is the biggest part. Only fewer rows or a smaller push
  help.
- step-bound: `step` is the biggest part. These are the ones to look into.

`--soft` reads the device build's objects (`pio run -e stopwatch` first) for
the soft-double calls in the hack's own object, so a step-bound hack with a
count is a wrapper candidate. A hack the table calls step-bound with no calls
is in its drawing, not its maths.
"""

import argparse
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

RUN = re.compile(r"^run (?P<hack>\S+) (?P<fields>.*)$")
FIELD = re.compile(r"(\w+)=([-\d.]+)")


@dataclass
class Row:
    hack: str
    windows: int
    fps: float
    worst_fps: float
    step: float
    push: float
    wait: float
    kind: str


def parse(text: str) -> dict[str, list[dict[str, float]]]:
    """The `run` windows for each hack, in order."""
    out: dict[str, list[dict[str, float]]] = defaultdict(list)
    for raw in text.splitlines():
        if m := RUN.match(raw.strip()):
            fields = {k: float(v) for k, v in FIELD.findall(m.group("fields"))}
            out[m.group("hack")].append(fields)
    return out


def classify(step: float, push: float, wait: float) -> str:
    if wait > step and wait > push:
        return "paced"
    if push > step:
        return "push-bound"
    return "step-bound"


def _mean(windows: list[dict[str, float]], key: str) -> float:
    return sum(w.get(key, 0.0) for w in windows) / len(windows)


def summarise(runs: dict[str, list[dict[str, float]]], skip: int = 1) -> list[Row]:
    """One row a hack. The first window holds the boot and the name overlay, so
    it is left out when the hack has more than one."""
    rows = []
    for hack, windows in runs.items():
        kept = windows[skip:] or windows

        step, push, wait = (_mean(kept, k) for k in ("step", "push", "wait"))
        rows.append(
            Row(
                hack,
                len(kept),
                _mean(kept, "fps"),
                min(w.get("fps", 0.0) for w in kept),
                step,
                push,
                wait,
                classify(step, push, wait),
            )
        )
    return rows


def soft_counts() -> dict[str, int]:
    """Soft-double calls per object stem, from the device build."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import softdouble_report as sd

    if not sd.BUILD.is_dir():
        raise SystemExit(f"no device build at {sd.BUILD}: run pio run -e stopwatch")
    counts: dict[str, int] = {}
    for obj in sd.BUILD.rglob("*.o"):
        if "hacks/" not in str(obj):
            continue
        found = sd.soft_calls(obj)
        counts[obj.name.split(".")[0]] = sum(sum(c.values()) for c in found.values())
    return counts


def soft_for(hack: str, counts: dict[str, int]) -> int:
    """The calls in objects whose name starts with the hack's, in any wrapper."""
    key = hack.lower()
    return sum(n for stem, n in counts.items() if stem.startswith(key))


def render(rows: list[Row], counts: dict[str, int] | None) -> str:
    order = {"step-bound": 0, "push-bound": 1, "paced": 2}
    rows = sorted(rows, key=lambda r: (order[r.kind], -r.step))
    head = "| Hack | Class | fps | worst | step | push | wait |"
    rule = "| --- | --- | --- | --- | --- | --- | --- |"
    if counts is not None:
        head += " soft |"
        rule += " --- |"
    lines = [head, rule]
    for r in rows:
        line = (
            f"| {r.hack} | {r.kind} | {r.fps:.1f} | {r.worst_fps:.1f} | "
            f"{r.step:.1f} ms | {r.push:.1f} ms | {r.wait:.1f} ms |"
        )
        if counts is not None:
            line += f" {soft_for(r.hack, counts)} |"
        lines.append(line)
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("log", type=Path)
    ap.add_argument("--soft", action="store_true", help="add soft-double counts")
    args = ap.parse_args(argv)
    rows = summarise(parse(args.log.read_text(errors="replace")))
    sys.stdout.write(render(rows, soft_counts() if args.soft else None))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
