"""Build-time patch for analogtv: the ghost filter in float.

analogtv_add_signal adds every sample of the picture to the receiver's buffer
and mixes in a ghost from the four taps in `rec->ghostfir`, which the struct
holds as `double`. Each group of four samples therefore multiplies four floats
by doubles and adds them (seven software double operations plus the
conversions, since the ESP32-S3's FPU is single precision only), about 175
cycles a sample over the 238,944 samples of a frame. On the board that was
209 of Pong's 356 ms step: with the signal passes skipped it dropped to 146 ms.

The copied analogtv.c stays byte-identical to upstream. The firmware build runs
this on its text (firmware/patch_analogtv.py) and the wrapper
src/hacks/analogtv_tv.c includes the result. The taps are read once per call into
floats; their sum is then a float, so the loop does float maths only. Results
differ from the double loop only in the last bits of a float, which the test
checks against the real function at levels from 0.001 to 10,000.

Drawing every Nth line (analogtv_thread_draw_lines) is the second patch. A line
costs its three filters, its interpolation, its colour rows and a read of its
signal from PSRAM, 121 of the step's 195 ms for 200 lines. With a step of 2 only
every other line is worked out, and its row of colours is then written into the
rows of the line it skips as well as its own, each at that line's own height, so
analogtv's scanline shading (a table it builds only for the heights near the
average) is applied as upstream applies it. The picture has half the vertical
resolution and the loop does about half the work: the colour rows are still
written once per line. The step is the wrapper's variable `analogtv_line_step` (1 is
upstream's behaviour, frame for frame), and each drawn line is counted in
`analogtv_lines_drawn` so a test can see the work done.

The replacements are checked, so a change upstream fails the build loudly.
"""

OLD_TAPS = "  float hfloss=rec->hfloss;\n"

NEW_TAPS = """\
  float hfloss=rec->hfloss;
  float ghostfir0=rec->ghostfir[0], ghostfir1=rec->ghostfir[1];
  float ghostfir2=rec->ghostfir[2], ghostfir3=rec->ghostfir[3];
"""

OLD_SUM = """\
    sigr=(dp[1]*rec->ghostfir[0] + dp[2]*rec->ghostfir[1] +
          dp[3]*rec->ghostfir[2] + dp[4]*rec->ghostfir[3]);
"""

NEW_SUM = """\
    sigr=(dp[1]*ghostfir0 + dp[2]*ghostfir1 +
          dp[3]*ghostfir2 + dp[4]*ghostfir3);
"""


