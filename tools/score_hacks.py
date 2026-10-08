"""Score every xscreensaver hack for porting effort against the X11 shim.

Run with: uv run tools/score_hacks.py [--vendor DIR] [--out FILE]
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

X_CALL = re.compile(r"\b(X[A-Z][A-Za-z0-9]*)\s*\(")
GL_IDENT = re.compile(r"\b(glBegin|glVertex\w*|glx\w+|GLX\w+)\b")
NOISE = re.compile(
    r"""/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'""", re.DOTALL
)
MATH_CALL = re.compile(r"\b(sin|cos|sqrt|pow)\s*\(")

FLAG_CALLS = {
    "pixmaps": {"XCreatePixmap", "XCopyArea"},
    "readback": {"XGetImage", "XGetPixel"},
    "xor": {"XSetFunction"},
    "text": {"XDrawString", "XLoadFont", "XLoadQueryFont"},
    "clipmask": {"XSetClipMask"},
}
FLOAT_HEAVY_THRESHOLD = 20
MAX_MEDIUM_MISSING = 4


def strip_noise(source: str) -> str:
    return NOISE.sub(lambda m: "\n" * m.group(0).count("\n") or " ", source)


def implemented_calls(header: str) -> set[str]:
    return set(X_CALL.findall(strip_noise(header)))


def used_calls(source: str) -> set[str]:
    return set(X_CALL.findall(strip_noise(source)))


def classify(path: Path, source: str) -> str:
    if "glx" in path.parts or GL_IDENT.search(strip_noise(source)):
        return "gl"
    return "2d"


def score(path: Path, source: str, provided: set[str]) -> dict:
    clean = strip_noise(source)
    used = used_calls(source)
    missing = sorted(used - provided)
    kind = classify(path, source)
    flags = [name for name, calls in FLAG_CALLS.items() if used & calls]
    if len(MATH_CALL.findall(clean)) >= FLOAT_HEAVY_THRESHOLD:
        flags.append("float-heavy")
    if re.search(r'#\s*include\s*"xlockmore\.h"', source):
        flags.append("needs-xlockmore")
    if kind == "gl":
        effort = "XL"
    elif not missing:
        effort = "S"
    elif len(missing) > MAX_MEDIUM_MISSING or {"pixmaps", "readback"} & set(flags):
        effort = "L"
    else:
        effort = "M"
    return {
        "name": path.stem,
        "kind": kind,
        "effort": effort,
        "missing": missing,
        "flags": flags,
        "loc": source.count("\n") + 1,
    }


def rank_missing(rows: list[dict], n: int) -> list[tuple[str, float, int]]:
    """Missing calls ranked for 2D hacks. A hack missing k calls gives each
    1/k, so calls blocking nearly-ready hacks rank first."""
    weight: dict[str, float] = defaultdict(float)
    hacks: dict[str, int] = defaultdict(int)
    for row in rows:
        if row["kind"] != "2d" or not row["missing"]:
            continue
        for call in row["missing"]:
            weight[call] += 1 / len(row["missing"])
            hacks[call] += 1
    ranked = sorted(weight, key=lambda c: (-weight[c], c))
    return [(c, round(weight[c], 2), hacks[c]) for c in ranked[:n]]


def render_table(rows: list[dict]) -> str:
    lines = [
        "| Hack | Kind | Effort | Missing X calls | Flags | LOC |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for r in sorted(rows, key=lambda r: (r["effort"], r["name"])):
        missing = ", ".join(r["missing"]) or "-"
        flags = ", ".join(r["flags"]) or "-"
        lines.append(
            f"| {r['name']} | {r['kind']} | {r['effort']} | {missing} | {flags} | {r['loc']} |"
        )
    return "\n".join(lines) + "\n"


def render_ranking(ranked: list[tuple[str, float, int]]) -> str:
    lines = ["| Call | Score | 2D hacks needing it |", "| --- | --- | --- |"]
    lines += [f"| {c} | {s} | {h} |" for c, s, h in ranked]
    return "\n".join(lines) + "\n"


def scan(vendor: Path, provided: set[str]) -> list[dict]:
    files = sorted((vendor / "hacks").glob("*.c")) + sorted(
        (vendor / "hacks" / "glx").glob("*.c")
    )
    return [score(p, p.read_text(errors="replace"), provided) for p in files]


def main(argv: list[str]) -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vendor", default=str(next((root / "vendor").glob("xscreensaver-*"))))
    parser.add_argument("--header", default=str(root / "firmware/src/x11shim/xshim.h"))
    parser.add_argument("--intro", default=str(root / "tools/assessment_intro.md"))
    parser.add_argument("--measured", default=str(root / "tools/assessment_measured.md"))
    parser.add_argument("--out", default=str(root / "docs/porting-assessment.md"))
    args = parser.parse_args(argv)

    vendor = Path(args.vendor)
    version = vendor.name.removeprefix("xscreensaver-")
    rows = scan(vendor, implemented_calls(Path(args.header).read_text()))
    counts = {e: sum(r["effort"] == e for r in rows) for e in ("S", "M", "L", "XL")}
    intro = Path(args.intro).read_text().format(
        version=version, total=len(rows), **{f"n_{k.lower()}": v for k, v in counts.items()}
    )
    measured = Path(args.measured).read_text() if Path(args.measured).exists() else ""
    out = (
        intro
        + "\n"
        + measured
        + "\n## Suggested order for shim stage 2\n\n"
        + "Missing calls across 2D hacks, ranked so calls that block\n"
        + "hacks needing few additions come first.\n\n"
        + render_ranking(rank_missing(rows, 10))
        + "\n## All hacks\n\n"
        + render_table(rows)
    )
    Path(args.out).write_text(out)
    print(f"wrote {args.out}: {len(rows)} hacks, {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
