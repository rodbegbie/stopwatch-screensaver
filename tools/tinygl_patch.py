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
- Smooth lines: `glEnable(GL_LINE_SMOOTH)` is honoured. Vertices keep their
  fractional position (`zp.fx`, `zp.fy`), and a line is drawn with Wu's
  algorithm, blended over what is there.
- Filled triangles honour polygon offset, and treat `SRC_ALPHA,
  ONE_MINUS_SRC_ALPHA` as opaque (TinyGL has no alpha).
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
void glshim_depth_test_changed(ZBuffer* zb);
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

# TinyGL snaps every vertex to a whole pixel before it draws a line, so a line
# that moves a little jumps a whole pixel somewhere along its length. The
# vertex transforms now keep the fractional position (zp.fx, zp.fy), and when a
# hack enables GL_LINE_SMOOTH the line is drawn with Wu's algorithm: each step
# along the major axis shares one pixel of brightness between the two nearest
# pixels across it, blended over what is already there.
AA_LINE = """#include <math.h>

#if TGL_FEATURE_RENDER_BITS == 16
static GLint zb_aa_floor(GLfloat f) {
	GLint i = (GLint)f;
	return ((GLfloat)i > f) ? i - 1 : i;
}

static void zb_aa_plot(ZBuffer* zb, GLint x, GLint y, GLint z, GLint cover, GLint use_z, GLint sr, GLint sg, GLint sb) {
	GLushort* pz;
	PIXEL* pp;
	GLint zz, v, dr, dg, db;
	if (cover <= 0 || x < 0 || y < 0 || x >= zb->xsize || y >= zb->ysize) return;
	if (cover > 256) cover = 256;
	pz = zb->zbuf + (y * zb->xsize + x);
	zz = z >> ZB_POINT_Z_FRAC_BITS;
	if (use_z && zb->depth_test && zz < (GLint)*pz) return;
	pp = (PIXEL*)((GLbyte*)zb->pbuf + zb->linesize * y + x * PSZB);
	v = TGL_SWAP16(*pp);
	dr = (v >> 11) & 31;
	dg = (v >> 5) & 63;
	db = v & 31;
	dr = (sr * cover + dr * (256 - cover) + 128) >> 8;
	dg = (sg * cover + dg * (256 - cover) + 128) >> 8;
	db = (sb * cover + db * (256 - cover) + 128) >> 8;
	*pp = TGL_SWAP16((dr << 11) | (dg << 5) | db);
	if (use_z && zb->depth_write) *pz = zz;
}

/* Returns 0 for a line it cannot draw (colour interpolated along it, or a
 * vertex whose fractional position was not recorded), which the caller then
 * draws plain. */
static GLint ZB_line_aa(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2, GLint use_z) {
	GLfloat x0 = p1->fx, y0 = p1->fy, x1 = p2->fx, y1 = p2->fy;
	GLfloat z0 = (GLfloat)p1->z, dz = (GLfloat)(p2->z - p1->z);
	GLfloat dx, dy, grad, zinc, pos, z, scale, tmp;
	GLint colour, v, sr, sg, sb, steep, i, i0, i1, row, hi, lo;

	colour = RGB_TO_PIXEL(p1->r, p1->g, p1->b);
	if (RGB_TO_PIXEL(p2->r, p2->g, p2->b) != colour) return 0;
	if (fabsf(x0 - (GLfloat)p1->x) >= 1.0f || fabsf(y0 - (GLfloat)p1->y) >= 1.0f) return 0;
	if (fabsf(x1 - (GLfloat)p2->x) >= 1.0f || fabsf(y1 - (GLfloat)p2->y) >= 1.0f) return 0;
	v = TGL_SWAP16(colour);
	sr = (v >> 11) & 31;
	sg = (v >> 5) & 63;
	sb = v & 31;

	steep = fabsf(y1 - y0) > fabsf(x1 - x0);
	if (steep) {
		tmp = x0; x0 = y0; y0 = tmp;
		tmp = x1; x1 = y1; y1 = tmp;
	}
	if (x1 < x0) {
		tmp = x0; x0 = x1; x1 = tmp;
		tmp = y0; y0 = y1; y1 = tmp;
		z0 = (GLfloat)p2->z;
		dz = -dz;
	}
	dx = x1 - x0;
	dy = y1 - y0;
	grad = 0.0f;
	zinc = 0.0f;
	scale = 1.0f;
	i0 = zb_aa_floor(x0 - 0.5f) + 1;
	i1 = zb_aa_floor(x1 - 0.5f);
	if (i1 < i0 || dx < 1e-4f) {
		/* shorter than the gap between two pixel centres: its midpoint's pixel */
		i0 = i1 = zb_aa_floor((x0 + x1) * 0.5f);
		pos = (y0 + y1) * 0.5f - 0.5f;
		z = z0 + dz * 0.5f;
	} else {
		grad = dy / dx;
		zinc = dz / dx;
		scale = sqrtf(1.0f + grad * grad);
		pos = y0 + ((GLfloat)i0 + 0.5f - x0) * grad - 0.5f;
		z = z0 + ((GLfloat)i0 + 0.5f - x0) * zinc;
	}
	/* The brightness of a pixel is its share of a one pixel wide line. */
	for (i = i0; i <= i1; i++) {
		row = zb_aa_floor(pos);
		hi = (GLint)((pos - (GLfloat)row) * 256.0f * scale + 0.5f);
		lo = (GLint)((1.0f - (pos - (GLfloat)row)) * 256.0f * scale + 0.5f);
		if (steep) {
			zb_aa_plot(zb, row, i, (GLint)z, lo, use_z, sr, sg, sb);
			zb_aa_plot(zb, row + 1, i, (GLint)z, hi, use_z, sr, sg, sb);
		} else {
			zb_aa_plot(zb, i, row, (GLint)z, lo, use_z, sr, sg, sb);
			zb_aa_plot(zb, i, row + 1, (GLint)z, hi, use_z, sr, sg, sb);
		}
		pos += grad;
		z += zinc;
	}
	return 1;
}
#else
static GLint ZB_line_aa(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2, GLint use_z) {
	(void)zb; (void)p1; (void)p2; (void)use_z;
	return 0;
}
#endif

"""

