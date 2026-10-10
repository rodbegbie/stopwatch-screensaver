"""Patches that make TinyGL run on the StopWatch.

The copy in `firmware/src/tinygl/` stays byte-identical to upstream (the
C-Chads fork, commit 36a7987). The firmware build writes a patched tree into
the build directory with `firmware/patch_tinygl.py` and compiles that, as it
does for Maze and Pacman. Each patch checks what it replaces and fails loudly
if upstream has changed.

What the patches do, and why:

- 16-bit pixels, written byte-swapped: the canvas holds RGB565 with its bytes
  already swapped into display order, so TinyGL draws straight into it with no
  copy.
- A PSRAM-aware allocator: the z-buffer is 434 KB and the internal heap is
  325 KB.
- `glGenLists` starts at 1: hacks treat list name 0 as "invalid" and abort.
- `OP_BUFFER_MAX_SIZE` is 64, not 4096: every display list otherwise takes a
  16 KB buffer, and Pipes alone makes 500 of them.
- The buffer width is not rounded down to a multiple of 4: the canvas is 466.
- `memset_s` is renamed: macOS's `string.h` declares its own.
- A dirty rectangle: TinyGL reports the box around everything it draws to
  `src/glshim/glshim.c`, which clears only the box drawn since the last clear
  and marks only that box (and the one it replaced) for the display push.
- Single precision: the S3's FPU does only `float`, so `double` maths is
  software. Decimal literals get an `f` suffix (`float_literals.py`) and
  `sqrt`, `pow`, `sin`, `cos` and `floor` map to their `f` versions.
"""

import hashlib
from collections.abc import Iterable, Mapping
from dataclasses import dataclass

import float_literals

UPSTREAM_CHANGED = "has upstream changed?"


@dataclass(frozen=True)
class Replace:
    path: str
    anchor: str
    replacement: str
    count: int = 1


@dataclass(frozen=True)
class ReplaceFile:
    path: str
    sha256: str
    replacement: str


SWAPPED_PIXEL = """#elif TGL_FEATURE_RENDER_BITS == 16
/* The canvas keeps RGB565 with its bytes swapped (display order). */
#define TGL_SWAP16(v) ((((v) << 8) | ((v) >> 8)) & 0xFFFF)
#define RGB_TO_PIXEL(r,g,b) \\
\tTGL_SWAP16( COLOR_R_GET16(r) | COLOR_G_GET16(g) | COLOR_B_GET16(b)  )
#endif
"""

DIRTY_DECLARATIONS = """/* clip.c */

/* The dirty rectangle, kept in src/glshim/glshim.c. */
void glshim_note_box(GLint x0, GLint y0, GLint x1, GLint y1);
void glshim_clear(ZBuffer* zb, GLint clear_z, GLint z, GLint clear_color, GLint r, GLint g, GLint b);
"""

DIRTY_TRIANGLE_HELPER = """static void glshim_note_triangle(GLVertex* p0, GLVertex* p1, GLVertex* p2) {
	GLint lx = p0->zp.x < p1->zp.x ? p0->zp.x : p1->zp.x;
	GLint hx = p0->zp.x < p1->zp.x ? p1->zp.x : p0->zp.x;
	GLint ly = p0->zp.y < p1->zp.y ? p0->zp.y : p1->zp.y;
	GLint hy = p0->zp.y < p1->zp.y ? p1->zp.y : p0->zp.y;
	if (p2->zp.x < lx) lx = p2->zp.x;
	if (p2->zp.x > hx) hx = p2->zp.x;
	if (p2->zp.y < ly) ly = p2->zp.y;
	if (p2->zp.y > hy) hy = p2->zp.y;
	glshim_note_box(lx, ly, hx, hy);
}

"""

DIRTY_POINT = """void gl_draw_point(GLVertex* p0) {
	if (p0->clip_code == 0) {
		GLint m = (GLint)gl_get_context()->zb->pointsize + 1;
		glshim_note_box(p0->zp.x - m, p0->zp.y - m, p0->zp.x + m, p0->zp.y + m);
	}
"""

DIRTY_LINE = """void gl_draw_line(GLVertex* p1, GLVertex* p2) {
	if ((p1->clip_code | p2->clip_code) == 0) {
		GLint lx = p1->zp.x < p2->zp.x ? p1->zp.x : p2->zp.x;
		GLint hx = p1->zp.x < p2->zp.x ? p2->zp.x : p1->zp.x;
		GLint ly = p1->zp.y < p2->zp.y ? p1->zp.y : p2->zp.y;
		GLint hy = p1->zp.y < p2->zp.y ? p2->zp.y : p1->zp.y;
		glshim_note_box(lx - 1, ly - 1, hx + 1, hy + 1);
	} else { /* clipped: it may reach anywhere */
		glshim_note_box(0, 0, 100000, 100000);
	}
"""

