"""Predict how fast a GL hack would run on the StopWatch, from the host.

A GL hack's cost on the device is mostly what each frame asks of TinyGL: how
many vertices it sets up and how many pixels it fills. The host can count both
exactly, so this tool runs the hack on the host, counts per frame, and turns
the counts into a frame time with the cost model measured in issue #43.

It predicts, it does not measure: the model knows nothing about PSRAM
contention, and the figures are only as good as its calibration against Gears.
"""

from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

import tinygl_patch

# Milliseconds a frame spends on fixed work (clearing colour and z, then the
# push to the display), microseconds per vertex set up, extra microseconds per
# lit vertex, and nanoseconds per filled pixel. Issue #43 measured the first
# four on the device; pixel_ns is fitted to Gears in the calibration test.
MODEL = {
    "clear_ms": 24.6,
    "push_ms": 32.0,
    "vertex_us": 4.0,
    "lit_vertex_us": 2.8,
    "pixel_ns": 40.0,
}


@dataclass
class FrameCounts:
    """What an average frame asks of TinyGL."""

    vertices: float
    lit_vertices: float
    triangles: float
    lines: float
    points: float
    pixels: float


def predict_ms(counts: FrameCounts, model: dict[str, float] = MODEL) -> float:
    return (
        model["clear_ms"]
        + model["push_ms"]
        + counts.vertices * model["vertex_us"] / 1e3
        + counts.lit_vertices * model["lit_vertex_us"] / 1e3
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
)

COUNTS_DECLARATION = "extern double gl_budget_counts[6];\n"

# Adds to the fill count the screen area of a triangle about to be
# rasterised, overdraw included, capped at the framebuffer.
TRIANGLE_HELPER = """\
extern double gl_budget_counts[6];

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
        VERTEX_ENTRY
        + "\tgl_budget_counts[0]++;\n"
        + "\tif (c->lighting_enabled) gl_budget_counts[1]++;\n",
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
