import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import float_literals as fl  # noqa: E402
import tinygl_patch as tp  # noqa: E402

TINYGL = Path(__file__).resolve().parents[2] / "firmware" / "src" / "tinygl"


def load_sources() -> dict[str, str]:
    """The committed, byte-identical copy, keyed as the build script keys it."""
    return {
        str(p.relative_to(TINYGL)): p.read_text(errors="surrogateescape")
        for sub in ("src", "include")
        for p in sorted((TINYGL / sub).rglob("*"))
        if p.is_file()
    }


@pytest.fixture(scope="module")
def patched() -> dict[str, str]:
    return tp.patch_tree(load_sources())


def test_patch_tree_applies_every_listed_change(patched):
    features = patched["include/zfeatures.h"]
    assert "TGL_FEATURE_16_BITS        1" in features
    assert "TGL_FEATURE_32_BITS        0" in features
    assert "TGL_FEATURE_CUSTOM_MALLOC 1" in features
    assert "TGL_SWAP16" in patched["include/zbuffer.h"]
    zbuffer = patched["src/zbuffer.c"]
    assert "xsize & ~3" not in zbuffer
    assert not re.search(r"(?<!tgl_)memset_s", zbuffer)
    assert "for (i = 1; i < MAX_DISPLAY_LISTS; i++)" in patched["src/list.c"]
    assert "#define OP_BUFFER_MAX_SIZE 64" in patched["src/zgl.h"]
    assert '#include "tinygl_single.h"' in patched["src/zgl.h"]
    assert '#include "tinygl_single.h"' in patched["src/zmath.h"]
    memory = patched["src/memory.c"]
    assert "heap_caps_malloc" in memory and "192" in memory
    single = patched["src/tinygl_single.h"]
    for name in ("sqrt", "pow", "sin", "cos", "floor"):
        assert f"#define {name} {name}f" in single


@pytest.mark.parametrize("path", tp.FLOAT_LITERAL_FILES)
def test_float_literals_are_applied_to_the_listed_files(patched, path):
    # float_literals raises when it finds nothing left to suffix.
    with pytest.raises(ValueError, match="no floating-point literals"):
        fl.float_literals(patched[path])


@pytest.mark.parametrize("path", tp.FLOAT_LITERAL_FILES)
def test_every_listed_file_has_literals_upstream(path):
    fl.float_literals(load_sources()[path])


def anchor_id(rep: "tp.Replace") -> str:
    return f"{rep.path}:{rep.anchor[:24]!r}"


@pytest.mark.parametrize("rep", tp.REPLACEMENTS, ids=anchor_id)
def test_a_missing_anchor_names_upstream_as_the_problem(rep):
    sources = load_sources()
    sources[rep.path] = sources[rep.path].replace(rep.anchor, "")
    with pytest.raises(ValueError, match="has upstream changed"):
        tp.patch_tree(sources)


@pytest.mark.parametrize("rep", tp.REPLACEMENTS, ids=anchor_id)
def test_an_extra_copy_of_an_anchor_is_refused(rep):
    sources = load_sources()
    sources[rep.path] += "\n" + rep.anchor
    with pytest.raises(ValueError, match="has upstream changed"):
        tp.patch_tree(sources)


def test_a_changed_memory_c_is_refused():
    sources = load_sources()
    sources["src/memory.c"] += "/* upstream added something */\n"
    with pytest.raises(ValueError, match="has upstream changed"):
        tp.patch_tree(sources)


def test_a_missing_file_is_refused():
    sources = load_sources()
    del sources["src/list.c"]
    with pytest.raises(ValueError, match="has upstream changed"):
        tp.patch_tree(sources)


def test_unity_source_lists_c_files_sorted():
    text = tp.unity_source(["src/zgl.h", "src/b.c", "src/a.c", "include/GL/gl.h"])
    assert text == '#include "tinygl/src/a.c"\n#include "tinygl/src/b.c"\n'


def test_light_model_reads_only_as_many_values_as_the_parameter_has(patched):
    """Two-sided lighting takes one value; Morph3D passes a one-element array."""
    api = patched["src/api.c"]
    body = api[api.index("void glLightModelfv") : api.index("/* clear */")]
    assert "pname == GL_LIGHT_MODEL_AMBIENT ? 4 : 1" in body
    assert "i < n ? param[i] : 0" in body


def test_clip_epsilon_is_a_float_literal(patched):
    """`1E-5` is a double, so `w1 * (1.0f + CLIP_EPSILON)` ran a software
    double multiply and two conversions for every vertex."""
    zgl = patched["src/zgl.h"]
    assert "#define CLIP_EPSILON (1E-5f)" in zgl
    assert "#define CLIP_EPSILON (1E-5)\n" not in zgl


