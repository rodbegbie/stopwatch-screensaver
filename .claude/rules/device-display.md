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
- The panel is round, so only the canvas's disc is pushed (issue #77).
  `canvas_set_disc` (in `main.cpp`) records each row's visible span: centre
  (w/2, h/2), and a pixel counts if any part of it is inside the circle.
  `push_plan` clips every dirty span to it and drops rows left empty. A hack
  with `sync_to_refresh` (Unknown Pleasures) still pushes whole rows, because
  its TE offset was swept for that push (#68). `-DPUSH_NO_DISC` turns off the
  push clip only, for an A/B: GL hacks still clear and fill only the disc
  (`gl.md`). Call `canvas_set_disc` before any `glshim_open`, which keeps a
  pointer to the spans. On the board: CloudLife's full frame 28.0 to 24.8 ms
  (26.2 to 28.4 fps), HexTrail 30.6 to 27.3 ms, Pyro 1.60 to 1.43 ms.
- Do not split a push into more batches than `push_plan`'s greedy grouping
  makes. Each extra flush in one push cost about 0.85 ms on the board, not the
  model's 17 us: a full disc frame in 19 bands took 39.9 ms against 24.7 ms in
  the greedy grouping's three, and per-row `pushImage` calls in one batch cost
  no more than one whole-image call (28.0 against 27.8 ms). The last flush of a
  push seems to finish while the hack draws its next frame; earlier ones are
  waited for. The cost model was fitted to single-flush pushes and cannot see
  this, so a split it rates cheaper can be slower.
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
- Overlay modes cycle on a tap: nothing, name badge, QR code, fps, battery. The
  badge, the QR code and the info line share the stamp/unstamp path, and
  `kPatchMaxH` (140) must cover the text block plus its outline: the badge
  is two lines, and `stampText` drops any line that would not fit. The
  battery is polled every 30 s (`M5.Power.getBatteryLevel()`; negative means a
  failed read, so the last good level stays). The QR code is encoded once at
  boot with M5GFX's `lgfx_qrcode` (`buildQr`, lowest version that holds
  `QR_URL`) into a 328 px white square, the biggest square inside the round
  screen. Each frame only copies it over the canvas and back (it reuses
  `infoPatch`, so that buffer is sized for the QR), and 328 rows are pushed
  every frame while it shows. The badge stamps 48 outline copies per frame and
  adds about 16 to 20 ms to `push=`. Read `wait=` as well as fps: a hack with
  delay slack (Galaxy) hides it, one running flat out (Squiral) does not.
- `lgfx_qrcode.h` (M5GFX's QR encoder, included at the top of `main.cpp`)
  defines `MODE_*`, `ECC_*` and `LOCK_VERSION`, which clash with other code, so
  `main.cpp` `#undef`s them straight after the include. Its init returns
  nonzero when the text is too long for a version; `buildQr` tries versions 1
  to 40 and, if none fits, leaves `qrPixels` null so the tap cycle skips QR.
- Overlay text (`main.cpp`) is stamped into the canvas, pushed, then the
  pixels under it are restored. Drawing on the display after `pushImage`
  flickered badly, because the next push erases it. The canvas must end each
  frame exactly as the hack left it.
- Tearing. The panel scans its memory 60 times a second and `pushImage` is not
  synchronised with it, so for a refresh the top of the screen shows one
  picture and the rest another. A thin line that moves a few pixels a frame can
  fall in the gap and vanish for a frame (Unknown Pleasures' flicker: a flat
  stretch of one line gone for about 17 ms). The panel's TE line is on GPIO 38
  (`OLED_TE`), M5GFX programs it (`0x35`, `0x44` at line 466) and nothing waits
  for it. It pulses every 16.82 ms for 1.1 ms. A full push is 20.6 ms copying
  into M5GFX's framebuffer plus 11.7 ms flushing to the panel (split by an
  outer `startWrite()` around the `pushImage`; the flush happens at the
  `endWrite`). `HackEntry::sync_to_refresh` (`XLOCKMORE_HACK_SYNCED`) starts
  the push `kTeSyncOffsetUs` after TE rises (`runner/te_sync.h`). Only
  Unknown Pleasures asks, because it costs up to a refresh of latency a frame.
- The offset (7.4 ms) is the middle of the clean window, found by a sweep:
  eight offsets across one refresh, 3 s each, the step number stamped on the
  screen, filmed at 120 fps. 3.2 to 9.5 ms had no vanished line in 39 updates;
  1.1 and 15.8 ms did, with the seam at film row 495 and film row 268: about 107
  film rows per millisecond (the camera frame is taller than the panel; the
  466-row panel scans about 28 rows per millisecond), so the bad window is only
  about 5 ms wide. It is valid for a full-frame push (the 20.6 ms copy). A push
  of another size moves the flush and the sweep would have to be repeated: the
  QR overlay's 328-row push is one, so a seam may show while the code is up.
  `-DTE_DIAG` logs the TE period and where pushes start.
- `te_sync.c` trusts few edges. Once it knows the period it accepts a rising
  edge only within period/16 of a whole number of periods since the last, so a
  missed edge or a stray pulse cannot move the lock; four misfits in a row drop
  the lock and relearn it. An edge further ahead than one period is stale (the
  32-bit microsecond counter can pass half its range with the line dead), and
  waiting stops 100 ms after the last edge. The ISR is placed in IRAM, but
  Arduino's pin dispatcher and `gpio_get_level` are in flash; that is safe only
  because `CONFIG_ARDUINO_ISR_IRAM` is unset. The run log's `sync=` is the wait
  for the edge and is left out of `push=`: compare `push=` across hacks, and add
  `sync=` to get the frame's latency.
