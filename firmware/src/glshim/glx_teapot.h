/* The device's copy of the Utah teapot table, deflated at build time (see
 * tools/teapot_table.py). Ours. */
#ifndef GLSHIM_GLX_TEAPOT_H
#define GLSHIM_GLX_TEAPOT_H

#include "GL/gl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Unpacks the table into a buffer held until xshim_teapot_release. NULL when
 * memory runs out. unit_teapot calls both itself. */
const GLfloat *xshim_teapot_unpack(void);
void xshim_teapot_release(void);

#ifdef __cplusplus
}
#endif

#endif