# Two things a hack asks of filled triangles that TinyGL stores but ignores.
#
# Polygon offset: glPolygonOffset and GL_POLYGON_OFFSET_FILL are recorded and
# nothing reads them, so a line drawn on the edge of its own filled polygon
# fights it for the depth test. The offset is factor * the polygon's steepest
# depth slope + units * the smallest depth step, pushed away from the viewer.
# The rasteriser also snaps a polygon's vertices to whole pixels, so its depth
# at a pixel can be wrong by a pixel's slope in each direction, which the
# hack's own numbers assume away: when an offset is asked for, that margin
# (|dz/dx| + |dz/dy|) is added. Without it the polygon still erased a few
# percent of its own line, a different few each frame, which flickered.
#
# Alpha: TinyGL has no alpha, which is what GL's SRC_ALPHA / ONE_MINUS_SRC_ALPHA
# calls an alpha of 1, an opaque fill. Its own blend adds the colours, and reads
# our byte-swapped pixels as if they were not swapped.
FILL_TRIANGLE = """#include <math.h>
static void gl_draw_triangle_fill_plain(GLVertex* p0, GLVertex* p1, GLVertex* p2);

void gl_draw_triangle_fill(GLVertex* p0, GLVertex* p1, GLVertex* p2) {
	GLContext* c = gl_get_context();
	GLint blend = c->zb->enable_blend;
	GLint bias = 0;
	if (blend && c->zb->sfactor == GL_SRC_ALPHA && c->zb->dfactor == GL_ONE_MINUS_SRC_ALPHA)
		c->zb->enable_blend = 0;
	if (c->offset_states & TGL_OFFSET_FILL) {
		GLfloat ax = (GLfloat)(p1->zp.x - p0->zp.x), ay = (GLfloat)(p1->zp.y - p0->zp.y), az = (GLfloat)(p1->zp.z - p0->zp.z);
		GLfloat bx = (GLfloat)(p2->zp.x - p0->zp.x), by = (GLfloat)(p2->zp.y - p0->zp.y), bz = (GLfloat)(p2->zp.z - p0->zp.z);
		GLfloat det = ax * by - ay * bx;
		GLfloat slope = 0.0f, snap = 0.0f;
		if (det != 0.0f) {
			GLfloat dzdx = fabsf((az * by - ay * bz) / det);
			GLfloat dzdy = fabsf((ax * bz - az * bx) / det);
			slope = dzdx > dzdy ? dzdx : dzdy;
			snap = (c->offset_factor != 0.0f || c->offset_units != 0.0f) ? dzdx + dzdy : 0.0f;
		}
		GLfloat shift = c->offset_factor * slope + c->offset_units * (GLfloat)(1 << ZB_POINT_Z_FRAC_BITS);
		shift += shift < 0.0f ? -snap : snap;
		if (shift > 536870912.0f) shift = 536870912.0f;
		if (shift < -536870912.0f) shift = -536870912.0f;
		bias = (GLint)shift;
	}
	GLint z0 = p0->zp.z, z1 = p1->zp.z, z2 = p2->zp.z;
	if (bias) {
		const GLint zmax = (1 << (ZB_Z_BITS + ZB_POINT_Z_FRAC_BITS)) - 1;
		p0->zp.z = z0 - bias < 0 ? 0 : (z0 - bias > zmax ? zmax : z0 - bias);
		p1->zp.z = z1 - bias < 0 ? 0 : (z1 - bias > zmax ? zmax : z1 - bias);
		p2->zp.z = z2 - bias < 0 ? 0 : (z2 - bias > zmax ? zmax : z2 - bias);
	}
	gl_draw_triangle_fill_plain(p0, p1, p2);
	p0->zp.z = z0;
	p1->zp.z = z1;
	p2->zp.z = z2;
	c->zb->enable_blend = blend;
}

TGL_HOT static void gl_draw_triangle_fill_plain(GLVertex* p0, GLVertex* p1, GLVertex* p2) {
"""

# Lazy lighting (GL_LAZY_SHADE): TinyGL lights every vertex as it arrives,
# before it knows whether any triangle that uses the vertex survives back-face
# culling. Gears' heavy layout culls 12,300 of its 17,600 triangles. A lit
# vertex is marked pending instead, and shaded when a triangle, line or point
# that uses it is drawn. Lighting reads only the vertex's own eye position
# and normal, which are kept, and the material and light state, so any
# change to that state first shades every vertex still pending: the result is
# what shading at arrival would have given.
LAZY_SHADE_DECLARATIONS = """
void gl_shade_pending(GLVertex* v);
void gl_shade_pending_all(void);
void gl_eye_normal(GLVertex* v, V4* n);
#ifdef GL_LAZY_SHADE
#define GL_SHADE_NOW(v) do { if ((v)->shade_pending) gl_shade_pending(v); } while (0)
#define GL_SHADE_FLUSH() gl_shade_pending_all()
#else
#define GL_SHADE_NOW(v) do { } while (0)
#define GL_SHADE_FLUSH() do { } while (0)
#endif
#define GL_SHADE_NOW3(a, b, c) do { GL_SHADE_NOW(a); GL_SHADE_NOW(b); GL_SHADE_NOW(c); } while (0)
"""

