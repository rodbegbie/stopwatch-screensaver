/* The shared-memory image calls and visual_depth, over plain XImages. Written
 * for this project; not derived from xscreensaver's utils. */
#include <stdlib.h>

#include "visual.h"
#include "xshm.h"

int visual_class(Screen *screen, Visual *visual) {
  (void)screen;
  (void)visual;
  return TrueColor;
}

void visual_rgb_masks(Screen *screen, Visual *visual, unsigned long *red_mask,
                      unsigned long *green_mask, unsigned long *blue_mask) {
  (void)screen;
  (void)visual;
  if (red_mask) *red_mask = 0xF800;
  if (green_mask) *green_mask = 0x07E0;
  if (blue_mask) *blue_mask = 0x001F;
}

int visual_depth(Screen *screen, Visual *visual) {
  (void)screen;
  (void)visual;
  return 16;
}

int visual_pixmap_depth(Screen *screen, Visual *visual) {
  (void)screen;
  (void)visual;
  return 16;
}

XImage *create_xshm_image(Display *dpy, Visual *visual, unsigned int depth,
                          int format, XShmSegmentInfo *shm, unsigned int w,
                          unsigned int h) {
  (void)shm;
  XImage *im = XCreateImage(dpy, visual, depth, format, 0, NULL, w, h, 16, 0);
  if (!im) return NULL;
  im->data = (char *)calloc(h, (size_t)im->bytes_per_line);
  if (!im->data) {
    XDestroyImage(im);
    return NULL;
  }
  return im;
}

void put_xshm_image(Display *dpy, Drawable d, GC gc, XImage *im, int src_x,
                    int src_y, int dest_x, int dest_y, unsigned int w,
                    unsigned int h, XShmSegmentInfo *shm) {
  (void)shm;
  XPutImage(dpy, d, gc, im, src_x, src_y, dest_x, dest_y, w, h);
}

void destroy_xshm_image(Display *dpy, XImage *im, XShmSegmentInfo *shm) {
  (void)dpy;
  (void)shm;
  XDestroyImage(im);
}
