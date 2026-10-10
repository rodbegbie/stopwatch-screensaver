"""Score every xscreensaver hack for porting effort against the X11 shim.

Run with: uv run tools/score_hacks.py [--vendor DIR] [--out FILE]
"""

import argparse
import re
import subprocess
import sys
import tempfile
import textwrap
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import probe_hacks

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
class ScoreError(Exception):
    pass


ANY_ERROR = re.compile(r"^.*?\berror: (.+)$", re.MULTILINE)
MODULE_ENTRY = re.compile(r"\bXSCREENSAVER_MODULE(?:_2)?\s*\(")
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
# The hacks worth running on the host are the S ones. A gap is a compile error,
# so an M or L hack cannot be built until the shim fills it, and a GL hack needs
# a rasteriser first.
PROBED_EFFORTS = {"S"}
EFFORT_ORDER = {"S": 0, "M": 1, "L": 2, "XL": 3}
REGISTRY_ARRAY = re.compile(r"\bg_hacks\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;", re.DOTALL)
REGISTRY_ENTRY = re.compile(r"&(\w+)_hack\b")
PORTED = "\u2705"
FAILED = "\u274c"


def strip_noise(source: str) -> str:
    return NOISE.sub(lambda m: "\n" * m.group(0).count("\n") or " ", source)


def is_hack(source: str) -> bool:
    """A screensaver registers itself with XSCREENSAVER_MODULE; everything
    else in hacks/ is a model, a helper library or a command-line tool."""
    return bool(MODULE_ENTRY.search(strip_noise(source)))


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
        cmd = [cc, "-fsyntax-only", "-w", "-ferror-limit=0", "-DSTANDALONE"]
        cmd += [f"-I{d}" for d in include_dirs] + [f"-I{stubs}", str(src_path)]
        run = None
        for _ in range(MAX_HEADER_STUBS):
            try:
                run = subprocess.run(cmd, capture_output=True, text=True)
            except FileNotFoundError as err:
                raise ScoreError(f"compiler not found: {cc}") from err
            missing = MISSING_HEADER.search(run.stderr)
            if not missing:
                break
            header = missing.group(1)
            found.add(header)
            (stubs / header).parent.mkdir(parents=True, exist_ok=True)
            (stubs / header).write_text("")
    stderr = run.stderr
    recognised = [MISSING_HEADER, *(pattern for pattern, _ in ERROR_PATTERNS)]
    for pattern, template in ERROR_PATTERNS:
        for match in pattern.finditer(stderr):
            found.add(UNNAMED.sub("", template.format(*match.groups())))
    for match in MISSING_HEADER.finditer(stderr):
        found.add(match.group(1))
    for line in stderr.splitlines():
        error = ANY_ERROR.match(line)
        if error and not any(p.search(line) for p in recognised):
            found.add("error: " + UNNAMED.sub("", error.group(1)))
    if run.returncode != 0 and not found:
        found.add(f"compiler exited {run.returncode} with no diagnostics")
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


def registered_hacks(registry: str) -> set[str]:
    """Names of the hacks in firmware's g_hacks[]: the ones already ported."""
    array = REGISTRY_ARRAY.search(strip_noise(registry))
    if not array:
        raise ScoreError("no g_hacks[] array found in the registry source")
    return set(REGISTRY_ENTRY.findall(array.group(1)))


def read_failed_ports(text: str) -> dict[str, str]:
    """Parses failed_ports.txt: one `name: reason` per line, # for comments."""
    failed = {}
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, colon, reason = line.partition(":")
        if not colon or not name.strip():
            raise ScoreError(
                f"failed_ports line {number} is not `name: reason`: {line}"
            )
        failed[name.strip()] = reason.strip()
    return failed


def check_ports(ported: set[str], failed: dict[str, str], known: set[str]) -> None:
    """Catches a hack marked both ways and names that match no scanned hack."""
    both = sorted(ported & set(failed))
    if both:
        raise ScoreError(f"ported and also listed as failed: {', '.join(both)}")
    unknown = sorted((ported | set(failed)) - known)
    if unknown:
        raise ScoreError(f"not a scanned hack (typo?): {', '.join(unknown)}")


