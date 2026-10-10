"""Predict how fast a GL hack would run on the StopWatch, from the host.

A GL hack's cost on the device is mostly what each frame asks of TinyGL: how
many vertices it sets up and how many pixels it fills. The host can count both
exactly, so this tool runs the hack on the host, counts per frame, and turns
the counts into a frame time with the cost model measured in issue #43.

It predicts, it does not measure: the model knows nothing about PSRAM
contention, and the figures are only as good as its calibration against Gears.
"""

import argparse
import re
import subprocess
import sys
import tempfile
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

import probe_hacks
import tinygl_patch

# Milliseconds a frame spends on fixed work (clearing colour and z, then the
# push to the display), microseconds per vertex set up, extra microseconds per
# lit vertex, and nanoseconds per filled pixel. Issue #43 measured the first
# four on the device. pixel_ns is fitted so that the heavy Gears scene's fill
# comes to the 40 ms measured there (40e6 ns / 69,761 pixels). It is an
# effective figure for scenes of many small triangles: it also absorbs the
# rasteriser's per-triangle overhead, so a scene of few, large triangles is
# likely over-predicted. Lines and points are not costed at all.
MODEL = {
    "clear_ms": 24.6,
    "push_ms": 32.0,
    "vertex_us": 4.0,
    "lit_vertex_us": 2.8,
    "pixel_ns": 570.0,
}


# Seeds of this repo's Gears that reproduce the two scenes measured on the
# device in issue #43: the heavy planetary layout (17,706 vertices) and a light
# one (about 4,300). The spike's own seed numbers do not apply here.
GEARS_SEEDS = {"heavy": 13, "light": 11}
CALIBRATION_FRAMES = 60

# Issue #43's breakdown of the heavy scene, in milliseconds a frame: the sum of
# its parts is 197 to 217. Light layouts ran at about 9 fps.
HEAVY_GEARS_MS = 207.0
LIGHT_GEARS_MS = 111.0


@dataclass
class Coverage:
    """Fractions of the canvas, averaged over frames. `bbox` is the box around
    everything a frame drew. A hack that clears only the previous frame's box
    clears `clear`, and pushes the rows of the old and new boxes, `push`."""

    bbox: float
    clear: float
    push: float


@dataclass
class FrameCounts:
    """What an average frame asks of TinyGL."""

    vertices: float
    lit_vertices: float
    triangles: float
    lines: float
    points: float
    pixels: float
    # Enabled lights summed over lit vertices: each light is shaded separately.
    light_terms: float = 0.0


def predict_ms(counts: FrameCounts, model: dict[str, float] = MODEL) -> float:
    return (
        model["clear_ms"]
        + model["push_ms"]
        + counts.vertices * model["vertex_us"] / 1e3
        + counts.light_terms * model["lit_vertex_us"] / 1e3
        + counts.pixels * model["pixel_ns"] / 1e6
    )


def fps(ms: float) -> float:
    return 1000.0 / ms


# Index into the `gl_budget_counts` array that the host driver defines.
COUNTER_NAMES = (
    "vertices",
    "lit_vertices",
    "triangles",
    "lines",
    "points",
    "pixels",
    "light_terms",
)

COUNTS_DECLARATION = "extern double gl_budget_counts[7];\n"

# Adds to the fill count the screen area of a triangle about to be
# rasterised, overdraw included, capped at the framebuffer.
TRIANGLE_HELPER = """\
extern double gl_budget_counts[7];

static void gl_budget_triangle(GLContext* c, GLVertex* p0, GLVertex* p1,
                               GLVertex* p2) {
	double area = 0.5 * ((double)(p1->zp.x - p0->zp.x) * (p2->zp.y - p0->zp.y) -
	                     (double)(p2->zp.x - p0->zp.x) * (p1->zp.y - p0->zp.y));
	double screen = (double)c->zb->xsize * c->zb->ysize;
	if (area < 0) area = -area;
	gl_budget_counts[2]++;
	gl_budget_counts[5] += area < screen ? area : screen;
}

"""

VERTEX_ENTRY = (
    "void glopVertex(GLParam* p) {\n"
    "\tGLVertex* v;\n"
    "\tGLint n, i, cnt;\n"
    "\tGLContext* c = gl_get_context();\n"
)

