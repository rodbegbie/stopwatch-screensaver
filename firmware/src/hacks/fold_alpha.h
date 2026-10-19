/* TinyGL has no alpha, so a hack that blends `GL_SRC_ALPHA, GL_ONE` folds the
 * vertex alpha into its colour: an additive blend cannot tell the difference.
 * Real `glColor4f` clamps alpha to [0, 1], and TinyGL masks a colour channel
 * that goes past full instead of saturating it, so the alpha is clamped here
 * (GLMatrix gives a spinner glyph 1.5 times its brightness). Plain floats, so
 * a host test can call it. */
#ifndef HACKS_FOLD_ALPHA_H
#define HACKS_FOLD_ALPHA_H

static inline void fold_alpha(float r, float g, float b, float a, float out[3]) {
  if (a > 1.0f) a = 1.0f;
  if (a < 0.0f) a = 0.0f;
  out[0] = r * a;
  out[1] = g * a;
  out[2] = b * a;
}

#endif
