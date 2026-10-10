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

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
/* TinyGL has drawn into the canvas already; this marks all of it dirty. */
void glXSwapBuffers(Display *dpy, GLXDrawable drawable);

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble znear,
                    GLdouble zfar);
void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble cx,
               GLdouble cy, GLdouble cz, GLdouble upx, GLdouble upy,
               GLdouble upz);

void glMateriali(GLint face, GLint pname, GLint value);
/* The only caller asks about GL_TEXTURE_2D, which no hack here enables. */
int glIsEnabled(GLint cap);

/* TinyGL defines these but its header does not declare them. */
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

#ifdef __cplusplus
}
#endif

#endif