- Reading a film of the display: iPhone slo-mo files store their first and
  last seconds at normal speed, so the label changes every ~100 frames there
  and every 360 in the 120 fps middle. Stabilise against the scenery (four
  corner patches; a mask edge around the display pins phase correlation at
  zero), level the rows, and count a vanish only when a line is missing across
  at least three 24 px windows on one row and not in a settled frame of the
  same update. A detector that finds nothing must also find the event you know
  is there: mine reported zero until a known vanish was used as a control, and
  reported many false events on steep line stretches until settled frames were.
- Every push also dims the screen: the dimmest band falls to 0.86 of its
  settled brightness for about 25 ms (three camera frames at 120 fps), at 5 Hz,
  with or without TE sync (0.860 synced, 0.870 unsynced). It is probably the
  "whole screen redraws" flicker. Its cause is not known. Rod sees no dimming
  with the sparser build (20 rows of 50 points, a step of about 91 ms against
  165), so it may track how much is lit or how long the copy runs; not yet
  filmed (issue #68).
- A hack's returned delay is waited after the push, and only the push counts
  against it, so its own step adds to the frame time: to hold a rate, return
  the target minus the step (`ad_flyingtoasters.c`: 19.5 fps became 28.9).
- PSRAM cost model (`diag/psram_bench.h`, `-DPSRAM_BENCH`): the data cache is
  32 KB, 8-way, 32-byte lines over 80 MHz octal PSRAM, and every miss costs
  about 0.92 us whatever the write size (sequential about 35 MB/s). Firmware
  switches to 64-byte lines (4-way) when a hack starts (`dcache.h`, through
  ROM calls the SDK header says an application should not make): sequential
  passes get about 28% faster and the full-frame push drops from 29 to
  24.8 ms, but a lone short write costs 1.3 us a miss. Pinned A/B, two
  rounds each: Deluxe, Interference, Kumppa, Abstractile +9%, HexTrail +17%,
  Marbling +7%, WhirlWindWarp +6%, Qix and Hexadrop +4%, Gears +3%; Lisa,
  Substrate, XRaySwarm, Pyro, RotZoomer flat. A `sync_to_refresh` hack
  keeps 32 bytes (Unknown Pleasures' offset was swept against the old copy
  time) and so does a hack with `dcache_line32` set in its `HackEntry`
  (Discrete: -5% fps, step 67.9 to 72.8 ms, many short spans). The other core
  is parked in IRAM with interrupts off while the cache is reconfigured
  (`dcache_with_other_core_parked`, over `esp_ipc_call`), as the SDK does for
  flash writes: `noInterrupts()` stops only the calling core.
  `-DDCACHE_LINE32` turns it off for every hack. Compare builds only
  pinned: a free-running lap changes random parameters per visit and showed
  false regressions of 3x on five hacks. A ROM prefetch setting (trigger 1,
  size 0) changed nothing; other settings are untried.
- A hack with `repaints_all` (`hack_entry.h`) draws its next frame while core 0
  pushes the last (`runner/push_overlap.[ch]`, issue #54): two canvases swap
  each frame, the worker pushes the finished one, and the loop credits that
  push's measured duration against the hack's delay. Pinned A/B on the board,
  plain to overlapped: Deluxe 14.0 to 19.0 fps, Compass 18.8 to 29.1, FuzzyFlakes
  18.4 to 28.9, Moire2 16.5 to 26.4, Scooter 22.1 to 28.4; eighteen hacks are
  flagged, five of them GL (grep `repaints_all`, and
  `XLOCKMORE_HACK_REPAINTS_ALL`). The step slows by about 5 ms (both cores
  share the PSRAM cache), so the frame is roughly the step, not the longer of
  the two. Rules:
  - Flag a hack only if it never reads the canvas it starts from. The noise
    test and the real-swap test in `test_push_overlap` run every flagged hack
    for 300 frames; 40 frames missed Halo, which the 300-frame run caught. GL
    hacks pass since #118 (see `gl.md`); GraphStat's swap run still fails.
  - Only a gain of 5% or more on a pinned A/B earns the flag. Deco and CCurve
    are paced by their own delays and gained 0 and 2.6%.
  - A hack whose `HackEntry` comes from `XSCREENSAVER_MODULE` in the
    byte-identical copy (Truchet, +25%) needs a wrapper to be flagged.
  - Any overlay (name, badge, QR, fps, battery) makes the push synchronous
    while it shows, so the fps overlay reads the plain rate. Read `fps=` in
    the serial log for the overlapped one.
  - Three bugs the spike hit: a redraw from the wait loop must not dispatch a
    second push (it blocked 25 ms), the overlay text canvas wraps one buffer
    and must follow `canvas.px`, and the newest-frame pointer must be dropped
    when a hack starts or Forest's first frame swapped the old picture back.
  - `-DPUSH_NO_OVERLAP` turns it off for every hack (the A side of `ab_flash`).
    The spare canvas is 434 KB of PSRAM; if it will not allocate the firmware
    pushes synchronously.
