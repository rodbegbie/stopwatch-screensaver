import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import score_hacks as sh  # noqa: E402

HEADER = """
int XCreateGC(Display *, Drawable, unsigned long, XGCValues *);
int XFillRectangle(Display *, Drawable, GC, int, int, unsigned, unsigned);
int XDrawLine (Display *, Drawable, GC, int, int, int, int);
typedef struct XColor XColor;
#define XSCREENSAVER_MODULE(a, b) x
"""
PROVIDED = {"XCreateGC", "XFillRectangle", "XDrawLine"}


def test_used_calls_ignores_comments_and_strings():
    src = '''
    /* XGetImage(dpy) in a comment */
    // XCopyArea( here too
    const char *s = "XPutPixel(x)";
    XFillRectangle (dpy, w, gc, 0, 0, 1, 1);
    XDrawLine(dpy, w, gc, 0, 0, 1, 1);
    '''
    assert sh.used_calls(src) == {"XFillRectangle", "XDrawLine"}


def test_implemented_calls_reads_header():
    assert sh.implemented_calls(HEADER) == PROVIDED


def test_classify_gl_by_path_and_by_identifier():
    assert sh.classify(Path("hacks/glx/gears.c"), "int x;") == "gl"
    assert sh.classify(Path("hacks/foo.c"), "void f(){ glBegin(GL_LINES); }") == "gl"
    assert sh.classify(Path("hacks/foo.c"), "XDrawLine(a,b,c,d,e,f,g);") == "2d"


def test_score_pyro_like_source_is_S():
    src = "XCreateGC(a,b,c,d); XFillRectangle(a,b,c,d,e,f,g);"
    row = sh.score(Path("hacks/pyro.c"), src, PROVIDED)
    assert row["name"] == "pyro"
    assert row["kind"] == "2d"
    assert row["missing"] == []
    assert row["effort"] == "S"


def test_score_flags_readback_pixmaps_xor():
    src = "XGetImage(a); XCreatePixmap(a); XSetFunction(a); XDrawString(a);"
    row = sh.score(Path("hacks/x.c"), src, PROVIDED)
    assert {"readback", "pixmaps", "xor", "text"} <= set(row["flags"])


def test_score_effort_M_L_XL():
    m = sh.score(Path("hacks/a.c"), "XDrawArc(a); XDrawLine(a);", PROVIDED)
    assert m["effort"] == "M" and m["missing"] == ["XDrawArc"]
    many = "XA1b(); XA2b(); XA3b(); XA4b(); XA5b();"
    assert sh.score(Path("hacks/b.c"), many, PROVIDED)["effort"] == "L"
    assert sh.score(Path("hacks/c.c"), "XCreatePixmap(a);", PROVIDED)["effort"] == "L"
    gl = sh.score(Path("hacks/glx/d.c"), "XDrawLine(a);", PROVIDED)
    assert gl["effort"] == "XL"


def test_score_float_heavy_and_xlockmore_flags():
    src = '#include "xlockmore.h"\n' + "x = sin(a) + cos(b);\n" * 10
    row = sh.score(Path("hacks/e.c"), src, PROVIDED)
    assert "float-heavy" in row["flags"]
    assert "needs-xlockmore" in row["flags"]


def test_rank_missing_prefers_calls_blocking_nearly_ready_hacks():
    rows = [
        {"kind": "2d", "missing": ["XA"]},
        {"kind": "2d", "missing": ["XA", "XB"]},
        {"kind": "2d", "missing": ["XB", "XC", "XD"]},
        {"kind": "gl", "missing": ["XA"]},
    ]
    ranked = sh.rank_missing(rows, 3)
    assert [r[0] for r in ranked][0] == "XA"
    assert ranked[0][2] == 2


def test_table_output_is_lint_clean_markdown():
    rows = [sh.score(Path("hacks/pyro.c"), "XCreateGC(a);", PROVIDED)]
    md = sh.render_table(rows)
    assert md.startswith("| Hack |")
    assert md.endswith("\n") and not md.endswith("\n\n")
    assert all(line.startswith("|") for line in md.strip().splitlines())