NORMAL_TRANSFORM = """		m = &c->matrix_model_view_inv.m[0][0];
		n = &c->current_normal;

		v->normal.X = (n->X * m[0] + n->Y * m[1] + n->Z * m[2]);
		v->normal.Y = (n->X * m[4] + n->Y * m[5] + n->Z * m[6]);
		v->normal.Z = (n->X * m[8] + n->Y * m[9] + n->Z * m[10]);

		if (c->normalize_enabled) {
			gl_V3_Norm_Fast(&v->normal);
		}
"""

# Under GL_LAZY_SHADE the eye-space normal is made by gl_shade_pending, if
# ever: the inverse modelview cannot change inside glBegin/glEnd.
LAZY_NORMAL_TRANSFORM = (
    """#ifdef GL_LAZY_SHADE
		v->normal.X = c->current_normal.X;
		v->normal.Y = c->current_normal.Y;
		v->normal.Z = c->current_normal.Z;
#else
"""
    + NORMAL_TRANSFORM
    + "#endif\n"
)

LAZY_SHADE_FUNCTIONS = """/* Lazy lighting (GL_LAZY_SHADE): the colour, then the rasteriser's copy of
 * it, as glopVertex and gl_transform_to_viewport_vertex_c would have set them. */
/* The normal in eye space, as gl_vertex_transform made it. */
TGL_HOT void gl_eye_normal(GLVertex* v, V4* n) {
	GLContext* c = gl_get_context();
	GLfloat* m = &c->matrix_model_view_inv.m[0][0];
	GLfloat x = n->X, y = n->Y, z = n->Z;
	v->normal.X = (x * m[0] + y * m[1] + z * m[2]);
	v->normal.Y = (x * m[4] + y * m[5] + z * m[6]);
	v->normal.Z = (x * m[8] + y * m[9] + z * m[10]);
	if (c->normalize_enabled) {
		gl_V3_Norm_Fast(&v->normal);
	}
}

TGL_HOT void gl_shade_pending(GLVertex* v) {
	V4 n;
	v->shade_pending = 0;
	n.X = v->normal.X;
	n.Y = v->normal.Y;
	n.Z = v->normal.Z;
	gl_eye_normal(v, &n);
	gl_shade_vertex(v);
#if TGL_OPTIMIZATION_HINT_BRANCH_COST < 2
	if (v->clip_code == 0)
#endif
	{
		v->zp.r = (GLint)(v->color.v[0] * COLOR_CORRECTED_MULT_MASK + COLOR_MIN_MULT) & COLOR_MASK;
		v->zp.g = (GLint)(v->color.v[1] * COLOR_CORRECTED_MULT_MASK + COLOR_MIN_MULT) & COLOR_MASK;
		v->zp.b = (GLint)(v->color.v[2] * COLOR_CORRECTED_MULT_MASK + COLOR_MIN_MULT) & COLOR_MASK;
	}
}

void gl_shade_pending_all(void) {
	GLContext* c = gl_get_context();
	GLint i;
	for (i = 0; i < POLYGON_MAX_VERTEX; i++)
		GL_SHADE_NOW(&c->vertex[i]);
}

/* non optimized lightening model */
"""

DISC_SCANLINE_ANCHOR = "#ifdef INTERP_STZ\n\n\n#endif\n\t\t\t\twhile (n >= 3) {"

DISC_SCANLINE = """#ifdef INTERP_STZ


#endif
\t\t\t\tif (tgl_disc_clip) {
\t\t\t\t\tconst GLint row = (GLint)((pp1 - (PIXEL*)zb->pbuf) / zb->xsize);
\t\t\t\t\tconst GLint over = x1 + n - glshim_span_x1[row];
\t\t\t\t\tconst GLint skip = glshim_span_x0[row] - x1;
\t\t\t\t\tif (over > 0)
\t\t\t\t\t\tn -= over;
\t\t\t\t\tif (skip > 0) {
\t\t\t\t\t\tn -= skip;
\t\t\t\t\t\tpp += skip;
#ifdef INTERP_Z
\t\t\t\t\t\tpz += skip;
\t\t\t\t\t\tz += dzdx * skip;
#endif
#ifdef INTERP_RGB
\t\t\t\t\t\tor1 += drdx * skip;
\t\t\t\t\t\tog1 += dgdx * skip;
\t\t\t\t\t\tob1 += dbdx * skip;
#endif
#ifdef INTERP_ST
\t\t\t\t\t\ts += dsdx * skip;
\t\t\t\t\t\tt += dtdx * skip;
#endif
\t\t\t\t\t}
\t\t\t\t}
\t\t\t\twhile (n >= 3) {"""

# The disc is convex, so a triangle with every vertex inside it needs no clip,
# and its scanlines skip the check: on Gears, which never reaches the corners,
# a check on every scanline cost 3.2 ms a frame.
DISC_TRIANGLE_ANCHOR = "\tpp1 = (PIXEL*)(zb->pbuf) + zb->xsize * p0->y; \n"

DISC_TRIANGLE = (
    DISC_TRIANGLE_ANCHOR
    + "\tconst GLint tgl_disc_clip = glshim_span_x0 && !(TGL_IN_DISC(zb, p0) && "
    "TGL_IN_DISC(zb, p1) && TGL_IN_DISC(zb, p2));\n"
)

