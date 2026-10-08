import sys
from pathlib import Path

import pytest

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


SHIM_INCLUDES = [
    Path(__file__).resolve().parents[2] / "firmware" / "src",
    Path(__file__).resolve().parents[2] / "firmware" / "src" / "x11shim" / "include",
]


def test_blockers_clean_source_has_none():
    assert sh.blockers_for("ok", "int f(void) { return 0; }\n", SHIM_INCLUDES) == []


def test_blockers_reports_missing_header_and_keeps_going():
    src = '#include "nosuch.h"\nint f(void) { return GXnothing; }\n'
    got = sh.blockers_for("hdr", src, SHIM_INCLUDES)
    assert "nosuch.h" in got
    assert "GXnothing" in got


def test_blockers_reports_undeclared_function_type_and_member():
    src = (
        '#include "screenhack.h"\n'
        "void f(Display *d) { XNotARealCall(d); XWindowAttributes a; a.bogus = 1;"
        " NotAType t; }\n"
    )
    got = sh.blockers_for("misc", src, SHIM_INCLUDES)
    assert "XNotARealCall" in got
    assert "NotAType" in got
    assert "XWindowAttributes.bogus" in got


def test_blockers_define_standalone_like_the_real_build():
    src = (
        "#ifdef STANDALONE\nint standalone_set;\n"
        '#else\n#include "xlock.h"\n#endif\n'
    )
    assert sh.blockers_for("sa", src, SHIM_INCLUDES) == []


def test_ported_xlockmore_hack_is_clean_under_the_default_includes():
    root = Path(__file__).resolve().parents[2]
    src = (root / "firmware/src/hacks/hopalong/hopalong.c").read_text()
    includes = sh.default_includes(root)
    assert sh.blockers_for("hopalong", src, includes) == []


def test_blockers_pyro_like_hack_compiles_clean_against_shim():
    pyro = Path(__file__).resolve().parents[2] / "firmware/src/hacks/pyro/pyro.c"
    assert sh.blockers_for("pyro", pyro.read_text(), SHIM_INCLUDES) == []


def test_score_with_blockers_is_not_S_even_if_x_calls_provided():
    src = "XCreateGC(a,b,c,d);"
    row = sh.score(Path("hacks/a.c"), src, PROVIDED, blockers=["erase.h"])
    assert row["effort"] == "M"
    assert row["gaps"] == ["erase.h"]
    clean = sh.score(Path("hacks/a.c"), src, PROVIDED, blockers=[])
    assert clean["effort"] == "S"


def test_score_five_or_more_gaps_is_L():
    row = sh.score(Path("hacks/a.c"), "int x;", PROVIDED, blockers=list("abcde"))
    assert row["effort"] == "L"


def test_score_gl_hack_is_XL_and_not_compiled():
    row = sh.score(Path("hacks/glx/a.c"), "int x;", PROVIDED, blockers=None)
    assert row["effort"] == "XL"


def test_blockers_anonymous_struct_member_has_clean_name_without_paths():
    src = (
        '#include "screenhack.h"\n'
        "int f(XEvent *e) { return e->xbutton.nosuch; }\n"
    )
    got = sh.blockers_for("anon", src, SHIM_INCLUDES)
    assert "XEvent.nosuch" in got
    assert not any("unnamed" in g or "/" in g for g in got)


def test_is_hack_detects_module_entry_point():
    assert sh.is_hack('XSCREENSAVER_MODULE ("Pyro", pyro)\n')
    assert sh.is_hack('XSCREENSAVER_MODULE_2 ("X", x, y)\n')
    assert not sh.is_hack("int helper(void) { return 0; }\n")


def test_is_hack_ignores_comments_and_strings():
    assert not sh.is_hack("/* XSCREENSAVER_MODULE (a, b) */\nint x;\n")
    assert not sh.is_hack('const char *s = "XSCREENSAVER_MODULE (a, b)";\n')