def code_cell(text: str) -> str:
    """Markdown-safe table cell: a code span with pipes escaped."""
    return "`" + text.replace("`", "'").replace("|", "\\|") + "`"


def probe_speed(rows: list[dict], ported: set[str], probe_one, workers: int = 4) -> None:
    """Runs each unported 2D hack rated S on the host and stores the result
    as row["speed"]: {"host_ms": ...} or {"error": ...}. `probe_one(name)` does
    the build and run (see probe_hacks.probe)."""
    todo = [
        r
        for r in rows
        if r["name"] not in ported and r["kind"] == "2d" and r["effort"] in PROBED_EFFORTS
    ]
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for row, result in zip(todo, pool.map(lambda r: probe_one(r["name"]), todo)):
            row["speed"] = result


def speed_cell(row: dict, done: bool = False, measured: dict | None = None) -> str:
    """A ported hack shows the step measured on the device; an unported one
    shows its host band, or why it could not be probed."""
    if done:
        steps = (measured or {}).get(row["name"])
        if not steps:
            return "-"
        lo, hi = steps
        return (f"{lo:g}" if lo == hi else f"{lo:g}-{hi:g}") + " ms measured"
    speed = row.get("speed")
    if not speed:
        return "-"
    if "error" in speed:
        return speed["error"].split(":")[0].split(" (")[0]
    return f"{probe_hacks.band(speed['host_ms'])} ({speed['host_ms']:.2g} ms)"


