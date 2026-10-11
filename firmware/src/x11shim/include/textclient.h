/* Stand-in for xscreensaver's utils/textclient.h. xmatrix's text modes read a
 * program or a phone-trace script through it. The device has no pipe or
 * terminal, so there is never a text source: textclient_open returns NULL,
 * which xmatrix already treats as "no text". Not derived from xscreensaver
 * code. */
#ifndef XSHIM_TEXTCLIENT_H
#define XSHIM_TEXTCLIENT_H

#include "x11shim/xshim.h"

typedef struct text_data text_data;

static inline text_data *textclient_open(Display *dpy) {
  (void)dpy;
  return 0;
}
static inline void textclient_close(text_data *td) { (void)td; }
static inline void textclient_reshape(text_data *td, int pix_w, int pix_h,
                                      int char_w, int char_h, int max_lines) {
  (void)td, (void)pix_w, (void)pix_h, (void)char_w, (void)char_h,
      (void)max_lines;
}
static inline int textclient_getc(text_data *td) {
  (void)td;
  return -1;
}
static inline Bool textclient_puts(text_data *td, const char *s) {
  (void)td, (void)s;
  return False;
}
static inline Bool textclient_putc_event(text_data *td, XKeyEvent *event) {
  (void)td, (void)event;
  return False;
}

#endif
