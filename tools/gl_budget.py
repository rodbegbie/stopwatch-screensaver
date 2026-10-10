"""Predict how fast a GL hack would run on the StopWatch, from the host.

A GL hack's cost on the device is mostly what each frame asks of TinyGL: how
many vertices it sets up and how many pixels it fills. The host can count both
exactly, so this tool runs the hack on the host, counts per frame, and turns
the counts into a frame time with the cost model measured in issue #43.

It predicts, it does not measure: the model knows nothing about PSRAM
contention, and the figures are only as good as its calibration against Gears.
"""

from dataclasses import dataclass

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
