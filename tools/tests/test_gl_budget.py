import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gl_budget as gb  # noqa: E402
import tinygl_patch  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
needs_cc = pytest.mark.skipif(shutil.which("cc") is None, reason="no C compiler")


def patched_tree():
    return tinygl_patch.patch_tree(gb.read_tinygl(ROOT))


def test_instrument_adds_a_counter_at_each_site():
    tree = gb.load_tinygl(ROOT)
    assert "gl_budget_counts[0]" in tree["src/vertex.c"]
    assert "gl_budget_counts[1]" in tree["src/vertex.c"]
    for index in (2, 3, 4, 5):
        assert f"gl_budget_counts[{index}]" in tree["src/clip.c"]
    assert "extern double gl_budget_counts" in tree["src/zgl.h"]


def test_instrument_fails_when_an_anchor_is_missing():
    tree = patched_tree()
    tree["src/clip.c"] = ""
    with pytest.raises(ValueError, match="has upstream changed"):
        gb.instrument(tree)


def test_instrument_leaves_its_argument_alone():
    tree = patched_tree()
    before = dict(tree)
    gb.instrument(tree)
    assert tree == before


def test_predict_with_nothing_drawn_is_the_fixed_cost():
    nothing = gb.FrameCounts(0, 0, 0, 0, 0, 0)
    assert gb.predict_ms(nothing) == gb.MODEL["clear_ms"] + gb.MODEL["push_ms"]


def test_each_term_adds_its_own_cost():
    model = {
        "clear_ms": 0,
        "push_ms": 0,
        "vertex_us": 4,
        "lit_vertex_us": 2,
        "pixel_ns": 10,
    }
    counts = gb.FrameCounts(
        vertices=1000, lit_vertices=500, triangles=0, lines=0, points=0, pixels=100000
    )
    assert gb.predict_ms(counts, model) == pytest.approx(6.0)


def test_fps_is_the_inverse_of_milliseconds():
    assert gb.fps(125.0) == 8.0


STUB = """\
#define DEFAULTS "*delay: 30000\\n*count: 0\\n*showFPS: False\\n*wireframe: False\\n"
# define release_stub 0
#include "xlockmore.h"

ENTRYPOINT ModeSpecOpt stub_opts = {0, NULL, 0, NULL, NULL};
typedef struct { GLXContext *glx_context; GLuint list; } stub_configuration;
static stub_configuration *bps = NULL;

ENTRYPOINT void reshape_stub (ModeInfo *mi, int w, int h) { }

ENTRYPOINT void init_stub (ModeInfo *mi) {
  stub_configuration *bp;
  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];
  bp->glx_context = init_GL(mi);
  @INIT@
}

ENTRYPOINT void draw_stub (ModeInfo *mi) {
  stub_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  @DRAW@
  glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

ENTRYPOINT Bool stub_handle_event (ModeInfo *mi, XEvent *event) { return False; }
ENTRYPOINT void free_stub (ModeInfo *mi) { }
XSCREENSAVER_MODULE ("Stub", stub)
"""

TRIANGLE = "glVertex3f(-0.5f,-0.5f,0); glVertex3f(0.5f,-0.5f,0); glVertex3f(0,0.5f,0);"


def stub_hack(init: str = "", draw: str = "") -> str:
    return "#define USE_GL\n" + STUB.replace("@INIT@", init).replace("@DRAW@", draw)


def run(init: str = "", draw: str = "", frames: int = 30, seed: int = 1) -> dict:
    return gb.measure("stub", stub_hack(init, draw), ROOT, frames=frames, seed=seed)


@needs_cc
def test_triangles_in_a_replayed_display_list_count_every_frame():
    init = (
        "bp->list = glGenLists(1); glNewList(bp->list, GL_COMPILE);"
        f"glBegin(GL_TRIANGLES); {TRIANGLE * 10} glEnd(); glEndList();"
    )
    result = run(init, "glCallList(bp->list);")
    assert result["counts"].triangles == 10
    assert result["counts"].vertices == 30
    assert result["counts"].pixels > 0


@needs_cc
def test_lines_and_points_are_counted():
    draw = (
        "glBegin(GL_LINES); glVertex3f(-.5f,0,0); glVertex3f(.5f,0,0);"
        "glVertex3f(0,-.5f,0); glVertex3f(0,.5f,0); glEnd();"
        "glBegin(GL_POINTS); glVertex3f(0,0,0); glVertex3f(.1f,0,0);"
        "glVertex3f(.2f,0,0); glEnd();"
    )
    counts = run(draw=draw)["counts"]
    assert (counts.lines, counts.points, counts.vertices) == (2, 3, 7)