def test_scan_excludes_non_hacks_and_reports_them(tmp_path):
    hacks = tmp_path / "xscreensaver-9.9" / "hacks"
    (hacks / "glx").mkdir(parents=True)
    (hacks / "real.c").write_text('XSCREENSAVER_MODULE ("Real", real)\n')
    (hacks / "helper.c").write_text("int helper(void) { return 0; }\n")
    (hacks / "glx" / "model.c").write_text("int model;\n")
    (hacks / "glx" / "gl_hack.c").write_text('XSCREENSAVER_MODULE ("G", g)\nglBegin(0);\n')
    rows, excluded = sh.scan(tmp_path / "xscreensaver-9.9", set(), SHIM_INCLUDES)
    assert sorted(r["name"] for r in rows) == ["gl_hack", "real"]
    assert excluded == ["helper", "model"]


def test_render_excluded_wraps_names_in_a_paragraph():
    names = [f"helper{i}" for i in range(40)]
    text = sh.render_excluded(names)
    assert all(len(line) <= 78 for line in text.splitlines())
    assert "helper0" in text and "helper39" in text
    assert text.endswith("\n") and not text.endswith("\n\n")


def test_blockers_keep_unrecognised_compile_errors():
    src = '#include "screenhack.h"\nvoid f(Display *d) { XDrawLine(d); }\n'
    got = sh.blockers_for("badcall", src, SHIM_INCLUDES)
    assert any("too few arguments" in b for b in got)


def test_score_is_never_S_after_a_failed_compile():
    src = '#include "screenhack.h"\nvoid f(Display *d) { XDrawLine(d); }\n'
    blockers = sh.blockers_for("badcall", src, SHIM_INCLUDES)
    row = sh.score(Path("hacks/badcall.c"), src, PROVIDED | {"XDrawLine"}, blockers)
    assert row["effort"] != "S"


def test_blockers_failed_compile_without_diagnostics_is_a_blocker():
    got = sh.blockers_for("quiet", "int x;\n", SHIM_INCLUDES, cc="false")
    assert got == ["compiler exited 1 with no diagnostics"]


def test_blockers_missing_compiler_is_a_clear_error():
    with pytest.raises(sh.ScoreError, match="not-a-real-compiler"):
        sh.blockers_for("x", "int x;\n", SHIM_INCLUDES, cc="not-a-real-compiler")


def test_find_vendor_picks_highest_version(tmp_path):
    for v in ("6.9", "6.16", "6.2"):
        (tmp_path / "vendor" / f"xscreensaver-{v}").mkdir(parents=True)
    assert sh.find_vendor(tmp_path).name == "xscreensaver-6.16"


def test_find_vendor_without_source_points_at_the_fetch_script(tmp_path):
    with pytest.raises(sh.ScoreError, match="fetch_xscreensaver"):
        sh.find_vendor(tmp_path)


def test_main_help_works_without_a_vendor_directory(tmp_path):
    with pytest.raises(SystemExit) as exc:
        sh.main(["--help"], root=tmp_path)
    assert exc.value.code == 0


def test_main_explicit_vendor_works_without_a_default_one(tmp_path, capsys):
    vendor = tmp_path / "ext" / "xscreensaver-9.9"
    (vendor / "hacks").mkdir(parents=True)
    (vendor / "hacks" / "demo.c").write_text('XSCREENSAVER_MODULE ("Demo", demo)\n')
    header = tmp_path / "xshim.h"
    header.write_text("int XDrawLine(int);\n")
    intro = tmp_path / "intro.md"
    intro.write_text("# T\n\n{version} {total} {excluded} {n_s} {n_m} {n_l} {n_xl}\n")
    registry = tmp_path / "registry.c"
    registry.write_text("const HackEntry *const g_hacks[] = {&demo_hack};\n")
    out = tmp_path / "out.md"
    argv = ["--vendor", str(vendor), "--header", str(header), "--intro", str(intro),
            "--registry", str(registry), "--out", str(out)]
    for d in SHIM_INCLUDES:
        argv += ["--shim-include", str(d)]
    assert sh.main(argv, root=tmp_path) == 0
    assert "demo" in out.read_text()


def test_main_missing_vendor_is_an_actionable_error(tmp_path, capsys):
    assert sh.main([], root=tmp_path) == 1
    assert "fetch_xscreensaver" in capsys.readouterr().err


