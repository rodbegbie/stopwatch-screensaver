---
paths:
  - "firmware/src/main.cpp"
  - "firmware/src/core/**"
  - "firmware/src/runner/**"
  - "firmware/native/**"
---

# Display, canvas and runner gotchas

Moved verbatim from the Gotchas section of AGENTS.md.

- M5GFX reads a plain `uint32_t` colour as RGB888. `TFT_RED` and friends are
  RGB565 constants. The canvas holds pixels already byte-swapped into the
  display's order (`rgb565()` returns them that way), and the display runs
  with `setSwapBytes(false)`. A swap done by M5GFX on each push cost 10 ms of
  every frame (#23: pushes of 41-45 ms became 31 ms). `px_swap()` turns a
  canvas pixel into ordinary RGB565 and back: only code that reads colour bits
  needs it (the logo loader, `dump_main.c`, tests). A hue-sweeping palette
  stays a rainbow with its bytes swapped (red, green and blue rotate), so only
  pure black and white, or a colour you know (Maze's red flame, its green
  solving path), expose a wrong order on the device. Host frames cannot,
  since the swap happens in the push; compare them with `px_swap` applied.
- A hack that takes colour bits out of pixel values itself (Substrate's alpha
  blend, in `point2rgb`) breaks on swapped pixels without a compile error and
  without a crash. `substrate_single.c` swaps at `XAllocColor` and
  `XSetForeground` so the hack sees ordinary RGB565; its golden-frame test
  fails if either is missing. Searching for `XGetPixel` finds none of this:
  dump every hack before and after a pixel-format change and `cmp` the frames.
  Pyro differs by a few hundred pixels, because it sorts projectiles by pixel
  value and so draws overlaps in another order.
- The display only gets the rows a hack drew. Every canvas write must go
  through `canvas_clear`, `canvas_point`, `hspan` or `canvas_paste_rect`
  (which record a span per row), or call `canvas_mark_dirty`, or the screen
  keeps the old pixels. The overlay text in `main.cpp` is the one deliberate
  bypass: it is written straight into `px`, marked by hand, and restored with
  `canvas_paste_rect`, which marks it again so the next push erases it.
  `test_push_present` replays every hack into a shadow display to catch a
  missed mark. M5GFX keeps a framebuffer for this panel and `endWrite` flushes
  one bounding box around everything written in a `startWrite` batch, so rows
  far apart must not share a batch (two corner pixels cost 11.7 ms together,
  0.1 ms apart). `push_plan` groups rows with a cost model fitted to device
  probes; its calibration shapes must include scattered rows, since compact
  rectangles cannot tell the models apart. Hacks that redraw unchanged pixels
  (CloudLife, Pedal) dirty nearly every row and gain little.
- Hacks run on the Arduino `loopTask`, now set to a 16 KB stack in `main.cpp`.
  A hack with big local arrays can still overflow it (Rorschach's 9.6 KB did
  at 8 KB): the device reboots on that hack's first frame, and host tests
  cannot see it. Check the serial log for "Stack canary".
- `M5.update()` samples the buttons only when it runs and keeps no edge, so a
  press during a long hack step vanishes. `main.cpp` runs a 5 ms polling task
  on core 0 into `button_latch`, which needs the pin to hold a level for 30 ms.
  Reading the pin level inside a GPIO interrupt did not work: the release
  bounced and was counted as a second press. Check `press_waited` in the log.
- The runner caps a hack's delay at 10 s, and the loop credits only the push
  (31 ms; it was 41-45 ms between the byte-order fix and #23) against it,
  because a hack's delay is its pause after drawing. Compare device numbers
  with a baseline from the same build, not with old rows: Pyro fell from 30 to
  23 fps with no change to Pyro.
- `runner_start` paints the canvas in the hack's `background` resource before
  `init`, as `screenhack.c` paints the window, or black if it has none.
  Substrate is white; every other hack asks for black or nothing.
- Overlay modes cycle on a tap: nothing, name badge, fps, battery. The badge
  and the info line share the stamp/unstamp path, and `kPatchMaxH` (100) must
  cover the tallest glyph plus its outline. The battery is polled every 30 s
  (`M5.Power.getBatteryLevel()`; negative means a failed read, so the last
  good level stays). The badge stamps 48 outline copies per frame and adds
  about 16 to 20 ms to `push=`. Read `wait=` as well as fps: a hack with delay
  slack (Galaxy) hides it, one running flat out (Squiral) does not.
- Overlay text (`main.cpp`) is stamped into the canvas, pushed, then the
  pixels under it are restored. Drawing on the display after `pushImage`
  flickered badly, because the next push erases it. The canvas must end each
  frame exactly as the hack left it.
