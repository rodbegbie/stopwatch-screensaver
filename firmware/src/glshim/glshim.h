/* The GL a hack sees when it is built with USE_GL: TinyGL's API, the entry
 * points TinyGL defines but its header does not declare, and small stand-ins
 * for the GLU and GLX calls hacks make. Not derived from xscreensaver code. */
#ifndef GLSHIM_H
#define GLSHIM_H

#include "GL/gl.h"
#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

/* TinyGL reads vertex arrays when a display list is replayed, and counts
 * stride in floats; GL reads them at the draw call and counts bytes. Hacks
 * free an array right after drawing it, so their calls come here, and a draw
 * is expanded into ordinary vertex calls on the spot. */
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
#define glVertexPointer glshim_VertexPointer
#define glNormalPointer glshim_NormalPointer
#define glColorPointer glshim_ColorPointer
#define glTexCoordPointer glshim_TexCoordPointer
#define glEnableClientState glshim_EnableClientState
#define glDisableClientState glshim_DisableClientState
#define glDrawArrays glshim_DrawArrays

/* Spike experiment (-DGL_SPIKE_QUARTER): draw into a quarter of the screen to
 * separate per-pixel cost from per-vertex cost. */
#ifdef GL_SPIKE_QUARTER
void glshim_Viewport(GLint x, GLint y, GLint width, GLint height);
#define glViewport glshim_Viewport
#endif

/* Spike instrumentation: times the clear and reports per-frame work. */
void glshim_Clear(GLint mask);
#define glClear glshim_Clear

void glMateriali(GLint face, GLint pname, GLint value);
int glIsEnabled(GLint cap);

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble znear,
                    GLdouble zfar);
void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble cx,
               GLdouble cy, GLdouble cz, GLdouble upx, GLdouble upy,
               GLdouble upz);

/* There is one context and one drawable, so these only mark the canvas. */
typedef void *GLXContext;
typedef Drawable GLXDrawable;
Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
void glXSwapBuffers(Display *dpy, GLXDrawable drawable);

#ifdef __cplusplus
}
#endif

#endif
