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