# Line batching (ARS-56). The lines of one glBegin(GL_LINES) batch that move in
# lockstep (same major axis, step count and start) are drawn a few
# pixels each in turn instead of one after another, so that neighbouring lines
# share PSRAM cache lines in the depth and colour buffers. A pixel is visited
# at one major coordinate, so lines in lockstep reach it at the same step, in
# queue order: the result is bit for bit what drawing them in turn gives. Only
# flat-coloured, depth-tested, unsmoothed lines are queued, and only
# consecutive ones are grouped. Everything else flushes the queue first.
# -DGL_NO_LINE_BATCH draws every line at once, as before.
LINE_BATCH = """
#ifndef GL_NO_LINE_BATCH
#define ZB_LINEQ_MAX 8
#define ZB_LINE_TURN 32

typedef struct {
	ZBufferPoint a, b;
	GLint color;
} ZBQLine;

typedef struct {
	PIXEL* pp;
	GLushort* pz;
	GLint z, zinc, a, d1, d2, i1, i2, left;
	PIXEL color;
} ZBLineRun;

static ZBQLine zb_lineq[ZB_LINEQ_MAX];
static GLint zb_lineq_n;

/* The axis the line steps along (0 x, 1 y, -1 for a single pixel), its step
 * count and where it starts along it. Two lines with the same three reach a
 * pixel at the same step, so their direction does not matter: lines that run
 * apart from one start share only that first pixel. */
static GLint zb_lockstep_key(const ZBQLine* q, GLint* n, GLint* start) {
	GLint dx = q->b.x - q->a.x, dy = q->b.y - q->a.y;
	GLint adx = dx < 0 ? -dx : dx;
	if (dx == 0 && dy == 0) return -1;
	if (adx >= dy) {
		*n = adx;
		*start = q->a.x;
		return 0;
	}
	*n = dy;
	*start = q->a.y;
	return 1;
}

/* The state zline.h's DRAWLINE starts from, for a flat, depth-tested line. */
static void zb_line_run_init(ZBuffer* zb, const ZBQLine* q, ZBLineRun* r) {
	GLint sx = zb->xsize;
	GLint dx = q->b.x - q->a.x, dy = q->b.y - q->a.y;
	GLint major, minor, i1, i2;
	r->pp = (PIXEL*)((GLbyte*)zb->pbuf + zb->linesize * q->a.y + q->a.x * PSZB);
	r->pz = zb->zbuf + (q->a.y * sx + q->a.x);
	r->z = q->a.z;
	r->color = (PIXEL)q->color;
	if (dx == 0 && dy == 0) {
		r->zinc = r->a = r->d1 = r->d2 = r->i1 = r->i2 = 0;
		r->left = 1;
		return;
	}
	if (dx > 0) {
		if (dx >= dy) {
			major = dx, minor = dy, i1 = sx + 1, i2 = 1;
		} else {
			major = dy, minor = dx, i1 = sx + 1, i2 = sx;
		}
	} else {
		dx = -dx;
		if (dx >= dy) {
			major = dx, minor = dy, i1 = sx - 1, i2 = -1;
		} else {
			major = dy, minor = dx, i1 = sx - 1, i2 = sx;
		}
	}
	r->zinc = (q->b.z - q->a.z) / major;
	r->a = 2 * minor - major;
	r->d1 = 2 * minor;
	r->d2 = 2 * major - 2 * minor;
	r->i1 = i1;
	r->i2 = i2;
	r->left = major + 1;
}

/* Up to `steps` more pixels of the line, each step as the loop in zline.h. */
TGL_HOT static void zb_line_run(ZBuffer* zb, ZBLineRun* r, GLint steps) {
	GLubyte zbdt = zb->depth_test;
	GLubyte zbdw = zb->depth_write;
	register PIXEL* pp = r->pp;
	register GLushort* pz = r->pz;
	register GLint z = r->z, a = r->a;
	const GLint zinc = r->zinc, d1 = r->d1, d2 = r->d2, i1 = r->i1, i2 = r->i2;
	const PIXEL color = r->color;
	if (steps > r->left) steps = r->left;
	r->left -= steps;
	while (--steps >= 0) {
		GLint zz = z >> ZB_POINT_Z_FRAC_BITS;
		if (ZCMP(zz, *pz)) {
			*pp = color;
			if (zbdw) *pz = zz;
		}
		z += zinc;
		if (a > 0) {
			pp += i1;
			pz += i1;
			a -= d2;
		} else {
			pp += i2;
			pz += i2;
			a += d1;
		}
	}
	r->pp = pp;
	r->pz = pz;
	r->z = z;
	r->a = a;
}

void ZB_line_flush(ZBuffer* zb) {
	GLint i = 0;
	if (!zb_lineq_n) return;
	GLPROF_SCOPE_OUTER(GLPROF_LINE);
	while (i < zb_lineq_n) {
		GLint n0, start0, n, start, j = i + 1;
		GLint axis0 = zb_lockstep_key(&zb_lineq[i], &n0, &start0);
		while (axis0 >= 0 && j < zb_lineq_n && zb_lockstep_key(&zb_lineq[j], &n, &start) == axis0 &&
			   n == n0 && start == start0)
			j++;
		if (j - i == 1) {
			ZB_line_flat_z(zb, &zb_lineq[i].a, &zb_lineq[i].b, zb_lineq[i].color);
		} else {
			ZBLineRun runs[ZB_LINEQ_MAX];
			GLint k, left = n0 + 1;
			for (k = i; k < j; k++) zb_line_run_init(zb, &zb_lineq[k], &runs[k - i]);
			while (left > 0) {
				for (k = 0; k < j - i; k++) zb_line_run(zb, &runs[k], ZB_LINE_TURN);
				left -= ZB_LINE_TURN;
			}
			zb_line_lockstep_lines += j - i;
		}
		i = j;
	}
	zb_lineq_n = 0;
}

void ZB_line_dispatch(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {
	if (!zb->depth_test) {
		ZB_line_flush(zb);
		ZB_line(zb, p1, p2);
		return;
	}
	if (!zb->line_smooth) {
		GLint color1 = RGB_TO_PIXEL(p1->r, p1->g, p1->b);
		GLint color2 = RGB_TO_PIXEL(p2->r, p2->g, p2->b);
		if (color1 == color2) {
			ZBQLine* q;
			if (zb_lineq_n == ZB_LINEQ_MAX) ZB_line_flush(zb);
			q = &zb_lineq[zb_lineq_n++];
			if (p1->y > p2->y || (p1->y == p2->y && p1->x > p2->x)) {
				q->a = *p2;
				q->b = *p1;
			} else {
				q->a = *p1;
				q->b = *p2;
			}
			q->color = color1;
			return;
		}
	}
	ZB_line_flush(zb);
	ZB_line_z(zb, p1, p2);
}
#else /* GL_NO_LINE_BATCH */
void ZB_line_flush(ZBuffer* zb) { (void)zb; }

void ZB_line_dispatch(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {
	if (zb->depth_test)
		ZB_line_z(zb, p1, p2);
	else
		ZB_line(zb, p1, p2);
}
#endif
"""