def render_table(
    rows: list[dict], ported=frozenset(), failed=frozenset(), measured=None
) -> str:
    lines = [
        "| Hack | Kind | Effort | Speed | Ported | Shim gaps | Flags | LOC |",
        "| --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    def order(r):
        return (r["name"] in ported, EFFORT_ORDER[r["effort"]], r["name"])

    for r in sorted(rows, key=order):
        missing = ", ".join(code_cell(g) for g in r.get("gaps", r["missing"])) or "-"
        flags = ", ".join(r["flags"]) or "-"
        done = r["name"] in ported
        mark = PORTED if done else FAILED if r["name"] in failed else "-"
        effort = "-" if done else r["effort"]
        speed = speed_cell(r, done, measured)
        lines.append(
            f"| {r['name']} | {r['kind']} | {effort} | {speed} | {mark} | {missing} | {flags} | {r['loc']} |"
        )
    return "\n".join(lines) + "\n"


def render_failed(failed: dict[str, str]) -> str:
    """The reasons behind the failed marks; nothing at all if none failed."""
    if not failed:
        return ""
    items = "".join(f"- **{name}**: {failed[name]}\n" for name in sorted(failed))
    return "\n## Failed ports\n\n" + items


def render_ranking(ranked: list[tuple[str, float, int]]) -> str:
    lines = ["| Gap | Score | 2D hacks needing it |", "| --- | --- | --- |"]
    lines += [f"| {code_cell(c)} | {s} | {h} |" for c, s, h in ranked]
    return "\n".join(lines) + "\n"


def render_excluded(names: list[str]) -> str:
    return textwrap.fill(", ".join(names), width=78) + "\n"


def scan(
    vendor: Path, provided: set[str], include_dirs: list[Path]
) -> tuple[list[dict], list[str]]:
    """Returns (scored hacks, names of excluded non-hack files)."""
    files = sorted((vendor / "hacks").glob("*.c")) + sorted(
        (vendor / "hacks" / "glx").glob("*.c")
    )
    all_sources = {p: p.read_text(errors="replace") for p in files}
    sources = {p: src for p, src in all_sources.items() if is_hack(src)}
    excluded = sorted(p.stem for p in all_sources if p not in sources)
    files = list(sources)

    def one(path: Path) -> dict:
        source = sources[path]
        blockers = None
        if classify(path, source) == "2d":
            blockers = blockers_for(path.stem, source, include_dirs)
        return score(path, source, provided, blockers)

    with ThreadPoolExecutor(max_workers=8) as pool:
        return list(pool.map(one, files)), excluded


def find_vendor(root: Path) -> Path:
    """The highest-versioned xscreensaver source under root/vendor."""
    candidates = [p for p in (root / "vendor").glob("xscreensaver-*") if p.is_dir()]
    if not candidates:
        raise ScoreError(
            "no xscreensaver source in vendor/; run "
            "`uv run tools/fetch_xscreensaver.py` or pass --vendor DIR"
        )

    def version(path: Path) -> tuple[int, ...]:
        parts = path.name.removeprefix("xscreensaver-").split(".")
        return tuple(int(p) if p.isdigit() else 0 for p in parts)

    return max(candidates, key=version)


def main(argv: list[str], root: Path | None = None) -> int:
    root = root or Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vendor", help="xscreensaver source dir (default: newest in vendor/)")
    parser.add_argument("--header", default=str(root / "firmware/src/x11shim/xshim.h"))
    parser.add_argument(
        "--shim-include",
        action="append",
        help="include dir for the shim compile check (default: the firmware's)",
    )
    parser.add_argument("--registry", default=str(root / "firmware/src/hacks/registry.c"))
    parser.add_argument("--failed-ports", default=str(root / "tools/failed_ports.txt"))
    parser.add_argument("--intro", default=str(root / "tools/assessment_intro.md"))
    parser.add_argument("--measured", default=str(root / "tools/assessment_measured.md"))
    parser.add_argument("--out", default=str(root / "docs/porting-assessment.md"))
    parser.add_argument(
        "--no-probe",
        dest="probe",
        action="store_false",
        help="skip running the S hacks on the host for the Speed column",
    )
    args = parser.parse_args(argv)

    try:
        vendor = Path(args.vendor) if args.vendor else find_vendor(root)
        return run(args, vendor, root)
    except ScoreError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1


def default_includes(root: Path) -> list[Path]:
    return [
        root / "firmware/src",
        root / "firmware/src/x11shim/include",
        root / "firmware/src/xs_support",
    ]


def run(args: argparse.Namespace, vendor: Path, root: Path) -> int:
    version = vendor.name.removeprefix("xscreensaver-")
    includes = (
        [Path(d) for d in args.shim_include]
        if args.shim_include
        else default_includes(root)
    )
    rows, excluded = scan(
        vendor, implemented_calls(Path(args.header).read_text()), includes
    )
    registry = Path(args.registry)
    if not registry.exists():
        raise ScoreError(f"registry not found: {registry} (see --registry)")
    ported = registered_hacks(registry.read_text())
    failed_path = Path(args.failed_ports)
    failed = read_failed_ports(failed_path.read_text()) if failed_path.exists() else {}
    check_ports(ported, failed, {r["name"] for r in rows})
    if args.probe:
        probe_speed(
            rows,
            ported,
            lambda name: probe_hacks.probe(
                name, (vendor / "hacks" / f"{name}.c").read_text(errors="replace"), root
            ),
        )
    todo = [r for r in rows if r["name"] not in ported]
    counts = {e: sum(r["effort"] == e for r in todo) for e in ("S", "M", "L", "XL")}
    intro = Path(args.intro).read_text().format(
        version=version,
        total=len(rows),
        n_ported=len(ported),
        excluded=len(excluded),
        **{f"n_{k.lower()}": v for k, v in counts.items()},
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
        + render_table(rows, ported, set(failed), probe_hacks.parse_measured(measured))
        + render_failed(failed)
        + "\n## Excluded files\n\n"
        + "These files in `hacks/` and `hacks/glx/` have no `XSCREENSAVER_MODULE`\n"
        + "entry point, so they are models, helper libraries or command-line\n"
        + "tools rather than screensavers:\n\n"
        + render_excluded(excluded)
    )
    Path(args.out).write_text(out)
    print(f"wrote {args.out}: {len(rows)} hacks ({len(ported)} ported, {len(excluded)} files excluded), {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
