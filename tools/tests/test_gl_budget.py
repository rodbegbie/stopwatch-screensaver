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
