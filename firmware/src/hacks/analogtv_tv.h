#ifndef ANALOGTV_TV_H
#define ANALOGTV_TV_H

/* What the shared analogtv build (analogtv_tv.c) reads when it draws. A hack that
 * uses analogtv sets these in its init, since the last one to start leaves its
 * values behind; the defaults are upstream's. */
extern int analogtv_line_step; /* draw every Nth line of the picture; 1 is upstream */
extern int analogtv_view_size; /* a centred square view of this size; 0 is the whole window */
extern long analogtv_lines_drawn; /* lines drawn since the program started: for the tests */

#endif