def test_table_renders_gaps_as_code_and_escapes_pipes():
    row = sh.score(
        Path("hacks/a.c"), "int x;", PROVIDED, blockers=["error: unknown type 'Display *'", "a|b"]
    )
    table = sh.render_table([row])
    data_line = table.strip().splitlines()[-1]
    assert "`error: unknown type 'Display *'`" in data_line
    assert "`a\\|b`" in data_line
    assert data_line.replace("\\|", "").count("|") == 8


def test_ranking_renders_gaps_as_code():
    text = sh.render_ranking([("Display *", 1.0, 2)])
    assert "| `Display *` |" in text


REGISTRY = """
extern const HackEntry pyro_hack;
XLOCKMORE_HACK(hopalong, "Hopalong");
XLOCKMORE_HACK_WITH(galaxy, "Galaxy", kGalaxyOverrides);
/* old list: g_hacks[] = { &ghost_hack }; */
const HackEntry *const g_hacks[] = {
    &pyro_hack,     &hopalong_hack,
    &galaxy_hack};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
"""


def test_registered_hacks_are_the_entries_of_g_hacks():
    assert sh.registered_hacks(REGISTRY) == {"pyro", "hopalong", "galaxy"}


def test_registered_hacks_without_the_array_is_an_error():
    with pytest.raises(sh.ScoreError, match="g_hacks"):
        sh.registered_hacks("int x;")


def test_failed_ports_reads_names_and_reasons_and_skips_comments():
    text = "# comment\n\nmaze: needs XCopyArea\nfoo:no space\n"
    assert sh.read_failed_ports(text) == {"maze": "needs XCopyArea", "foo": "no space"}


def test_failed_port_line_without_a_colon_is_an_error():
    with pytest.raises(sh.ScoreError, match="failed_ports"):
        sh.read_failed_ports("maze\n")


def test_a_hack_cannot_be_both_ported_and_failed():
    with pytest.raises(sh.ScoreError, match="pyro"):
        sh.check_ports({"pyro"}, {"pyro": "x"}, {"pyro", "maze"})


def test_a_failed_port_must_name_a_scanned_hack():
    with pytest.raises(sh.ScoreError, match="typo"):
        sh.check_ports(set(), {"typo": "x"}, {"pyro"})


def test_a_registered_name_that_was_not_scanned_is_an_error():
    with pytest.raises(sh.ScoreError, match="ghost"):
        sh.check_ports({"ghost"}, {}, {"pyro"})


def _row(name, effort):
    return {"name": name, "kind": "2d", "effort": effort, "missing": [], "gaps": [],
            "flags": [], "loc": 1}


def test_table_sorts_by_effort_size_then_name():
    rows = [_row("b", "XL"), _row("z", "S"), _row("c", "L"), _row("a", "L"),
            _row("m", "M"), _row("a2", "S")]
    lines = sh.render_table(rows).splitlines()[2:]
    assert [line.split("|")[1].strip() for line in lines] == [
        "a2", "z", "m", "a", "c", "b"]


def test_table_has_a_ported_column_with_ticks_crosses_and_dashes():
    rows = [_row("pyro", "S"), _row("maze", "L"), _row("other", "L")]
    table = sh.render_table(rows, ported={"pyro"}, failed={"maze"})
    header, _, *data = table.splitlines()
    cells = [c.strip() for c in header.strip("|").split("|")]
    col = cells.index("Ported")
    got = {line.split("|")[1].strip(): line.split("|")[col + 1].strip() for line in data}
    assert got == {"pyro": "\u2705", "maze": "\u274c", "other": "-"}


def test_failed_section_is_empty_when_nothing_failed():
    assert sh.render_failed({}) == ""


def test_failed_section_lists_each_hack_with_its_reason():
    text = sh.render_failed({"maze": "needs XCopyArea", "a": "slow"})
    assert text.startswith("\n## Failed ports\n\n")
    assert "- **a**: slow\n- **maze**: needs XCopyArea\n" in text
    assert text.endswith("\n") and not text.endswith("\n\n")


