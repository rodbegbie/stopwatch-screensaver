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
src/hacks/pong_tv.c includes the result. The taps are read once per call into
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
written once per line. The step is the wrapper's variable `pong_line_step` (1 is
upstream's behaviour, frame for frame), and each drawn line is counted in
`pong_lines_drawn` so a test can see the work done.

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
       lineno += it->threads.count * pong_line_step) {
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
    pong_lines_drawn++;
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
        for (k = 1; k < pong_line_step && lineno + k < ANALOGTV_BOT; k++) {
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