REPLACEMENTS: tuple[Replace, ...] = (
    Replace(
        "src/clip.c",
        "void gl_draw_triangle_fill(GLVertex* p0, GLVertex* p1, GLVertex* p2) { \n",
        FILL_TRIANGLE,
    ),
    Replace(
        "include/zbuffer.h",
        "    GLint depth_write;\n",
        "    GLint depth_write;\n    GLint line_smooth; /* GL_LINE_SMOOTH */\n",
    ),
    Replace(
        "include/zbuffer.h",
        "  GLint x,y,z;     /* integer coordinates in the zbuffer */\n",
        "  GLint x,y,z;     /* integer coordinates in the zbuffer */\n"
        "  GLfloat fx,fy;   /* x and y before they were truncated */\n",
    ),
    Replace(
        "src/clip.c",
        "v->zp.y = (GLint)(v->pc.Y * winv * c->viewport.scale.Y + c->viewport.trans.Y);\n",
        "v->zp.y = (GLint)(v->pc.Y * winv * c->viewport.scale.Y + c->viewport.trans.Y);\n"
        "\t\tv->zp.fx = v->pc.X * winv * c->viewport.scale.X + c->viewport.trans.X;\n"
        "\t\tv->zp.fy = v->pc.Y * winv * c->viewport.scale.Y + c->viewport.trans.Y;\n",
    ),
    Replace(
        "src/vertex.c",
        "v->zp.y = (GLint)(v->pc.Y * winv * c->viewport.scale.Y + c->viewport.trans.Y);\n",
        "v->zp.y = (GLint)(v->pc.Y * winv * c->viewport.scale.Y + c->viewport.trans.Y);\n"
        "\t\tv->zp.fx = v->pc.X * winv * c->viewport.scale.X + c->viewport.trans.X;\n"
        "\t\tv->zp.fy = v->pc.Y * winv * c->viewport.scale.Y + c->viewport.trans.Y;\n",
    ),
    Replace(
        "src/init.c",
        "\tc->zb->depth_write = 1;\n",
        "\tc->zb->depth_write = 1;\n\tc->zb->line_smooth = 0;\n",
    ),
    Replace(
        "src/misc.c",
        "\tcase GL_POLYGON_STIPPLE:\n",
        "\tcase GL_LINE_SMOOTH:\n\t\tc->zb->line_smooth = v;\n\t\tbreak;\n\tcase GL_POLYGON_STIPPLE:\n",
    ),
    Replace(
        "src/zline.c",
        "void ZB_line_z(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {\n",
        AA_LINE + "void ZB_line_z(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {\n"
        "\tif (zb->line_smooth && ZB_line_aa(zb, p1, p2, 1)) return;\n",
    ),
    Replace(
        "src/zline.c",
        "void ZB_line(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {\n",
        "void ZB_line(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {\n"
        "\tif (zb->line_smooth && ZB_line_aa(zb, p1, p2, 0)) return;\n",
    ),
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
        "src/api.c",
        "\tp[2].i = type;\n"
        "\t/* TODO: 3 components ? */\n"
        "\tfor (i = 0; i < 4; i++)\n\t\tp[3 + i].f = v[i];\n",
        "\tp[2].i = type;\n"
        "\t/* Colours and the position have four values, a spot direction three and\n"
        "\t * the rest one, and hacks pass arrays of that size. */\n"
        "\tfor (i = 0; i < 4; i++)\n"
        "\t\tp[3 + i].f = i < (type == GL_SPOT_DIRECTION ? 3\n"
        "\t\t                  : (type == GL_AMBIENT || type == GL_DIFFUSE ||\n"
        "\t\t                     type == GL_SPECULAR || type == GL_POSITION)\n"
        "\t\t                      ? 4\n"
        "\t\t                      : 1)\n"
        "\t\t                 ? v[i]\n"
        "\t\t                 : 0;\n",
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
        "src/misc.c",
        "\t\tc->zb->depth_test = v;\n",
        "\t\tc->zb->depth_test = v;\n\t\tglshim_depth_test_changed(c->zb);\n",
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
    Replace(
        "src/zgl.h",
        "\tGLint edge_flag;\n} GLVertex;\n",
        "\tGLint edge_flag;\n\tGLint shade_pending; /* lit, not shaded yet (GL_LAZY_SHADE) */\n"
        "} GLVertex;\n",
    ),
    Replace(
        "src/zgl.h",
        "extern GLContext gl_ctx;\n",
        "extern GLContext gl_ctx;\n" + LAZY_SHADE_DECLARATIONS,
    ),
    Replace(
        "src/vertex.c",
        "\tif (c->lighting_enabled) {\n\t\tgl_shade_vertex(v);\n",
        "\tif (c->lighting_enabled) {\n#ifdef GL_LAZY_SHADE\n\t\tv->shade_pending = 1;\n"
        "#else\n\t\tgl_shade_vertex(v);\n#endif\n",
    ),
    Replace("src/vertex.c", NORMAL_TRANSFORM, LAZY_NORMAL_TRANSFORM),
    Replace(
        "src/vertex.c",
        "\t\tv->color = c->current_color;\n",
        "\t\tv->color = c->current_color;\n#ifdef GL_LAZY_SHADE\n\t\tv->shade_pending = 0;\n#endif\n",
    ),
    Replace(
        "src/light.c", "/* non optimized lightening model */\n", LAZY_SHADE_FUNCTIONS
    ),
    Replace(
        "src/light.c",
        "void glopMaterial(GLParam* p) {\n\tGLContext* c = gl_get_context();\n",
        "void glopMaterial(GLParam* p) {\n\tGLContext* c = gl_get_context();\n"
        "\tGL_SHADE_FLUSH();\n",
    ),
    Replace(
        "src/light.c",
        "void glopLight(GLParam* p) {\n\tGLContext* c = gl_get_context();\n",
        "void glopLight(GLParam* p) {\n\tGLContext* c = gl_get_context();\n"
        "\tGL_SHADE_FLUSH();\n",
    ),
    Replace(
        "src/light.c",
        "void glopLightModel(GLParam* p) {\n\tGLContext* c = gl_get_context();\n",
        "void glopLightModel(GLParam* p) {\n\tGLContext* c = gl_get_context();\n"
        "\tGL_SHADE_FLUSH();\n",
    ),
    Replace(
        "src/light.c",
        "void glopSetEnableSpecular(GLParam* p) {\n",
        "void glopSetEnableSpecular(GLParam* p) {\n\tGL_SHADE_FLUSH();\n",
    ),
    Replace(
        "src/clip.c",
        "static void updateTmp(GLVertex* q, GLVertex* p0, GLVertex* p1, GLfloat t) { \n"
        "\tGLContext* c = gl_get_context();\n",
        "static void updateTmp(GLVertex* q, GLVertex* p0, GLVertex* p1, GLfloat t) { \n"
        "\tGLContext* c = gl_get_context();\n"
        "#ifdef GL_LAZY_SHADE\n\tq->shade_pending = 0; /* its colour comes from shaded vertices */\n#endif\n",
    ),
    Replace(
        "src/misc.c",
        "void glopEnableDisable(GLParam* p) {\n\tGLContext* c = gl_get_context();\n",
        "void glopEnableDisable(GLParam* p) {\n\tGLContext* c = gl_get_context();\n"
        "\tGL_SHADE_FLUSH();\n",
    ),
    # Additive blending (issue #73): a blend that only adds the colours (each
    # factor ONE or one TinyGL treats as ONE, with GL_FUNC_ADD) takes a
    # saturating add on the byte-swapped RGB565 pixel. TinyGL's own path
    # decoded the swapped pixel as unswapped and ran three switches a pixel.
    Replace(
        "include/zbuffer.h",
        "#define TGL_BLEND_VARS GLuint zbblendeq = zb->blendeq; GLuint sfactor = zb->sfactor; "
        "GLuint dfactor = zb->dfactor;",
        "extern const short *glshim_span_x0, *glshim_span_x1; /* glshim.c, issue #77 */\n"
        "#define TGL_IN_DISC(zb, p) ((p)->y >= 0 && (p)->y < (zb)->ysize && "
        "(p)->x >= glshim_span_x0[(p)->y] && (p)->x <= glshim_span_x1[(p)->y])\n"
        "static inline PIXEL glshim_add565(PIXEL dst, PIXEL src) {\n"
        "\tGLuint a = TGL_SWAP16(dst), b = TGL_SWAP16(src);\n"
        "\tGLuint r = (a & 0xF800) + (b & 0xF800), g = (a & 0x07E0) + (b & 0x07E0), "
        "bl = (a & 0x001F) + (b & 0x001F);\n"
        "\tif (r > 0xF800) r = 0xF800;\n\tif (g > 0x07E0) g = 0x07E0;\n"
        "\tif (bl > 0x001F) bl = 0x001F;\n"
        "\treturn (PIXEL)TGL_SWAP16(r | g | bl);\n}\n"
        "#define TGL_BLEND_ADDS(sf, df, eq) ((sf) != GL_ONE_MINUS_SRC_COLOR && (sf) != GL_ZERO && "
        "(df) != GL_ONE_MINUS_DST_COLOR && (df) != GL_ZERO && (eq) != GL_FUNC_SUBTRACT && "
        "(eq) != GL_FUNC_REVERSE_SUBTRACT)\n"
        "#define TGL_BLEND_VARS GLuint zbblendeq = zb->blendeq; GLuint sfactor = zb->sfactor; "
        "GLuint dfactor = zb->dfactor; GLint tgl_adds = TGL_BLEND_ADDS(sfactor, dfactor, zbblendeq);",
    ),
    Replace(
        "include/zbuffer.h",
        "#define TGL_BLEND_FUNC(source, dest){\t",
        "#define TGL_BLEND_FUNC(source, dest){ if (tgl_adds) { (dest) = glshim_add565((dest), (source)); } else\t",
    ),
    Replace(
        "include/zbuffer.h",
        "#define TGL_BLEND_FUNC_RGB(rr, gg, bb, dest){\t",
        "#define TGL_BLEND_FUNC_RGB(rr, gg, bb, dest){ if (tgl_adds) { (dest) = glshim_add565((dest), "
        "RGB_TO_PIXEL((rr), (gg), (bb))); } else\t",
    ),
    # Built without GL_POLYGON (which also drops GL_LINE_LOOP), an unhandled
    # primitive draws nothing but kept counting vertices, and the fifth was
    # written past c->vertex over the matrices and colours after it.
    Replace(
        "src/vertex.c",
        "#else\n\tdefault:\n\t\tbreak;\n#endif\n\t}\n\n\tc->vertex_n = n;",
        "#else\n\tdefault:\n\t\tn = 0; /* draws nothing; must not run past c->vertex */\n"
        "\t\tbreak;\n#endif\n\t}\n\n\tc->vertex_n = n;",
    ),
    # GL_LINE_LOOP is wanted without GL_POLYGON (issue #80). The vertex case is
    # the strip's, which keeps the first vertex in slot 2; glEnd closes the loop.
    Replace(
        "src/vertex.c",
        "\tcase GL_LINE_STRIP:\n#if TGL_FEATURE_GL_POLYGON == 1\n\tcase GL_LINE_LOOP:\n#endif\n",
        "\tcase GL_LINE_STRIP:\n\tcase GL_LINE_LOOP:\n",
    ),
    Replace(
        "src/vertex.c",
        "#if TGL_FEATURE_GL_POLYGON == 1\n\t\tif (c->begin_type == GL_LINE_LOOP) {\n"
        "\t\t\tif (c->vertex_cnt >= 3) {\n\t\t\t\tgl_draw_line(&c->vertex[0], &c->vertex[2]);\n"
        "\t\t\t}\n\t\t} else if (c->begin_type == GL_POLYGON) {\n",
        "\t\tif (c->begin_type == GL_LINE_LOOP && c->vertex_cnt >= 3) {\n"
        "\t\t\tgl_draw_line(&c->vertex[0], &c->vertex[2]);\n\t\t}\n"
        "#if TGL_FEATURE_GL_POLYGON == 1\n\t\tif (c->begin_type == GL_POLYGON) {\n",
    ),
    # Line batching (ARS-56): see LINE_BATCH.
    Replace(
        "include/zbuffer.h",
        "void ZB_line_z(ZBuffer * zb, ZBufferPoint * p1, ZBufferPoint * p2);",
        "void ZB_line_z(ZBuffer * zb, ZBufferPoint * p1, ZBufferPoint * p2);\n"
        "extern GLuint zb_line_lockstep_lines;\n"
        "void ZB_line_dispatch(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2);\n"
        "void ZB_line_flush(ZBuffer* zb);\n",
    ),
    Replace(
        "src/zline.c",
        "void ZB_line_z(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {\n",
        "GLuint zb_line_lockstep_lines;\n"
        "static void ZB_line_flat_z(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2, GLint color);\n"
        + LINE_BATCH
        + "void ZB_line_z(ZBuffer* zb, ZBufferPoint* p1, ZBufferPoint* p2) {\n",
    ),
    Replace(
        "src/clip.c",
        "\t\t\tif (c->zb->depth_test)\n\t\t\t\tZB_line_z(c->zb, &p1->zp, &p2->zp);\n"
        "\t\t\telse\n\t\t\t\tZB_line(c->zb, &p1->zp, &p2->zp);\n",
        "\t\t\tZB_line_dispatch(c->zb, &p1->zp, &p2->zp);\n",
    ),
    Replace(
        "src/clip.c",
        "\t\t\t\tif (c->zb->depth_test)\n\t\t\t\t\tZB_line_z(c->zb, &q1.zp, &q2.zp);\n"
        "\t\t\t\telse\n\t\t\t\t\tZB_line(c->zb, &q1.zp, &q2.zp);\n",
        "\t\t\t\tZB_line_dispatch(c->zb, &q1.zp, &q2.zp);\n",
    ),
    Replace(
        "src/vertex.c",
        "\tc->in_begin = 1;\n",
        "\tZB_line_flush(c->zb);\n\tc->in_begin = 1;\n",
    ),
    Replace(
        "src/vertex.c",
        "\tc->in_begin = 0;\n",
        "\tZB_line_flush(c->zb);\n\tc->in_begin = 0;\n",
    ),
    # GL_POLYGON is a fan from its first vertex, exact for the convex polygons
    # the hacks draw (ARS-18). Done where the op runs, so a display list that
    # records GL_POLYGON replays it as a fan too.
    Replace(
        "src/vertex.c",
        "\t\ttype = p[1].i;\n\tc->begin_type = type;",
        "\t\ttype = p[1].i;\n\tif (type == GL_POLYGON) type = GL_TRIANGLE_FAN;\n\tc->begin_type = type;",
    ),
    # The round panel (issue #77): each scanline of a filled triangle is cut to
    # the span the panel shows, with the depth and colour it would have had at
    # its new first pixel. Only the generic scanline; the textured fills have
    # their own DRAW_LINE.
    Replace("src/ztriangle.h", DISC_TRIANGLE_ANCHOR, DISC_TRIANGLE),
    Replace("src/ztriangle.h", DISC_SCANLINE_ANCHOR, DISC_SCANLINE),
)