def test_main_marks_registered_hacks_as_ported(tmp_path):
    vendor = tmp_path / "ext" / "xscreensaver-9.9"
    (vendor / "hacks").mkdir(parents=True)
    for name in ("demo", "other"):
        (vendor / "hacks" / f"{name}.c").write_text(f'XSCREENSAVER_MODULE ("{name}", {name})\n')
    header = tmp_path / "xshim.h"
    header.write_text("int XDrawLine(int);\n")
    intro = tmp_path / "intro.md"
    intro.write_text("# T\n\n{version} {total} {excluded} {n_s} {n_m} {n_l} {n_xl}\n")
    registry = tmp_path / "registry.c"
    registry.write_text("const HackEntry *const g_hacks[] = {&demo_hack};\n")
    failed = tmp_path / "failed.txt"
    failed.write_text("other: would not run\n")
    out = tmp_path / "out.md"
    argv = ["--vendor", str(vendor), "--header", str(header), "--intro", str(intro),
            "--registry", str(registry), "--failed-ports", str(failed), "--out", str(out)]
    for d in SHIM_INCLUDES:
        argv += ["--shim-include", str(d)]
    assert sh.main(argv, root=tmp_path) == 0
    text = out.read_text()
    assert "| demo |" in text and "\u2705" in text and "\u274c" in text
    assert "- **other**: would not run" in text


def test_main_missing_registry_is_an_actionable_error(tmp_path, capsys):
    vendor = tmp_path / "xscreensaver-9.9"
    (vendor / "hacks").mkdir(parents=True)
    header = tmp_path / "xshim.h"
    header.write_text("int XDrawLine(int);\n")
    argv = ["--vendor", str(vendor), "--header", str(header)]
    assert sh.main(argv, root=tmp_path) == 1
    assert "--registry" in capsys.readouterr().err


def _cells(table):
    header, _, *data = table.splitlines()
    names = [c.strip() for c in header.strip("|").split("|")]
    return [dict(zip(names, (c.strip() for c in line.strip("|").split("|")))) for line in data]


def test_a_ported_hack_has_no_effort_rating():
    rows = [_row("pyro", "S"), _row("todo", "S")]
    got = {c["Hack"]: c["Effort"] for c in _cells(sh.render_table(rows, ported={"pyro"}))}
    assert got == {"pyro": "-", "todo": "S"}


def test_ported_hacks_sort_after_every_unported_one():
    rows = [_row("pyro", "S"), _row("big", "XL"), _row("a", "L"), _row("b", "S")]
    order = [c["Hack"] for c in _cells(sh.render_table(rows, ported={"pyro"}))]
    assert order == ["b", "a", "big", "pyro"]


def test_a_failed_hack_keeps_its_effort_rating_and_its_place():
    rows = [_row("maze", "L"), _row("a", "S"), _row("z", "XL")]
    cells = _cells(sh.render_table(rows, failed={"maze"}))
    assert [(c["Hack"], c["Effort"]) for c in cells] == [("a", "S"), ("maze", "L"), ("z", "XL")]


def test_main_counts_only_unported_hacks_per_rating(tmp_path):
    vendor = tmp_path / "xscreensaver-9.9"
    (vendor / "hacks").mkdir(parents=True)
    for name in ("done", "todo"):
        (vendor / "hacks" / f"{name}.c").write_text(f'XSCREENSAVER_MODULE ("{name}", {name})\n')
    header = tmp_path / "xshim.h"
    header.write_text("int XDrawLine(int);\n")
    intro = tmp_path / "intro.md"
    intro.write_text("# T\n\ncounts={n_s} {n_m} {n_l} {n_xl} ported={n_ported}\n")
    registry = tmp_path / "registry.c"
    registry.write_text("const HackEntry *const g_hacks[] = {&done_hack};\n")
    out = tmp_path / "out.md"
    argv = ["--vendor", str(vendor), "--header", str(header), "--intro", str(intro),
            "--registry", str(registry), "--out", str(out)]
    for d in SHIM_INCLUDES:
        argv += ["--shim-include", str(d)]
    assert sh.main(argv, root=tmp_path) == 0
    line = next(l for l in out.read_text().splitlines() if l.startswith("counts="))
    counts, ported = line.removeprefix("counts=").split(" ported=")
    assert sum(int(n) for n in counts.split()) == 1
    assert ported == "1"
