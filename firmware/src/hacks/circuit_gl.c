/* Builds the unmodified circuit.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c.
 *
 * glMaterialfv takes four values for a colour, but Circuit gives it a
 * three-float array for its capacitor (`brown`), so TinyGL read one float past
 * it (AddressSanitizer: stack-buffer-overflow). Colours are passed on with an
 * alpha of 1, which TinyGL ignores anyway. */
#define USE_GL
#include "xlockmore.h"

static void circuit_materialfv(GLint face, GLint pname, const GLfloat *v) {
  if (pname == GL_SHININESS) {
    glMaterialfv(face, pname, (GLfloat *)v);
    return;
  }
  GLfloat colour[4] = {v[0], v[1], v[2], 1.0f};
  glMaterialfv(face, pname, colour);
}

#define glMaterialfv circuit_materialfv
#include "circuit/circuit.c"

/* free_circuit never frees the components still on the board, so each stop
 * leaked up to ten of them. A component that leaves the screen is freed as
 * DrawComponent and display() free it: its parts, then the component. */
static void circuit_free_with_components(ModeInfo *mi) {
  Circuit *ci = &circuit[MI_SCREEN(mi)];
  for (int j = 0; j < MAX_COMPONENTS; j++) {
    Component *c = ci->components[j];
    if (!c) continue;
    if (c->type == 1) free(((Diode *)c->c)->band);
    free(c->c);
    free(c);
    ci->components[j] = NULL;
  }
  free_circuit(mi);
}

__attribute__((constructor)) static void circuit_frees_its_components(void) {
  circuit_xlockmore_function_table.hack_free = circuit_free_with_components;
}

/* `-DCIRCUIT_HALF` renders at half the width and height
 * (glshim_set_render_scale), as menger_gl.c does. The board's grid lines were
 * 40 ms of a 79.8 ms step and the clear 15 ms, and both shrink with the
 * picture: 9.7 fps became 14.8, but the fine grid lines come out blocky, so it
 * is opt-in. A scale-2 hack cannot overlap its push (see the registry). */
#ifdef CIRCUIT_HALF
static void init_circuit_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_circuit(mi);
}

__attribute__((constructor)) static void circuit_renders_at_half_size(void) {
  circuit_xlockmore_function_table.hack_init = init_circuit_half;
}
#endif
