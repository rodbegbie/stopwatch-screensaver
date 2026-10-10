#ifndef RUNNER_QR_LAYOUT_H
#define RUNNER_QR_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Where a QR code's modules go inside a white square. scale is pixels per
 * module (0 if the code cannot fit), size is the modules' width in pixels, and
 * origin is the gap from the square's edge to the first module. */
typedef struct {
  int scale;
  int size;
  int origin;
} QrLayout;

/* The biggest whole-pixel scale that leaves `quiet` modules of white on every
 * side of a `modules`-wide code inside a `side`-pixel square. Whole pixels keep
 * every module the same size, which scanners need. */
QrLayout qr_layout(int modules, int quiet, int side);

#ifdef __cplusplus
}
#endif

#endif