# (path, anchor, expected count, replacement). Each anchor must occur exactly
# that many times, so an upstream change fails loudly rather than miscounting.
INSTRUMENTATION = (
    (
        "src/zgl.h",
        "#define _tgl_zgl_h_\n",
        1,
        "#define _tgl_zgl_h_\n" + COUNTS_DECLARATION,
    ),
    (
        "src/vertex.c",
        VERTEX_ENTRY,
        1,
        VERTEX_ENTRY + "\tgl_budget_counts[0]++;\n" + "\tif (c->lighting_enabled) {\n"
        "\t\tGLLight* gl_budget_l;\n"
        "\t\tgl_budget_counts[1]++;\n"
        "\t\tfor (gl_budget_l = c->first_light; gl_budget_l; gl_budget_l = gl_budget_l->next)\n"
        "\t\t\tgl_budget_counts[6]++;\n"
        "\t}\n",
    ),
    (
        "src/clip.c",
        "void gl_draw_point(GLVertex* p0) {\n",
        1,
        TRIANGLE_HELPER
        + "void gl_draw_point(GLVertex* p0) {\n\tgl_budget_counts[4]++;\n",
    ),
    (
        "src/clip.c",
        "void gl_draw_line(GLVertex* p1, GLVertex* p2) {\n",
        1,
        "void gl_draw_line(GLVertex* p1, GLVertex* p2) {\n\tgl_budget_counts[3]++;\n",
    ),
    (
        "src/clip.c",
        "c->draw_triangle_front(p0, p1, p2);",
        2,
        "{ gl_budget_triangle(c, p0, p1, p2); c->draw_triangle_front(p0, p1, p2); }",
    ),
    (
        "src/clip.c",
        "c->draw_triangle_back(p0, p1, p2);",
        2,
        "{ gl_budget_triangle(c, p0, p1, p2); c->draw_triangle_back(p0, p1, p2); }",
    ),
)


def instrument(sources: Mapping[str, str]) -> dict[str, str]:
    """Adds the counters to a tree that tinygl_patch.patch_tree has already
    patched. Returns a new dict; the argument is left alone."""
    out = dict(sources)
    for path, anchor, expected, replacement in INSTRUMENTATION:
        text = out.get(path, "")
        found = text.count(anchor)
        if found != expected:
            raise ValueError(
                f"expected {expected} of {anchor!r} in {path}, found {found}; "
                f"{tinygl_patch.UPSTREAM_CHANGED}"
            )
        out[path] = text.replace(anchor, replacement)
    return out


def read_tinygl(root: Path) -> dict[str, str]:
    """The upstream tree under firmware/src/tinygl, as patch_tree takes it."""
    source = root / "firmware" / "src" / "tinygl"
    return {
        str(path.relative_to(source)): path.read_text(errors="surrogateescape")
        for sub in ("src", "include")
        for path in sorted((source / sub).rglob("*"))
        if path.is_file()
    }


def load_tinygl(root: Path) -> dict[str, str]:
    """TinyGL as the firmware builds it, plus the harness's counters."""
    return instrument(tinygl_patch.patch_tree(read_tinygl(root)))


RUN_TIMEOUT_S = 120
WARMUP_FRAMES = 10
COUNTS_LINE = re.compile(r"^counts frames=(\d+) (.*)$", re.MULTILINE)


def shim_sources(root: Path) -> list[Path]:
    """What probe_hacks links, with this tool's driver in place of dump_main,
    plus the GL layer: glshim's own files and its wrappers for the glx helpers."""
    files = [f for f in probe_hacks.shim_sources(root) if f.name != "dump_main.c"]
    files.append(root / "firmware" / "native" / "gl_budget_main.c")
    files += sorted((root / "firmware" / "src" / "glshim").glob("*.c"))
    return files


def parse_counts(stdout: str) -> tuple[FrameCounts, int, float, Coverage]:
    """Reads the driver's one `counts` line: (counts, frames, host_ms,
    coverage)."""
    match = COUNTS_LINE.search(stdout)
    if not match:
        raise probe_hacks.ProbeError(
            f"no counts in the program's output: {stdout.strip()[:80]!r}"
        )
    values = dict(pair.split("=") for pair in match.group(2).split())
    counts = FrameCounts(**{name: float(values[name]) for name in COUNTER_NAMES})
    coverage = Coverage(
        bbox=float(values["bbox"]),
        clear=float(values["clear"]),
        push=float(values["push"]),
    )
    return counts, int(match.group(1)), float(values["host_ms"]), coverage


def write_tinygl(root: Path, generated: Path) -> None:
    """The instrumented TinyGL tree and its unity file, where the firmware's
    build puts the patched one."""
    tree = load_tinygl(root)
    for relative, text in tree.items():
        path = generated / "tinygl" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode(errors="surrogateescape"))
    (generated / "tinygl_unity.c").write_text(tinygl_patch.unity_source(tree))


