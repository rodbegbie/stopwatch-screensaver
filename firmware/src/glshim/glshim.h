/* The GL a hack sees when it is built with USE_GL: TinyGL's API, the entry
 * points TinyGL defines but its header does not declare, and small stand-ins
 * for the GLU and GLX calls hacks make. Not derived from xscreensaver code. */
#ifndef GLSHIM_H
#define GLSHIM_H

#include <stdint.h>

#include "GL/gl.h"
#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* There is one context and one drawable, so GLX has little to do. */
typedef void *GLXContext;
typedef Drawable GLXDrawable;

/* Opens TinyGL over the canvas's pixels, replacing any open context, and
 * registers a hook so the context is freed when the hack stops. Returns a
 * handle for the hack to keep, or NULL if the canvas is empty or TinyGL cannot
 * allocate its z-buffer. */
GLXContext *glshim_open(Canvas *canvas);
/* Frees the context. Safe to call when none is open. */
void glshim_close(void);
/* True while a context is open. The tests use it to check that stopping a hack
 * closed its context. */
int glshim_is_open(void);
/* Read by the next glshim_open and reset by glshim_close: at 2, TinyGL draws
 * at half the width and height, glViewport is halved to match, and each swap
 * doubles every pixel of the dirty box into the canvas. For a hack whose fill
 * is too slow at full size (Hypnowheel). */
void glshim_set_render_scale(int scale);
/* Per row of TinyGL's buffer, the span the round panel shows, or NULL when the
 * canvas has no disc. Set by glshim_open, cleared by glshim_close. At scale 2 a
 * pixel is in the span if any of its 2 by 2 block of the canvas is. Clears and
 * fills skip the rest. */
extern const int16_t *glshim_span_x0, *glshim_span_x1;
void glshim_Viewport(GLint x, GLint y, GLint w, GLint h);
#define glViewport glshim_Viewport

/* How many depth clears have actually run (a clear made while the depth test
 * is off is deferred until it is switched on). For the tests. */
unsigned glshim_depth_clears(void);

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
/* Called with every box (clamped, non-empty) that TinyGL reports it has drawn
 * in, in canvas pixels. NULL in the firmware; tools/gl_budget.py sets it so
 * that it measures exactly the boxes the dirty rectangle sees. */
extern void (*glshim_box_observer)(int x0, int y0, int x1, int y1);

/* Something other than TinyGL has written the canvas's pixels: the next clear
 * clears all of it. The dirty rectangle otherwise trusts that only TinyGL
 * writes there (test_push_overlap's noise run is the one caller). */
void glshim_canvas_overwritten(void);

/* TinyGL has drawn into the canvas already; this marks what it drew since the
 * last swap, and what the last clear erased, dirty. When canvas->px has
 * changed since TinyGL last drew (the push overlap swaps two buffers), TinyGL
 * follows it before its next clear or primitive, and the box last drawn in the
 * other buffer, which is what the display shows, is marked erased too. */
void glXSwapBuffers(Display *dpy, GLXDrawable drawable);

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble znear,
                    GLdouble zfar);
void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble cx,
               GLdouble cy, GLdouble cz, GLdouble upx, GLdouble upy,
               GLdouble upz);

/* TinyGL counts a vertex array's stride in floats and reads the array when a
 * display list is replayed. GL counts bytes and reads at the draw call, and
 * hacks free an array straight after drawing it. So hacks' array calls come
 * here, and a draw is expanded at once into glBegin, glNormal3f, glVertex3f
 * and glEnd. A stride of 0 means tightly packed. Texture coordinates are
 * accepted and ignored. */
void glshim_VertexPointer(GLint size, GLenum type, GLsizei stride,
                          const GLvoid *pointer);
void glshim_NormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer);
void glshim_ColorPointer(GLint size, GLenum type, GLsizei stride,
                         const GLvoid *pointer);
void glshim_TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                            const GLvoid *pointer);
void glshim_EnableClientState(GLenum array);
void glshim_DisableClientState(GLenum array);
void glshim_DrawArrays(GLenum mode, GLint first, GLsizei count);
/* Sets the arrays a packed format holds and turns the others off, as GL does,
 * for the formats gllist.c uses: GL_V3F, GL_C3F_V3F and GL_N3F_V3F. Any other
 * format turns every array off, so nothing is drawn from it. */
void glshim_InterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer);
#define glInterleavedArrays glshim_InterleavedArrays
#define glVertexPointer glshim_VertexPointer
#define glNormalPointer glshim_NormalPointer
#define glColorPointer glshim_ColorPointer
#define glTexCoordPointer glshim_TexCoordPointer
#define glEnableClientState glshim_EnableClientState
#define glDisableClientState glshim_DisableClientState
#define glDrawArrays glshim_DrawArrays

/* TinyGL takes only three-component GL_RGB, GL_UNSIGNED_BYTE textures and
 * exits on anything else. This converts GL_RGBA (alpha dropped), GL_LUMINANCE
 * and GL_LUMINANCE_ALPHA bytes to RGB first, and ignores any other format or
 * type. Not for use inside a display list: TinyGL would keep a pointer to the
 * converted copy, which is freed on return. */
void glshim_TexImage2D(GLint target, GLint level, GLint components, GLint width,
                       GLint height, GLint border, GLint format, GLint type,
                       const void *pixels);
#define glTexImage2D glshim_TexImage2D

void glMateriali(GLint face, GLint pname, GLint value);
/* TinyGL has this commented out. Pixel unpacking only matters for texture
 * upload, which no hack here does, so it is accepted and ignored. */
void glPixelStorei(GLint pname, GLint param);
/* The only caller asks about GL_TEXTURE_2D, which no hack here enables. */
int glIsEnabled(GLint cap);
/* TinyGL has this commented out and always draws lines one pixel wide, which
 * is the width a hack asks for on a canvas under 640 pixels. Accepted and
 * ignored. */
void glLineWidth(GLfloat width);
/* TinyGL has this commented out of its header. Multiplies the current matrix
 * by the usual orthographic projection. */
void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
             GLdouble znear, GLdouble zfar);

/* TinyGL has these commented out of its header. Its depth test is always
 * less-than and it always modulates a texture by the vertex colour, so
 * glDepthFunc and glTexEnvf are accepted and ignored; glTexParameterf passes
 * the value on to glTexParameteri. */
void glDepthFunc(GLint func);
void glTexEnvf(GLint target, GLint pname, GLfloat param);
void glTexParameterf(GLint target, GLint pname, GLfloat param);
/* TinyGL has only the float colour calls. */
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
/* TinyGL cannot generate texture coordinates (no sphere map): accepted and
 * ignored, so vertices keep the coordinates the hack gives them. */
void glTexGeni(GLint coord, GLint pname, GLint param);
/* OpenGL 1.2's separate specular colour: TinyGL ignores a light model it does
 * not know, so these change nothing. */
#define GL_LIGHT_MODEL_COLOR_CONTROL 0x81F8
#define GL_SINGLE_COLOR 0x81F9
#define GL_SEPARATE_SPECULAR_COLOR 0x81FA

/* TinyGL defines these but its header does not declare them. */
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

#ifdef __cplusplus
}
#endif

#endif