@needs_cc
def test_lit_vertices_are_counted_only_when_lighting_is_on():
    draw = f"glBegin(GL_TRIANGLES); glNormal3f(0,0,1); {TRIANGLE} glEnd();"
    unlit = run(draw=draw)["counts"]
    lit = run(init="glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);", draw=draw)["counts"]
    assert (unlit.vertices, unlit.lit_vertices) == (3, 0)
    assert (lit.vertices, lit.lit_vertices) == (3, 3)


@needs_cc
def test_a_hack_that_draws_nothing_reports_zero():
    counts = run()["counts"]
    assert (counts.vertices, counts.triangles, counts.pixels) == (0, 0, 0)


@needs_cc
def test_a_broken_hack_returns_an_error_not_a_traceback():
    source = stub_hack() + "\nint broken = ;\n"
    result = gb.measure("stub", source, ROOT, frames=3)
    assert "error:" in result["error"]


@needs_cc
def test_the_same_seed_gives_the_same_counts():
    draw = (
        "{ int n = 1 + random() % 8; glBegin(GL_TRIANGLES);"
        f"while (n--) {{ {TRIANGLE} }} glEnd(); }}"
    )
    first = run(draw=draw, frames=50, seed=3)["counts"]
    again = run(draw=draw, frames=50, seed=3)["counts"]
    other = run(draw=draw, frames=50, seed=4)["counts"]
    assert first == again
    assert first != other


def test_hack_path_finds_gears_in_the_firmware_tree():
    path = gb.hack_path(ROOT, "gears")
    assert path == ROOT / "firmware" / "src" / "hacks" / "gears" / "gears.c"


def test_hack_path_names_the_missing_vendor_file():
    with pytest.raises(FileNotFoundError, match="no_such_hack.c"):
        gb.hack_path(ROOT, "no_such_hack")


def measured_row(**counts) -> dict:
    values = dict(
        vertices=1000, lit_vertices=0, triangles=300, lines=0, points=0, pixels=5e4
    )
    values.update(counts)
    return {
        "name": "demo",
        "seed": 2,
        "counts": gb.FrameCounts(**values),
        "frames": 10,
        "host_ms": 1.0,
    }


def test_report_shows_the_counts_and_a_prediction():
    report = gb.render_report([measured_row()])
    assert "| demo | 2 | 1000 | 0 | 300 | 0 | 0 | 50000 |" in report
    ms = gb.predict_ms(measured_row()["counts"])
    assert f"{ms:.0f}" in report
    assert f"{gb.fps(ms):.1f}" in report


def test_report_shows_an_error_instead_of_numbers():
    report = gb.render_report(
        [{"name": "bad", "seed": 1, "error": "does not link: gllist"}]
    )
    assert "does not link: gllist" in report
    assert "inf" not in report


def test_a_hack_that_draws_nothing_gets_no_frame_rate():
    report = gb.render_report([measured_row(vertices=0, triangles=0, pixels=0)])
    assert "draws nothing" in report
    assert (
        f"{gb.fps(gb.predict_ms(gb.FrameCounts(0, 0, 0, 0, 0, 0))):.1f}" not in report
    )


def test_parse_arguments_gives_a_named_hack_its_own_seed():
    args = gb.parse_arguments(["--frames", "20", "gears:7", "cubicgrid"])
    assert args.frames == 20
    assert args.hacks == [("gears", [7]), ("cubicgrid", [1, 2, 3])]


@needs_cc
def test_the_command_line_runs_gears_end_to_end(capsys):
    assert gb.main(["--frames", "5", "gears:1"]) == 0
    out = capsys.readouterr().out
    assert "| gears | 1 |" in out


def gears_prediction(which: str) -> float:
    result = gb.measure(
        "gears",
        gb.hack_path(ROOT, "gears").read_text(errors="surrogateescape"),
        ROOT,
        frames=gb.CALIBRATION_FRAMES,
        seed=gb.GEARS_SEEDS[which],
    )
    return gb.predict_ms(result["counts"])


@needs_cc
def test_model_reproduces_heavy_gears():
    """pixel_ns is fitted to this scene, so this one is in-sample."""
    assert gears_prediction("heavy") == pytest.approx(gb.HEAVY_GEARS_MS, rel=0.10)