REPLACEMENTS: tuple[Replace, ...] = (
    Replace(
        "include/zfeatures.h",
        "#define TGL_FEATURE_CUSTOM_MALLOC 0",
        "#define TGL_FEATURE_CUSTOM_MALLOC 1",
    ),
    Replace(
        "include/zfeatures.h",
        "#define TGL_FEATURE_16_BITS        0\n#define TGL_FEATURE_32_BITS        1",
        "#define TGL_FEATURE_16_BITS        1\n#define TGL_FEATURE_32_BITS        0",
    ),
    Replace(
        "include/zbuffer.h",
        "#elif TGL_FEATURE_RENDER_BITS == 16\n"
        "#define RGB_TO_PIXEL(r,g,b) \\\n"
        "\t( COLOR_R_GET16(r) | COLOR_G_GET16(g) | COLOR_B_GET16(b)  )\n"
        "#endif\n",
        SWAPPED_PIXEL,
    ),
    Replace("src/zbuffer.c", "zb->xsize = xsize & ~3;", "zb->xsize = xsize;"),
    Replace(
        "src/zbuffer.c",
        "\txsize = xsize & ~3;",
        "\t/* the width is not rounded down: see tools/tinygl_patch.py */",
    ),
    Replace("src/zbuffer.c", "memset_s(", "tgl_memset_s(", count=3),
    Replace(
        "src/list.c",
        "for (i = 0; i < MAX_DISPLAY_LISTS; i++) {",
        "for (i = 1; i < MAX_DISPLAY_LISTS; i++) {",
    ),
    Replace(
        "src/api.c",
        '\tGLint i;\n#include "error_check_no_context.h"\n'
        "\tp[0].op = OP_LightModel;\n\tp[1].i = pname;\n"
        "\tfor (i = 0; i < 4; i++)\n\t\tp[2 + i].f = param[i];\n",
        "\tGLint i;\n"
        "\t/* Only the ambient colour has four values; the other parameters have\n"
        "\t * one, and hacks pass a one-element array. */\n"
        "\tGLint n = pname == GL_LIGHT_MODEL_AMBIENT ? 4 : 1;\n"
        '#include "error_check_no_context.h"\n'
        "\tp[0].op = OP_LightModel;\n\tp[1].i = pname;\n"
        "\tfor (i = 0; i < 4; i++)\n\t\tp[2 + i].f = i < n ? param[i] : 0;\n",
    ),
    Replace(
        "src/zgl.h",
        "#define CLIP_EPSILON (1E-5)",
        "#define CLIP_EPSILON (1E-5f)",
    ),
    Replace("src/zgl.h", "/* clip.c */\n", DIRTY_DECLARATIONS),
    Replace(
        "src/clip.c",
        "void gl_draw_point(GLVertex* p0) {\n",
        DIRTY_TRIANGLE_HELPER + DIRTY_POINT,
    ),
    Replace(
        "src/clip.c", "void gl_draw_line(GLVertex* p1, GLVertex* p2) {\n", DIRTY_LINE
    ),
    Replace(
        "src/clip.c",
        "c->draw_triangle_front(p0, p1, p2);",
        "{ glshim_note_triangle(p0, p1, p2); c->draw_triangle_front(p0, p1, p2); }",
        count=2,
    ),
    Replace(
        "src/clip.c",
        "c->draw_triangle_back(p0, p1, p2);",
        "{ glshim_note_triangle(p0, p1, p2); c->draw_triangle_back(p0, p1, p2); }",
        count=2,
    ),
    Replace(
        "src/zraster.c",
        "void glopDrawPixels(GLParam* p) {\n",
        "void glopDrawPixels(GLParam* p) {\n\tglshim_note_box(0, 0, 100000, 100000);\n",
    ),
    Replace(
        "src/ztext.c",
        "\tc->zb->pbuf[x] = pix;\n",
        "\tglshim_note_box(x % c->zb->xsize, x / c->zb->xsize, x % c->zb->xsize,\n\t\t\t\tx / c->zb->xsize);\n\tc->zb->pbuf[x] = pix;\n",
    ),
    Replace(
        "src/clear.c",
        "ZB_clear(c->zb, mask & GL_DEPTH_BUFFER_BIT,",
        "glshim_clear(c->zb, mask & GL_DEPTH_BUFFER_BIT,",
    ),
    Replace(
        "src/zgl.h",
        "#define OP_BUFFER_MAX_SIZE 4096",
        "#define OP_BUFFER_MAX_SIZE 64",
    ),
    Replace(
        "src/zgl.h",
        "#include <math.h>\n#include <stdlib.h>",
        '#include <math.h>\n#include "tinygl_single.h"\n#include <stdlib.h>',
    ),
    Replace(
        "src/zmath.h",
        "#include <math.h>\n#include <stdlib.h>",
        '#include <math.h>\n#include "tinygl_single.h"\n#include <stdlib.h>',
    ),
)