# Anchors that earlier patches wrote (the dirty-rectangle hooks and the
# profile scopes), so applied after REPLACEMENTS and HOT_FUNCTIONS.
LATE_REPLACEMENTS: tuple[Replace, ...] = (
    Replace(
        "src/clip.c",
        "{ glshim_note_triangle(p0, p1, p2);",
        "{ GL_SHADE_NOW3(p0, p1, p2); glshim_note_triangle(p0, p1, p2);",
        count=4,
    ),
    Replace(
        "src/clip.c",
        "\t\t\tgl_draw_triangle_clip(p0, p1, p2, 0);\n",
        "\t\t\tGL_SHADE_NOW3(p0, p1, p2);\n\t\t\tgl_draw_triangle_clip(p0, p1, p2, 0);\n",
    ),
    Replace(
        "src/clip.c",
        "\tGLPROF_SCOPE(GLPROF_POINT);\n",
        "\tGLPROF_SCOPE(GLPROF_POINT);\n\tGL_SHADE_NOW(p0);\n",
    ),
    Replace(
        "src/clip.c",
        "\tGLPROF_SCOPE(GLPROF_LINE);\n",
        "\tGLPROF_SCOPE(GLPROF_LINE);\n\tGL_SHADE_NOW(p1);\n\tGL_SHADE_NOW(p2);\n",
    ),
)