@needs_cc
def test_heavy_gears_fill_term_is_the_40_ms_measured_on_the_device():
    """The fit itself. A looser check on the total cannot see a halved
    pixel_ns: it moves the heavy scene only from 217 ms to 197 ms."""
    result = gb.measure(
        "gears",
        gb.hack_path(ROOT, "gears").read_text(errors="surrogateescape"),
        ROOT,
        frames=gb.CALIBRATION_FRAMES,
        seed=gb.GEARS_SEEDS["heavy"],
    )
    fill_ms = result["counts"].pixels * gb.MODEL["pixel_ns"] / 1e6
    assert fill_ms == pytest.approx(40.0, rel=0.05)


@needs_cc
def test_model_predicts_light_gears():
    """Held out: nothing was fitted to this scene. The stop rule for a bad fit
    is 20 percent; the test holds it to 8."""
    assert gears_prediction("light") == pytest.approx(gb.LIGHT_GEARS_MS, rel=0.08)


@needs_cc
def test_a_hack_can_call_glpixelstorei_for_unpack_alignment():
    """cubicgrid does; TinyGL defines the call but does not declare it."""
    result = run(init="glPixelStorei(GL_UNPACK_ALIGNMENT, 1);")
    assert "error" not in result, result.get("error")


def coverage(draw: str, init: str = "") -> "gb.Coverage":
    return run(init, draw)["coverage"]


@needs_cc
def test_coverage_of_a_static_triangle_is_its_bounding_box():
    """A triangle from (-.5,-.5) to (.5,.5) covers a quarter of the canvas, and
    drawing the same one again means the previous box is the current box."""
    draw = f"glBegin(GL_TRIANGLES); {TRIANGLE} glEnd();"
    cov = coverage(draw)
    assert cov.bbox == pytest.approx(0.25, abs=0.03)
    assert cov.clear == pytest.approx(0.25, abs=0.03)
    assert cov.push == pytest.approx(0.25, abs=0.03)


@needs_cc
def test_a_moving_triangle_pushes_both_its_old_and_new_place():
    """The triangle sits left on even frames and right on odd ones. Each box is
    about a twelfth of the canvas; the push covers both."""
    draw = (
        "static int n; float dx = (n++ & 1) ? 0.45f : -0.45f;"
        "glBegin(GL_TRIANGLES);"
        "glVertex3f(dx-.2f,-.2f,0); glVertex3f(dx+.2f,-.2f,0);"
        "glVertex3f(dx,.2f,0); glEnd();"
    )
    cov = coverage(draw)
    assert cov.bbox == pytest.approx(0.04, abs=0.02)
    assert cov.push == pytest.approx(2 * cov.bbox, abs=0.02)


@needs_cc
def test_a_full_screen_triangle_covers_everything():
    draw = (
        "glBegin(GL_TRIANGLES); glVertex3f(-1,-1,0); glVertex3f(3,-1,0);"
        "glVertex3f(-1,3,0); glEnd();"
    )
    assert coverage(draw).bbox == pytest.approx(1.0, abs=0.02)


@needs_cc
def test_a_hack_that_draws_nothing_has_empty_coverage():
    cov = coverage("")
    assert (cov.bbox, cov.clear, cov.push) == (0, 0, 0)


@needs_cc
def test_a_point_covers_what_the_firmware_gives_it():
    """glPointSize(5) at the centre: the firmware's box is the point's size plus
    one pixel each side, 13 pixels square (glshim_note_box)."""
    draw = "glPointSize(5); glBegin(GL_POINTS); glVertex3f(0, 0, 0); glEnd();"
    cov = coverage(draw)
    assert cov.bbox == pytest.approx(13 * 13 / (466 * 466), rel=0.05)


@needs_cc
def test_a_triangle_wholly_off_screen_covers_nothing():
    draw = (
        "glBegin(GL_TRIANGLES); glVertex3f(3,3,0); glVertex3f(4,3,0);"
        "glVertex3f(3,4,0); glEnd();"
    )
    assert coverage(draw).bbox == 0


@needs_cc
def test_a_line_has_the_same_box_whichever_way_it_runs():
    def line(a, b):
        return f"glBegin(GL_LINES); glVertex3f({a}); glVertex3f({b}); glEnd();"

    forward = coverage(line("-.5f,-.5f,0", ".5f,.5f,0"))
    backward = coverage(line(".5f,.5f,0", "-.5f,-.5f,0"))
    assert forward.bbox == backward.bbox > 0


@needs_cc
def test_a_clipped_line_is_assumed_to_reach_anywhere():
    draw = "glBegin(GL_LINES); glVertex3f(-3,0,0); glVertex3f(3,0,0); glEnd();"
    assert coverage(draw).bbox == pytest.approx(1.0, abs=0.01)
