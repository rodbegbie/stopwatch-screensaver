/* Declares hsv_to_rgb and rgb_to_hsv, implemented by the copied xscreensaver
 * hsv.c. */
#ifndef XSHIM_HSV_H
#define XSHIM_HSV_H

#ifdef __cplusplus
extern "C" {
#endif

void hsv_to_rgb(int h, double s, double v, unsigned short *r,
                unsigned short *g, unsigned short *b);
void rgb_to_hsv(unsigned short r, unsigned short g, unsigned short b, int *h,
                double *s, double *v);

#ifdef __cplusplus
}
#endif

#endif