def _replace_once(source: str, old: str, new: str) -> str:
    found = source.count(old)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{old.strip()[:40]}...` in analogtv.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(old, new)


def float_ghost_filter(source: str) -> str:
    source = _replace_once(source, OLD_TAPS, NEW_TAPS)
    return _replace_once(source, OLD_SUM, NEW_SUM)


OLD_LOOP = """\
  for (lineno=ANALOGTV_TOP + thread->thread_id;
       lineno<ANALOGTV_BOT;
       lineno += it->threads.count) {
"""

NEW_LOOP = """\
  for (lineno=ANALOGTV_TOP + thread->thread_id;
       lineno<ANALOGTV_BOT;
       lineno += it->threads.count * analogtv_line_step) {
"""

OLD_GET_LINE = """\
    if (! analogtv_get_line(it, lineno, &slineno, &ytop, &ybot,
        &signal_offset))
      continue;
"""

NEW_GET_LINE = """\
    if (! analogtv_get_line(it, lineno, &slineno, &ytop, &ybot,
        &signal_offset))
      continue;
    analogtv_lines_drawn++;
"""

OLD_BLAST = """\
      analogtv_blast_imagerow(it, raw_rgb_start, raw_rgb_end,
                              ytop,ybot);
"""

NEW_BLAST = """\
      analogtv_blast_imagerow(it, raw_rgb_start, raw_rgb_end,
                              ytop,ybot);
      {
        /* The lines this step skips show this line's colours, in their own rows. */
        int k;
        for (k = 1; k < analogtv_line_step && lineno + k < ANALOGTV_BOT; k++) {
          int slineno2, ytop2, ybot2;
          unsigned signal_offset2;
          if (analogtv_get_line(it, lineno + k, &slineno2, &ytop2, &ybot2,
                                &signal_offset2))
            analogtv_blast_imagerow(it, raw_rgb_start, raw_rgb_end,
                                    ytop2, ybot2);
        }
      }
"""


def step_the_line_loop(source: str) -> str:
    source = _replace_once(source, OLD_LOOP, NEW_LOOP)
    source = _replace_once(source, OLD_GET_LINE, NEW_GET_LINE)
    return _replace_once(source, OLD_BLAST, NEW_BLAST)


# analogtv_load_ximage fills two rows of colours (two XColor arrays, a mask), asks
# for them all in one XQueryColors, and then reads them back to filter. That is
# 28 KB of stack on the host and about 17 KB on the board, whose loop task has
# 16 KB: it overflowed on XAnalogTV's first picture load (a boot loop). The
# arrays were also most of a picture's cost: moved to the heap (PSRAM) they cost
# 376 ms of FilmLeader's 645 ms step, since every sample was written to PSRAM
# and read back. The colours are read where they are used, one sample at a time,
# which leaves the same signal (the filter sees the same numbers in the same
# order) in the 2.3 KB `multiq` table's worth of stack. A 32-bit picture, which
# is what XGetImage makes, is read straight from its rows: XGetPixel's call and
# checks were 110 ms of that picture's cost, and so were XQueryColor's calls
# (the shim's inline xshim_query_rgb565 is the same function). A picture that is not
# there (XGetImage found no memory for it) loads nothing.
OLD_LOAD_BUFFERS = """\
  XColor col1[ANALOGTV_PIC_LEN];
  XColor col2[ANALOGTV_PIC_LEN];
  char mask[ANALOGTV_PIC_LEN];
  int multiq[ANALOGTV_PIC_LEN+4];
"""

NEW_LOAD_BUFFERS = """\
  int multiq[ANALOGTV_PIC_LEN+4];
"""

OLD_LOAD_START = """\
  img_w = pic_im->width;
  img_h = pic_im->height;
"""

NEW_LOAD_START = """\
  if (!pic_im) return 0;

  img_w = pic_im->width;
  img_h = pic_im->height;
  const int fast = pic_im->bits_per_pixel == 32 && pic_im->data;
  const unsigned char *pic_base = (const unsigned char *)pic_im->data;
  const size_t pic_stride = pic_im->bytes_per_line;
"""

NEW_LOAD_ROWS = """\
    const unsigned char *row1 = NULL, *row2 = NULL;
    if (fast) {
      if (picy1 >= 0 && picy1 < img_h) row1 = pic_base + (size_t)picy1 * pic_stride;
      if (picy2 >= 0 && picy2 < img_h) row2 = pic_base + (size_t)picy2 * pic_stride;
    }
"""

OLD_LOAD_ROWS = """\
    for (x=0; x<x_length; x++) {
      int picx=(x*img_w)/x_length;
      col1[x].pixel=XGetPixel(pic_im, picx, picy1);
      col2[x].pixel=XGetPixel(pic_im, picx, picy2);
      if (mask_im)
        mask[x] = (XGetPixel(mask_im, picx, picy1) != black);
      else
        mask[x] = 1;
    }
    XQueryColors(it->dpy, it->colormap, col1, x_length);
    XQueryColors(it->dpy, it->colormap, col2, x_length);
"""

OLD_LOAD_SAMPLE = """\
      int composite;

      if (!mask[x]) continue;
"""

NEW_LOAD_SAMPLE = """\
      int composite;
      int picx=(x*img_w)/x_length;
      XColor col1, col2;

      if (mask_im && XGetPixel(mask_im, picx, picy1) == black) continue;
      col1.pixel = !fast ? XGetPixel(pic_im, picx, picy1)
                   : row1 ? ((const unsigned int *)row1)[picx] : 0;
      col2.pixel = !fast ? XGetPixel(pic_im, picx, picy2)
                   : row2 ? ((const unsigned int *)row2)[picx] : 0;
      xshim_query_rgb565(&col1);
      xshim_query_rgb565(&col2);
"""

OLD_LOAD_RAW = """\
      rawy=( 5*col1[x].red + 11*col1[x].green + 2*col1[x].blue +
             5*col2[x].red + 11*col2[x].green + 2*col2[x].blue)>>7;
      rawi=(10*col1[x].red -  4*col1[x].green - 5*col1[x].blue +
            10*col2[x].red -  4*col2[x].green - 5*col2[x].blue)>>7;
      rawq=( 3*col1[x].red -  8*col1[x].green + 5*col1[x].blue +
             3*col2[x].red -  8*col2[x].green + 5*col2[x].blue)>>7;
"""

NEW_LOAD_RAW = """\
      rawy=( 5*col1.red + 11*col1.green + 2*col1.blue +
             5*col2.red + 11*col2.green + 2*col2.blue)>>7;
      rawi=(10*col1.red -  4*col1.green - 5*col1.blue +
            10*col2.red -  4*col2.green - 5*col2.blue)>>7;
      rawq=( 3*col1.red -  8*col1.green + 5*col1.blue +
             3*col2.red -  8*col2.green + 5*col2.blue)>>7;
"""


def load_ximage_without_row_buffers(source: str) -> str:
    source = _replace_once(source, OLD_LOAD_BUFFERS, NEW_LOAD_BUFFERS)
    source = _replace_once(source, OLD_LOAD_START, NEW_LOAD_START)
    source = _replace_once(source, OLD_LOAD_ROWS, NEW_LOAD_ROWS)
    source = _replace_once(source, OLD_LOAD_SAMPLE, NEW_LOAD_SAMPLE)
    return _replace_once(source, OLD_LOAD_RAW, NEW_LOAD_RAW)