def measure(
    name: str,
    source: str,
    root: Path,
    frames: int = 300,
    seed: int = 1,
    cc: str = "cc",
) -> dict:
    """Builds the hack with USE_GL against the shim and the instrumented
    TinyGL, runs it, and returns {"counts": FrameCounts, "host_ms": float,
    "frames": int} or {"error": str}."""
    try:
        class_name, prefix = probe_hacks.module_entry(source)
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            generated = tmp / "generated"
            write_tinygl(root, generated)
            (tmp / f"{name}.c").write_text("#define USE_GL\n" + source)
            registry = tmp / "probe_registry.c"
            registry.write_text(
                probe_hacks.registry_source(
                    class_name, prefix, bool(probe_hacks.XLOCKMORE.search(source))
                )
            )
            program = tmp / "gl_budget"
            src = root / "firmware" / "src"
            cmd = [
                cc,
                *probe_hacks.BUILD_FLAGS,
                "-w",
                "-DSTANDALONE",
                "-DHAVE_MOBILE",
                "-DXSHIM_NATIVE",
                f"-I{src}",
                f"-I{src / 'x11shim' / 'include'}",
                f"-I{src / 'xs_support'}",
                f"-I{src / 'xs_support' / 'glx'}",
                f"-I{generated / 'tinygl' / 'include'}",
                f"-I{generated}",
                str(tmp / f"{name}.c"),
                str(registry),
                *map(str, shim_sources(root)),
                "-lm",
                "-lpthread",
                "-o",
                str(program),
            ]
            build = subprocess.run(cmd, capture_output=True, text=True, check=False)
            if build.returncode != 0:
                missing = probe_hacks.undefined_symbols(build.stderr)
                if missing:
                    return {"error": "does not link: " + ", ".join(missing[:6])}
                first = probe_hacks.FIRST_ERROR.search(build.stderr)
                detail = first.group(0).strip() if first else f"exit {build.returncode}"
                return {"error": "does not compile: " + detail[:160]}
            run = subprocess.run(
                [str(program), str(frames), str(seed)],
                capture_output=True,
                text=True,
                timeout=RUN_TIMEOUT_S,
                check=False,
            )
            if run.returncode != 0:
                how = (
                    f"signal {-run.returncode}"
                    if run.returncode < 0
                    else f"exit {run.returncode}"
                )
                return {"error": f"crashed on the host ({how})"}
            counts, ran, host_ms, cover = parse_counts(run.stdout)
            return {
                "counts": counts,
                "frames": ran,
                "host_ms": host_ms,
                "coverage": cover,
            }
    except subprocess.TimeoutExpired:
        return {"error": f"timed out after {RUN_TIMEOUT_S} s on the host"}
    except FileNotFoundError:
        return {"error": f"compiler not found: {cc}"}
    except probe_hacks.ProbeError as err:
        return {"error": str(err)}


DEFAULT_SEEDS = [1, 2, 3]
VENDOR_GLX = Path("vendor") / "xscreensaver-6.16" / "hacks" / "glx"


def hack_path(root: Path, name: str) -> Path:
    """Gears is ported, so its source is in the firmware tree. Anything else
    comes from the local, git-ignored xscreensaver checkout."""
    if name == "gears":
        return root / "firmware" / "src" / "hacks" / "gears" / "gears.c"
    path = root / VENDOR_GLX / f"{name}.c"
    if not path.exists():
        raise FileNotFoundError(f"no source for {name}: {path} does not exist")
    return path


def note_for(row: dict) -> str:
    if "error" in row:
        return row["error"]
    counts = row["counts"]
    if counts.vertices == 0:
        return "draws nothing"
    return ""


def render_report(rows: list[dict]) -> str:
    lines = [
        "| Hack | Seed | Vertices | Lit | Triangles | Lines | Points | Pixels "
        "| Box % | Clear % | Push % | Predicted ms | Predicted fps | Note |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for row in rows:
        note = note_for(row)
        cov = row.get("coverage")
        cover = (
            f"{cov.bbox * 100:.0f} | {cov.clear * 100:.0f} | {cov.push * 100:.0f}"
            if cov
            else "- | - | -"
        )
        if "counts" in row:
            c = row["counts"]
            numbers = (
                f"{c.vertices:.0f} | {c.lit_vertices:.0f} | {c.triangles:.0f} | "
                f"{c.lines:.0f} | {c.points:.0f} | {c.pixels:.0f}"
            )
            if c.vertices == 0:
                prediction = "- | -"
            else:
                ms = predict_ms(c)
                prediction = f"{ms:.0f} | {fps(ms):.1f}"
        else:
            numbers = " | ".join("-" for _ in range(6))
            prediction = "- | -"
        lines.append(
            f"| {row['name']} | {row['seed']} | {numbers} | {cover} | {prediction} "
            f"| {note} |"
        )
    model = ", ".join(f"{key}={value:g}" for key, value in MODEL.items())
    return "\n".join(lines) + f"\n\nPredictions, not measurements. Model: {model}.\n"


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Predict the device frame rate of GL hacks from host counts."
    )
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument(
        "--seeds",
        default=",".join(map(str, DEFAULT_SEEDS)),
        help="comma-separated seeds tried for each hack without its own",
    )
    parser.add_argument(
        "hacks", nargs="+", help="hack name, or name:seed to give it one seed"
    )
    args = parser.parse_args(argv)
    seeds = [int(seed) for seed in args.seeds.split(",")]
    parsed = []
    for spec in args.hacks:
        name, _, seed = spec.partition(":")
        parsed.append((name, [int(seed)] if seed else seeds))
    args.hacks = parsed
    return args


def main(argv: list[str]) -> int:
    args = parse_arguments(argv)
    root = Path(__file__).resolve().parent.parent
    rows = []
    for name, seeds in args.hacks:
        try:
            source = hack_path(root, name).read_text(errors="surrogateescape")
        except FileNotFoundError as err:
            rows += [{"name": name, "seed": seed, "error": str(err)} for seed in seeds]
            continue
        for seed in seeds:
            result = measure(name, source, root, frames=args.frames, seed=seed)
            rows.append({"name": name, "seed": seed, **result})
    print(render_report(rows), end="")
    return 1 if any("error" in row for row in rows) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