def test_every_writer_of_the_canvas_reports_its_box(patched):
    """The dirty rectangle must see everything TinyGL draws: triangles, lines,
    points, text, and glDrawPixels (which cannot run on a 64-bit host)."""
    assert "glshim_note_box(0, 0, 100000, 100000);" in patched["src/zraster.c"]
    assert "glshim_note_box(x % c->zb->xsize" in patched["src/ztext.c"]
    clip = patched["src/clip.c"]
    for site in ("glshim_note_triangle(p0, p1, p2);", "glshim_note_box(lx - 1, ly - 1"):
        assert site in clip


def test_the_line_box_is_ordered_before_its_margin_is_added(patched):
    """p1 can be right of or below p2: `p1.x - 1` to `p2.x + 1` then shrinks the
    box by two pixels instead of growing it."""
    clip = patched["src/clip.c"]
    line = clip[clip.index("void gl_draw_line") :]
    line = line[: line.index("}\n\telse") if "}\n\telse" in line else 600]
    assert "p1->zp.x < p2->zp.x ? p1->zp.x : p2->zp.x" in line
    assert "p1->zp.x - 1" not in line


def test_both_viewport_transforms_keep_the_fractional_position(patched):
    """Smooth lines need where a vertex really is, not the pixel it truncates
    to. A transform that forgets it leaves fx and fy stale."""
    for path in ("src/clip.c", "src/vertex.c"):
        source = patched[path]
        assert "v->zp.fx = v->pc.X * winv" in source, path
        assert "v->zp.fy = v->pc.Y * winv" in source, path
    assert "GLfloat fx,fy;" in patched["include/zbuffer.h"]


def test_gl_line_smooth_switches_smooth_lines_in_both_line_routines(patched):
    assert "case GL_LINE_SMOOTH:" in patched["src/misc.c"]
    assert "c->zb->line_smooth = 0;" in patched["src/init.c"]
    assert "GLint line_smooth;" in patched["include/zbuffer.h"]
    zline = patched["src/zline.c"]
    assert "if (zb->line_smooth && ZB_line_aa(zb, p1, p2, 1)) return;" in zline
    assert "if (zb->line_smooth && ZB_line_aa(zb, p1, p2, 0)) return;" in zline


def test_a_smooth_line_stays_inside_the_dirty_rectangle_margin(patched):
    """A smooth line shares each step between the two nearest rows or columns,
    so it reaches one pixel past its endpoints' truncated box: exactly the
    margin the dirty rectangle adds."""
    clip = patched["src/clip.c"]
    assert "glshim_note_box(lx - 1, ly - 1, hx + 1, hy + 1);" in clip


def test_filled_triangles_apply_polygon_offset_and_treat_alpha_one_as_opaque(patched):
    """TinyGL records glPolygonOffset and nothing reads it, and its blend adds
    colours and decodes our byte-swapped pixels wrongly."""
    clip = patched["src/clip.c"]
    wrapper = clip[clip.index("void gl_draw_triangle_fill(") :]
    wrapper = wrapper[: wrapper.index("gl_draw_triangle_fill_plain(GLVertex* p0")]
    assert "c->offset_states & TGL_OFFSET_FILL" in wrapper
    assert "c->offset_factor * slope + c->offset_units" in wrapper
    assert "shift += shift < 0.0f ? -snap : snap;" in wrapper
    assert "if (shift > 536870912.0f) shift = 536870912.0f;" in wrapper
    assert "if (shift < -536870912.0f) shift = -536870912.0f;" in wrapper
    assert "snap = (c->offset_factor != 0.0f || c->offset_units != 0.0f)" in wrapper
    assert "z0 - bias > zmax ? zmax" in wrapper
    assert "p0->zp.z = z0;" in wrapper
    assert "GL_SRC_ALPHA" in wrapper and "GL_ONE_MINUS_SRC_ALPHA" in wrapper
    assert "c->zb->enable_blend = blend;" in wrapper


def hot_id(hot: "tp.Hot") -> str:
    return f"{hot.path}:{hot.definition.split('(')[0].split()[-1]}"


@pytest.mark.parametrize("hot", tp.HOT_FUNCTIONS, ids=hot_id)
def test_every_hot_function_is_marked_and_timed_if_it_has_a_slot(patched, hot):
    """GL_IRAM moves exactly these functions; GL_PROFILE times the ones with
    a slot from their first line."""
    text = patched[hot.path]
    assert text.count("TGL_HOT " + hot.anchor) == 1
    if hot.slot:
        scope = "GLPROF_SCOPE_OUTER" if hot.reentrant else "GLPROF_SCOPE"
        assert f"TGL_HOT {hot.anchor}\t{scope}({hot.slot});\n" in text