@dataclass(frozen=True)
class Hot:
    """A function on TinyGL's per-vertex or per-triangle path, found by its
    definition line in the patched tree. It is marked `TGL_HOT` (IRAM when
    built with GL_IRAM). With a `slot` it is also timed under GL_PROFILE, from
    its first line to whichever return it leaves by; `reentrant` times only
    the outermost call (lists call lists)."""

    path: str
    definition: str
    slot: str = ""
    reentrant: bool = False

    @property
    def anchor(self) -> str:
        return self.definition + " {\n"

    def replacement(self) -> str:
        text = "TGL_HOT " + self.anchor
        if self.slot:
            scope = "GLPROF_SCOPE_OUTER" if self.reentrant else "GLPROF_SCOPE"
            text += f"\t{scope}({self.slot});\n"
        return text


_FILL_ARGS = "ZBuffer* zb, ZBufferPoint* p0, ZBufferPoint* p1, ZBufferPoint* p2"

# Applied after REPLACEMENTS, so a definition one of them wrote (the fill) is
# found too.
HOT_FUNCTIONS: tuple[Hot, ...] = (
    Hot("src/list.c", "void glopCallList(GLParam* p)", "GLPROF_LIST", True),
    Hot("src/vertex.c", "void glopVertex(GLParam* p)", "GLPROF_VERTEX"),
    Hot("src/light.c", "void gl_shade_vertex(GLVertex* v)", "GLPROF_SHADE"),
    Hot(
        "src/clip.c",
        "void gl_draw_triangle(GLVertex* p0, GLVertex* p1, GLVertex* p2)",
        "GLPROF_TRIANGLE",
    ),
    Hot(
        "src/clip.c",
        "void gl_draw_triangle_fill(GLVertex* p0, GLVertex* p1, GLVertex* p2)",
        "GLPROF_FILL",
    ),
    Hot("src/clip.c", "void gl_draw_point(GLVertex* p0)", "GLPROF_POINT"),
    Hot("src/clip.c", "void gl_draw_line(GLVertex* p1, GLVertex* p2)", "GLPROF_LINE"),
    Hot(
        "src/clip.c",
        "static void gl_draw_triangle_clip(GLVertex* p0, GLVertex* p1, GLVertex* p2, GLint clip_bit)",
    ),
    Hot("src/vertex.c", "static void gl_transform_to_viewport_vertex_c(GLVertex* v)"),
    Hot("src/vertex.c", "static void gl_vertex_transform(GLVertex* v)"),
    Hot("src/vertex.c", "void glopNormal(GLParam* p)"),
    Hot("src/vertex.c", "void glopColor(GLParam* p)"),
    Hot(
        "src/zgl.h",
        "static GLint gl_clipcode(GLfloat x, GLfloat y, GLfloat z, GLfloat w1)",
    ),
    Hot("src/zgl.h", "static GLfloat clampf(GLfloat a, GLfloat min, GLfloat max)"),
    Hot("src/zmath.h", "static int gl_V3_Norm_Fast(V3* a)"),
    Hot("src/zmath.c", "void gl_M4_MulV4(V4* a, M4* b, V4* c)"),
    Hot("src/ztriangle.c", f"void ZB_fillTriangleFlat({_FILL_ARGS})"),
    Hot("src/ztriangle.c", f"void ZB_fillTriangleFlatNOBLEND({_FILL_ARGS})"),
    Hot("src/ztriangle.c", f"void ZB_fillTriangleSmooth({_FILL_ARGS})"),
    Hot("src/ztriangle.c", f"void ZB_fillTriangleSmoothNOBLEND({_FILL_ARGS})"),
    Hot("src/zline.c", "void ZB_plot(ZBuffer* zb, ZBufferPoint* p)"),
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
/* TGL_HOT, and the GL_PROFILE counters (issue #43). */
#include "glshim/glshim_prof.h"
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
    for hot in HOT_FUNCTIONS:
        text = _get(out, hot.path)
        found = text.count(hot.anchor)
        if found != 1:
            raise ValueError(
                f"expected 1 of {hot.anchor!r} in {hot.path}, found {found}; "
                f"{UPSTREAM_CHANGED}"
            )
        out[hot.path] = text.replace(hot.anchor, hot.replacement())
    for rep in LATE_REPLACEMENTS:
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
