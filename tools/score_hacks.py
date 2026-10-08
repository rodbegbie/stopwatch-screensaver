"""Score every xscreensaver hack for porting effort against the X11 shim.

Run with: uv run tools/score_hacks.py [--vendor DIR] [--out FILE]
"""

import argparse
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
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
MISSING_HEADER = re.compile(r"fatal error: '([^']+)' file not found")
ERROR_PATTERNS = (
    (re.compile(r"use of undeclared identifier '([^']+)'"), "{0}"),
    (re.compile(r"call to undeclared function '([^']+)'"), "{0}"),
    (re.compile(r"unknown type name '([^']+)'"), "{0}"),
    (re.compile(r"no member named '([^']+)' in '(?:struct )?([^']+)'"), "{1}.{0}"),
)
UNNAMED = re.compile(r"::\(unnamed[^)]*\)")
MAX_HEADER_STUBS = 25
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


def blockers_for(
    name: str, source: str, include_dirs: list[Path], cc: str = "cc"
) -> list[str]:
    """Syntax-check source against the shim headers and list what is missing.

    Missing headers are stubbed with empty files so the compiler keeps going
    and reports everything else too. The source is compiled from a temporary
    directory so quoted includes cannot pick up the real xscreensaver headers.
    """
    found: set[str] = set()
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        stubs = tmp / "stubs"
        stubs.mkdir()
        src_path = tmp / f"{name}.c"
        src_path.write_text(source)
        cmd = [cc, "-fsyntax-only", "-w", "-ferror-limit=0"]
        cmd += [f"-I{d}" for d in include_dirs] + [f"-I{stubs}", str(src_path)]
        stderr = ""
        for _ in range(MAX_HEADER_STUBS):
            stderr = subprocess.run(cmd, capture_output=True, text=True).stderr
            missing = MISSING_HEADER.search(stderr)
            if not missing:
                break
            header = missing.group(1)
            found.add(header)
            (stubs / header).parent.mkdir(parents=True, exist_ok=True)
            (stubs / header).write_text("")
    for pattern, template in ERROR_PATTERNS:
        for match in pattern.finditer(stderr):
            found.add(UNNAMED.sub("", template.format(*match.groups())))
    return sorted(found)


def score(
    path: Path, source: str, provided: set[str], blockers: list[str] | None = None
) -> dict:
    clean = strip_noise(source)
    used = used_calls(source)
    missing = sorted(used - provided)
    kind = classify(path, source)
    flags = [name for name, calls in FLAG_CALLS.items() if used & calls]
    if len(MATH_CALL.findall(clean)) >= FLOAT_HEAVY_THRESHOLD:
        flags.append("float-heavy")
    if re.search(r'#\s*include\s*"xlockmore\.h"', source):
        flags.append("needs-xlockmore")
    gaps = sorted(set(missing) | set(blockers or []))
    if kind == "gl":
        effort = "XL"
    elif not gaps:
        effort = "S"
    elif len(gaps) > MAX_MEDIUM_MISSING or {"pixmaps", "readback"} & set(flags):
        effort = "L"
    else:
        effort = "M"
    return {
        "name": path.stem,
        "kind": kind,
        "effort": effort,
        "missing": missing,
        "gaps": gaps,
        "flags": flags,
        "loc": source.count("\n") + 1,
    }


def rank_missing(rows: list[dict], n: int) -> list[tuple[str, float, int]]:
    """Shim gaps ranked for 2D hacks. A hack with k gaps gives each 1/k, so
    gaps blocking nearly-ready hacks rank first."""
    weight: dict[str, float] = defaultdict(float)
    hacks: dict[str, int] = defaultdict(int)
    for row in rows:
        gaps = row.get("gaps", row["missing"])
        if row["kind"] != "2d" or not gaps:
            continue
        for call in gaps:
            weight[call] += 1 / len(gaps)
            hacks[call] += 1
    ranked = sorted(weight, key=lambda c: (-weight[c], c))
    return [(c, round(weight[c], 2), hacks[c]) for c in ranked[:n]]


def render_table(rows: list[dict]) -> str:
    lines = [
        "| Hack | Kind | Effort | Shim gaps | Flags | LOC |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for r in sorted(rows, key=lambda r: (r["effort"], r["name"])):
        missing = ", ".join(r.get("gaps", r["missing"])) or "-"
        flags = ", ".join(r["flags"]) or "-"
        lines.append(
            f"| {r['name']} | {r['kind']} | {r['effort']} | {missing} | {flags} | {r['loc']} |"
        )
    return "\n".join(lines) + "\n"


def render_ranking(ranked: list[tuple[str, float, int]]) -> str:
    lines = ["| Gap | Score | 2D hacks needing it |", "| --- | --- | --- |"]
    lines += [f"| {c} | {s} | {h} |" for c, s, h in ranked]
    return "\n".join(lines) + "\n"


def scan(vendor: Path, provided: set[str], include_dirs: list[Path]) -> list[dict]:
    files = sorted((vendor / "hacks").glob("*.c")) + sorted(
        (vendor / "hacks" / "glx").glob("*.c")
    )
    sources = {p: p.read_text(errors="replace") for p in files}

    def one(path: Path) -> dict:
        source = sources[path]
        blockers = None
        if classify(path, source) == "2d":
            blockers = blockers_for(path.stem, source, include_dirs)
        return score(path, source, provided, blockers)

    with ThreadPoolExecutor(max_workers=8) as pool:
        return list(pool.map(one, files))


def main(argv: list[str]) -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vendor", default=str(next((root / "vendor").glob("xscreensaver-*"))))
    parser.add_argument("--header", default=str(root / "firmware/src/x11shim/xshim.h"))
    parser.add_argument(
        "--shim-include",
        action="append",
        help="include dir for the shim compile check (default: the firmware's)",
    )
    parser.add_argument("--intro", default=str(root / "tools/assessment_intro.md"))
    parser.add_argument("--measured", default=str(root / "tools/assessment_measured.md"))
    parser.add_argument("--out", default=str(root / "docs/porting-assessment.md"))
    args = parser.parse_args(argv)

    vendor = Path(args.vendor)
    version = vendor.name.removeprefix("xscreensaver-")
    includes = [Path(d) for d in args.shim_include] if args.shim_include else [
        root / "firmware/src",
        root / "firmware/src/x11shim/include",
    ]
    rows = scan(vendor, implemented_calls(Path(args.header).read_text()), includes)
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
        + "Shim gaps across 2D hacks, ranked so gaps that block hacks\n"
        + "needing few additions come first.\n\n"
        + render_ranking(rank_missing(rows, 10))
        + "\n## All hacks\n\n"
        + render_table(rows)
    )
    Path(args.out).write_text(out)
    print(f"wrote {args.out}: {len(rows)} hacks, {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