@pytest.mark.parametrize("hot", tp.HOT_FUNCTIONS, ids=hot_id)
def test_a_missing_hot_function_names_upstream_as_the_problem(hot):
    sources = load_sources()
    patched_once = tp.patch_tree(sources)
    assert hot.anchor not in patched_once[hot.path].replace("TGL_HOT " + hot.anchor, "")
    broken = dict(sources)
    upstream_anchor = hot.anchor if hot.anchor in sources[hot.path] else None
    if upstream_anchor is None:  # written by an earlier patch: break that one
        pytest.skip("anchor is written by FILL_TRIANGLE")
    broken[hot.path] = sources[hot.path].replace(
        hot.anchor, hot.anchor.replace("(", " (")
    )
    with pytest.raises(ValueError, match="has upstream changed"):
        tp.patch_tree(broken)


def test_only_the_list_call_is_timed_as_reentrant():
    """Display lists call lists; timing every level would count time twice."""
    assert [h.definition for h in tp.HOT_FUNCTIONS if h.reentrant] == [
        "void glopCallList(GLParam* p)"
    ]


def test_the_single_header_brings_in_the_switches(patched):
    assert '#include "glshim/glshim_prof.h"' in patched["src/tinygl_single.h"]


def late_id(rep: "tp.Replace") -> str:
    return f"{rep.path}:{rep.anchor[:28]!r}"


@pytest.mark.parametrize("rep", tp.LATE_REPLACEMENTS, ids=late_id)
def test_a_late_anchor_that_moved_is_refused(rep, monkeypatch):
    """Late anchors are written by earlier patches, so break them there."""
    monkeypatch.setattr(
        tp,
        "LATE_REPLACEMENTS",
        tuple(
            tp.Replace(r.path, r.anchor + "\x00", r.replacement, r.count)
            if r is rep
            else r
            for r in tp.LATE_REPLACEMENTS
        ),
    )
    with pytest.raises(ValueError, match="has upstream changed"):
        tp.patch_tree(load_sources())


def test_lazy_lighting_shades_before_every_draw(patched):
    """Every place a lit vertex reaches the rasteriser shades it first."""
    clip = patched["src/clip.c"]
    assert clip.count("GL_SHADE_NOW3(p0, p1, p2); glshim_note_triangle") == 4
    assert (
        "GL_SHADE_NOW3(p0, p1, p2);\n\t\t\tgl_draw_triangle_clip(p0, p1, p2, 0);"
        in clip
    )
    assert "GL_SHADE_NOW(p0);" in clip
    assert "GL_SHADE_NOW(p1);\n\tGL_SHADE_NOW(p2);" in clip


def test_clip_temporaries_are_never_shaded_again(patched):
    """The clipper's stack vertices are interpolated from shaded ones and go
    back through gl_draw_triangle; an uninitialised flag shaded them with the
    material (a host test caught it as half-bright fills, depending on what
    the stack held)."""
    clip = patched["src/clip.c"]
    tmp = clip[clip.index("static void updateTmp") :]
    assert "q->shade_pending = 0;" in tmp[: tmp.index("q->color.v[0]")]


@pytest.mark.parametrize(
    "op",
    [
        "glopMaterial",
        "glopLight",
        "glopLightModel",
        "glopSetEnableSpecular",
        "glopEnableDisable",
    ],
)
def test_lighting_state_changes_shade_what_is_pending_first(patched, op):
    text = patched["src/misc.c" if op == "glopEnableDisable" else "src/light.c"]
    body = text[text.index(f"void {op}(GLParam* p) {{") :]
    assert "GL_SHADE_FLUSH();" in body[:120]


def test_lazy_lighting_defers_the_eye_normal_but_keeps_the_original(patched):
    """The default build keeps upstream's normal transform word for word."""
    vertex = patched["src/vertex.c"]
    assert "#ifdef GL_LAZY_SHADE\n\t\tv->normal.X = c->current_normal.X;" in vertex
    assert tp.NORMAL_TRANSFORM in vertex
    assert "gl_eye_normal(v, &n);\n\tgl_shade_vertex(v);" in patched["src/light.c"]


def test_additive_blending_saturates_each_channel(patched):
    zbuffer = patched["include/zbuffer.h"]
    assert "static inline PIXEL glshim_add565(PIXEL dst, PIXEL src)" in zbuffer
    assert "GLint tgl_adds = TGL_BLEND_ADDS(sfactor, dfactor, zbblendeq);" in zbuffer
    assert "if (tgl_adds) { (dest) = glshim_add565((dest), (source)); } else" in zbuffer
    assert "if (tgl_adds) { (dest) = glshim_add565((dest), RGB_TO_PIXEL(" in zbuffer