# Files with unsuffixed decimal literals, which are `double` in C.
FLOAT_LITERAL_FILES: tuple[str, ...] = (
    "src/clip.c",
    "src/init.c",
    "src/light.c",
    "src/matrix.c",
    "src/vertex.c",
    "src/zgl.h",
    "src/zmath.c",
    "src/zmath.h",
    "src/zraster.c",
    "src/ztriangle.c",
    "src/ztriangle.h",
)

MEMORY_C = """/*
 * Memory allocator for TinyGL on the StopWatch (see tools/tinygl_patch.py).
 * Allocations of 192 bytes or more go to PSRAM on the board: the z-buffer,
 * display list buffers and context state are large, and the internal heap is
 * 325 KB. Smaller ones stay in fast internal RAM.
 */

#include "zgl.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

void gl_free(void* p) { free(p); }

void* gl_malloc(GLint size) {
#ifdef ESP_PLATFORM
	if (size >= 192) return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#endif
	return malloc(size);
}

void* gl_zalloc(GLint size) {
	void* p = gl_malloc(size);
	if (p) memset(p, 0, size);
	return p;
}
"""

REPLACE_FILES: tuple[ReplaceFile, ...] = (
    ReplaceFile(
        "src/memory.c",
        "44acbbc1083a8b5c031d919f27589ab13f03390cf745c8e2dba966f074399b20",
        MEMORY_C,
    ),
)

SINGLE_HEADER_PATH = "src/tinygl_single.h"
SINGLE_HEADER = """#ifndef TINYGL_SINGLE_H
#define TINYGL_SINGLE_H
/* The S3's FPU is single-precision only, so double libm calls are software.
 * Route TinyGL's calls to the float versions. */
#include <math.h>
#define sqrt sqrtf
#define pow powf
#define sin sinf
#define cos cosf
#define floor floorf
#endif
"""


def _get(sources: dict[str, str], path: str) -> str:
    if path not in sources:
        raise ValueError(f"{path} is missing; {UPSTREAM_CHANGED}")
    return sources[path]


def patch_tree(sources: Mapping[str, str]) -> dict[str, str]:
    """Returns every file in `sources` (paths relative to firmware/src/tinygl),
    patched, plus the new `src/tinygl_single.h`."""
    out = dict(sources)
    for rf in REPLACE_FILES:
        text = _get(out, rf.path)
        digest = hashlib.sha256(text.encode(errors="surrogateescape")).hexdigest()
        if digest != rf.sha256:
            raise ValueError(f"{rf.path} is not the expected file; {UPSTREAM_CHANGED}")
        out[rf.path] = rf.replacement
    for rep in REPLACEMENTS:
        text = _get(out, rep.path)
        found = text.count(rep.anchor)
        if found != rep.count:
            raise ValueError(
                f"expected {rep.count} of {rep.anchor!r} in {rep.path}, "
                f"found {found}; {UPSTREAM_CHANGED}"
            )
        out[rep.path] = text.replace(rep.anchor, rep.replacement)
    for path in FLOAT_LITERAL_FILES:
        try:
            out[path] = float_literals.float_literals(_get(out, path))
        except ValueError as err:
            raise ValueError(f"{path}: {err}; {UPSTREAM_CHANGED}") from err
    out[SINGLE_HEADER_PATH] = SINGLE_HEADER
    return out


def unity_source(paths: Iterable[str]) -> str:
    """One translation unit that includes every patched `.c` file, as TinyGL's
    README allows. The include paths are relative to the generated directory."""
    return "".join(
        f'#include "tinygl/{p}"\n' for p in sorted(paths) if p.endswith(".c")
    )
