# Flying Toasters from After Dark 2.0: design

## Intent

Bring back After Dark's Flying Toasters, the classic 1990 Mac screensaver,
on the StopWatch, as faithfully as the hardware allows: the original art,
moving to the original rules. Rod owns a copy of After Dark 2.0 on an 800K
floppy image. This is a nostalgia piece for his own badge, in the same spirit
as Pyro was for the xscreensaver hacks.

The art and code are Berkeley Systems' copyright and carry no licence to
redistribute. The repo is public and MIT. So the design follows the
ScummVM model: **our code reproduces the behaviour; the art is read at build
time from the user's own disk image, which is never committed.** Anyone
without the image gets a build with no toasters in it. The Internet Archive
holds a copy of the disk, [After Dark 2.0v (800K)][ia-disk], and our docs
link there rather than shipping it.

[ia-disk]: https://archive.org/details/after-dark-2.0v-800k

## Goals

- A new hack, `ad-flyingtoasters`, that reproduces Flying Toasters' motion,
  spawning, wing-flap animation, collision nudging and toast doneness,
  worked out from the original 68000 code (see "Behaviour" below).
- The original sprites, extracted at build time from
  `vendor/after-dark-v2.0v-800k`.
- Each time the hack starts it picks, at random:
  - the **1-bit** or the **16-colour** art (even odds);
  - the toast **doneness**: Light, Medium or Dark (even odds).

  This is Rod's request: the original made these user settings.
- Chunky pixels by default: each sprite pixel is drawn as a 2×2 block (128×128
  on screen), with 6 flyers in a 233×233 logical world that runs the original
  rules unchanged. Rod chose this after seeing both sizes on the device (2× at
  about 21 fps, 1× at about 29). The original size, 64×64 with 10 flyers, is
  behind `-DAD_TOASTERS_SCALE=1`.
- Builds without the disk image still succeed, and simply leave the hack out.

## Non-goals

- Sound, for now. The board has a speaker, and the original's flap
  soundtrack is a possible future enhancement, but nothing in the project
  plays sound yet. The rule is recorded below so that work can start from
  it; the extraction tool need not pull the `snd` resources yet.
- User settings at run time (count, colour, doneness sliders). Count stays at
  the original's default; colour and doneness are random per start.
- Other After Dark modules. The disk holds 30-odd; they may follow later, and
  the extraction tool should not make that harder, but this design covers
  only Flying Toasters.
- Porting xscreensaver's `flyingtoasters` (jwz's OpenGL tribute). It is a
  different look and an XL GL port; it stays unported.

## Anti-goals

These would make it a failure even if it worked:

- **Any Berkeley Systems bytes in git**: no sprites, no PICT data, no
  disassembly listings, no test fixtures cut from the disk. That includes
  test data and docs. Behavioural descriptions and code offsets (as in this
  document) are fine.
- A build that fails for someone without the disk image.
- Publishing a firmware binary that contains the art. (None are published
  today; keep it that way for builds that include the toasters.)
- "Improving" the original's look: no alpha masks, no smoothing, no scaling.
  The black box around each sprite is part of how the original works, and
  stays.

## What is on the disk

Found during the investigation that led to this design (scratch work, not
committed):

- `vendor/after-dark-v2.0v-800k` is 838,400 bytes: 1600 sectors of 524
  bytes, each 512 data bytes followed by a 12-byte tag (a raw floppy dump with
  sector tags, not DiskCopy 4.2). Dropping the tags gives an 819,200-byte HFS
  volume named "After Dark™ Disk".
- `After Dark Files/Flying Toasters` is an `ADgm`/`ADrk` file with an empty
  data fork. Its resource fork holds:

  | Resource | Contents |
  | --- | --- |
  | `PICT` 128-131 | 1-bit toaster frames 0-3, 64×64 |
  | `PICT` 132 | 1-bit toast, 64×64 |
  | `PICT` 228-231 | 16-colour toaster frames 0-3, 64×64 (PICT v2, 4-bit indexed, own colour table) |
  | `PICT` 232 | 16-colour toast, 64×64 |
  | `ADgm` 0 | the module's 68000 code (12,402 bytes, Think C, MacsBug names intact) |
  | `snd` 128-130 | "Flap 1", "Flap 2", "Mix 1" |
  | `sVal`/`xVal`/`sUnt` | control defaults: count slider 54, toast doneness 0 (Light), colour on |
  | `TEXT` 1000, `STR` 128 | credits |

- All ten PICTs are a single PackBitsRect opcode (`0x98`). The 1-bit ones
  are PICT v1 whose bitmap is 72 px wide with 4 px either side outside the
  64×64 `picFrame`; draw them through the frame, as `DrawPicture` would, and
  the extra columns fall away. In the 1-bit art a set bit is black: the
  pictures are white line art on black.
- Credits from the module: Jack Eastman, Bruce Burkhalter and Patrick Beard,
  artwork by Tomoya Ikeda, ©1990-91 Berkeley Systems Inc. The hack's header
  comment should name them.

## Behaviour

Read from the `ADgm` code. Offsets are into that resource. Everything here
is **certain** (read directly from the code) unless marked otherwise.
Coordinates are QuickDraw's: x right, y down. Rect is (top, left, bottom,
right).

### Pacing

- One simulation step per two 60 Hz ticks: at most **30 steps a second**
  (`DoDrawFrame` 0xd90: step when `TickCount() >= next`, then
  `next = TickCount() + 2`). A late step is not caught up.
- Every step moves and draws every flyer.

### Flyers

- Slot count `n = max(1, countSlider / 5)` (0x3e2). The default slider of
  54 gives **10**, which is the original's count. We use 10 at 1× and 6 at
  2× (the default), where each sprite covers four times the screen area.
- Each flyer owns a **70×70 rect**: the 64×64 art plus a 6 px black strip
  along its top and a 6 px strip down its right. In sprite-local terms the
  rect is (top −6, left 0, bottom 64, right 70) and the art sits at
  (0..63, 0..63).
- Creating a flyer (`CreateFlyer` 0xdf8):

  ```c
  tries = 4;
  do {
      rect = running ? random_edge_rect(bounds) : random_inside_rect(bounds);
  } while (collides(i, rect) && --tries);
  if (tries == 0) return NULL;          /* slot stays empty; retried next step */
  r = abs(Random() % 100);              /* 0..99 */
  if (r >= 75) return toast(rect);      /* speed 1 */
  return toaster(rect, speed = r >= 50 ? 3 : r >= 25 ? 2 : 1);
  ```

  So 25% each: toast, slow toaster, medium toaster, fast toaster. Speed is
  fixed for a flyer's life.
- `random_inside_rect` (0x1538), used once, at start: the rect fully
  inside the bounds, `top = abs(Random() % (H − 70))` first, then
  `left = abs(Random() % (W − 70))`.
- `random_edge_rect` (0x1458), every later spawn: with even odds, either
  **just above the top edge** (`rect.bottom = bounds.top`, left anywhere in
  `abs(Random() % W)`) or **just past the right edge**
  (`rect.left = bounds.right`, top anywhere in `abs(Random() % H)`).
- Toolbox `Random()` returns −32767..32767 and `%` keeps the sign, hence the
  `abs`. Use our `random()`; the tiny bias towards 0 need not be copied.

### Motion

Per step (`AdvanceFlyers` 0x1066), in two passes over the slots:

```c
for (i = 0; i < n; i++) {                       /* pass 1 */
    if (!flyer[i]) flyer[i] = create_flyer(i);
    if (flyer[i]) offset(&flyer[i]->r, -2, +1); /* base step, unchecked */
}
for (i = 0; i < n; i++) {                       /* pass 2 */
    f = flyer[i]; if (!f) continue;
    if (!intersects(f->r, bounds)) { flyer[i] = NULL; continue; }
    s = f->speed - 1;
    if (s > 0) {
        cand = f->r; offset(&cand, -2 * s, +s);
        if ((p = collision(i, cand))) {
            cand = f->r; offset(&cand, p.h, -2 * p.v);
            if (!collision(i, cand)) f->r = cand;   /* else keep base step */
        } else f->r = cand;
    }
    draw(f);                                    /* drawn straight after its move */
}
```

- Net motion is `speed × (−2, +1)` px per step: down and to the left at a
  2:1 slope. Speed 1 is 60 px/s left and 30 px/s down at 30 steps/s.
- Only speed 2 and 3 toasters check for collisions. Toast and speed-1
  toasters move blindly.
- `collision(i, cand)` (0xf6e) looks for the first other flyer `o` whose
  rect intersects `cand`, and returns a nudge `p`:
  - if `|o.top − cand.top| < |o.left − cand.left|` (mostly beside it):
    `p = (v −1, h 0)`, which as applied above drops the flyer 2 px further
    down this step;
  - otherwise `p = (v 0, h cand.top < o.top ? −1 : +1)`: 1 px further left
    if it is above `o`, 1 px less far left if below.
- A flyer is removed once its rect no longer touches the bounds. Its slot
  respawns on the next step's pass 1.
- `bounds` is the whole screen. For us it is the 233×233 logical world at
  2× (the default) and the 466×466 square at 1×. Spawning and dying in the
  circle's hidden corners is faithful and harmless.

### Animation

`Toaster.Draw` (0x1360) draws the current frame, then advances:

```c
blit(frames[frame], f->r);
if (flap_down) { if (frame == 0) { frame = 1; flap_down = 0; } else frame--; }
else           { if (frame == 3) { frame = 2; flap_down = 1; } else frame++; }
```

- The sequence is **0 1 2 3 2 1 0 1 …**, one frame per step: 5 wing beats a
  second at every speed.
- Every toaster starts at frame 0 going up. There is no random phase.
  Toasters differ in phase only because they spawn on different steps.

### Drawing

- **There is no mask.** Each flyer is an opaque copy of its 70×70 rect,
  black included. No step moves a flyer more than 6 px left or down, so the
  black strips on the top and right wipe the trail of the previous position.
  There is no erase pass and no back buffer.
- Overlap is avoided by spacing (spawn retries, nudges), not hidden by
  masking. When flyers do overlap, the later slot's black box covers the
  earlier one. That is faithful; keep it.
- Draw order is slot order, higher slots on top.
- At start the screen is filled black and every flyer drawn once (`DoBlank`
  0xc40), which also advances each toaster to frame 1.
- The original clips one pixel row off the top of each monitor
  (`FixClipping` 0x1678); the reason is unknown and we skip it.

### Colour and doneness

- 1-bit mode draws PICTs 128-132 as white on black. 16-colour mode draws
  228-232 through their colour tables.
- Doneness applies **only in colour mode**, once at start, to the toast's
  pixels (0x9d4-0xa3c): QuickDraw's `subPin` mode, `c' = max(c − k, 0)` per
  channel, with:

  | Doneness | k (16-bit RGB) | About, in 8-bit |
  | --- | --- | --- |
  | Light | none | unchanged |
  | Medium | 0x4fff, 0x4fff, 0x4fff | 80, 80, 80 |
  | Dark | 0x7fff, 0x6fff, 0x6fff | 128, 112, 112 |

  Do the subtraction on the toast's RGB888 before converting to RGB565.
  Black stays black.
- In 1-bit mode doneness has no effect, as in the original. The random pick
  is still made, so the log can show it, but the toast looks the same.

### Sound (future enhancement)

Not built in this design. With After Dark's sound extension present,
Flap 1, Flap 2 and Mix 1 play back to back, each next one picked at random
but never the same twice running, starting with Flap 1. It is a soundtrack,
not tied to any toaster.

## How it gets built

### Extraction (build time)

- A uv script, `tools/after_dark_sprites.py`, reads the disk image and writes
  a C header of the ten sprites:
  - strips the 12-byte sector tags when the image is 1600 × 524 bytes; also
    accepts a plain 819,200-byte volume;
  - reads the HFS volume and the module's resource fork;
  - decodes the ten PICTs (PackBitsRect, 1-bit and 4-bit indexed with a
    colour table, drawn through `picFrame`);
  - writes 64×64 RGB888 for the colour frames and 1 bit per pixel for the
    mono ones, plus the image's SHA-256 in a comment.
- A PlatformIO pre-script, `firmware/after_dark_sprites.py`, modelled on
  `pacman_sprites.py`:
  - image absent: prints one line saying the toasters are left out, and
    does nothing else;
  - image present: regenerates the header under `$BUILD_DIR/generated/`
    when it is older than the image or the tool, writes through a
    `.partial` file, then adds the include path and defines
    `HAVE_AFTER_DARK_TOASTERS`;
  - image present but extraction fails: the build fails loudly. A silently
    missing hack would hide a broken tool.
- It is added to `extra_scripts` in all three envs (`stopwatch`, `native`,
  `dump`).
- The image path is `vendor/after-dark-v2.0v-800k`. The environment
  variable `AFTER_DARK_IMAGE` overrides it (decided by Claude while planning)
  so the absent and broken cases can be built without touching the shared,
  symlinked `vendor/`.

### The hack

- `firmware/src/hacks/ad_flyingtoasters/` holds our own code (MIT), not a
  copy of an xscreensaver hack, so the byte-identical rule does not apply
  and it needs no `THIRD_PARTY_NOTICES.md` row: nothing in the repo is
  copied. Its header comment credits the original authors and says the art
  comes from the user's own disk at build time.
- Split the simulation (slots, spawning, motion, collision, flap state,
  doneness maths) from the drawing and from the sprite data. Then host tests
  can drive the simulation with stand-in sprites, whether or not the disk is
  present.
- It is a `HackEntry` like the others (init, draw, free), registered in
  `g_hacks[]` under `#ifdef HAVE_AFTER_DARK_TOASTERS`, **at the end of the
  list**, so every other hack keeps its index for `dump` and the scorer.
- Naming (Rod's): the hack is `ad-flyingtoasters`, which acknowledges its
  After Dark origin and keeps it apart from xscreensaver's `flyingtoasters`.
  That string is the entry's `name`: what the overlay shows, the serial lines
  print and `START_HACK` matches, case-insensitively, so pin it with
  `START_HACK="ad-flyingtoasters"`. C identifiers cannot hold a hyphen, so
  the symbol is `ad_flyingtoasters_hack` and the directory
  `ad_flyingtoasters/`. The scorer reads registered hacks from the
  `&<name>_hack` symbols in `g_hacks[]` (`score_hacks.py`), so it sees
  `ad_flyingtoasters`, which matches no xscreensaver hack.
- **The two picks need real entropy** (decided by Claude). Nothing seeds
  `random()` from entropy on the device, so `random()` replays the same
  sequence after every boot, and a pinned build would make the same picks
  every time. Take the art and doneness picks from `esp_random()` on the
  device, behind a small function the host build replaces with `random()`
  (host tests then stay deterministic after `srandom(1)`). Do not reseed
  `random()` itself: every other hack draws from it, and changing its
  sequence would change what they see. Flyer placement and speeds keep
  using `random()`, like the original's `Random()`.
- Two build flags force the picks, for looking at each combination on the
  device: `-DTOASTERS_ART=mono|colour` and `-DTOASTERS_DONENESS=light|medium|dark`
  (exact spelling is the builder's). Without them the picks are random.
- At init: pick 1-bit or colour, pick a doneness, build the five 70×70
  sprites (art plus black strips) in RGB565, scatter the flyers (6 at 2×,
  10 at 1×), fill the
  screen black and draw them all once. Log the picks on one line (for
  example `ad-flyingtoasters: colour, doneness=dark`) so a serial capture says what
  the screen should show.
- Background resource black.

### Choices left to the builder

- How sprites reach the canvas: `XPutImage` through the shim, or a small
  blit straight into the canvas with `canvas_mark_dirty`. Either is fine if
  it stays opaque and marks only the touched rows dirty. At 2× (the
  default) it is six 140×140 blits a step, about 118,000 pixels; at 1× it is
  ten 70×70 blits, about 49,000. Dirty rows keep the push to roughly the
  rows the flyers cover.
- Pacing: the runner waits the returned delay *after* the push, and step
  time is not counted against it (see `main.cpp`). Aim for 30 steps a
  second, and report what the device actually achieves. Measured: the 2×
  default runs at about 21.5 fps (worst window 19.8) because `XPutImage`
  copies pixel by pixel (issue #82); 1× holds about 29.
- The HFS reader: the `machfs` package worked in the investigation. Check
  its licence before relying on it (it is a tool dependency fetched by uv,
  not copied into the repo); a small reader of our own is the fallback.
- The layout of the generated header.

## Testing

Tests first, as everywhere in this repo, and none may contain Berkeley
bytes.

- Python (`tools/tests`):
  - the PICT decoder and resource-fork parser, against small PICTs and
    resource forks the test builds itself;
  - sector-tag stripping on a synthetic image;
  - one optional test that runs the tool on the real image when it is
    present and checks five colour and five mono 64×64 sprites come out.
    Skipped, not failed, without it.
- C (native, Unity, with stand-in sprites such as solid colours):
  - flap order 0 1 2 3 2 1 0;
  - per-step motion for speeds 1-3;
  - the collision nudge, both branches, and the keep-base-step fallback;
  - removal once off screen and respawn at the top or right edge;
  - initial placement fully inside the bounds;
  - at most 4 placement tries, with the slot left empty and retried;
  - the doneness subtraction, including clamping at 0;
  - the 1-bit expansion to white on black;
  - **trail wiping**: after several steps, every non-black pixel lies inside
    some flyer's current rect.
- Mutation-check the new tests, as `AGENTS.md` asks.
- When the image is present, the suite's every-hack run (3000 frames under
  ASan, leak check across restarts) covers `ad-flyingtoasters` too.
- Under `#ifdef HAVE_AFTER_DARK_TOASTERS`, add the hack to
  `test_hacks.c`'s expected names and its baselines.
- On the device: flash a build pinned with `START_HACK="ad-flyingtoasters"`
  and rotation off (ask Rod first), capture `fps=` and `rows=` over a few
  minutes, and report the mean and the worst window. Restarts make random
  picks, but to show Rod each combination on purpose, flash with the force
  flags: colour Light, Medium and Dark, then 1-bit. Only Rod can confirm the
  look; tell him what to check: the colour toast's shade in the logged
  doneness, and white line art in 1-bit mode.

## Licensing and housekeeping

- Nothing extracted from the disk is committed. The generated header lives
  in the git-ignored build directory, and `vendor/` is already ignored.
- The README gets a short note: Flying Toasters needs your own copy of the
  After Dark 2.0 disk at `vendor/after-dark-v2.0v-800k`, links to the
  [Internet Archive copy][ia-disk], and says it is left out without it.
- `AGENTS.md` gets the new pre-script, the `HAVE_AFTER_DARK_TOASTERS` define,
  and the rule that `ad_flyingtoasters_hack` sits at the end of `g_hacks[]`.
- Check that `tools/score_hacks.py` and `tools/speed_backtest.py` cope with
  a registered name that is not an xscreensaver hack. The backtest probes
  every registered hack standalone, from xscreensaver's sources.

## Open questions

- The count slider's range is assumed to be 0-100 (After Dark's usual).
  It only matters if we ever expose the count; with the default 54 the
  original's answer is 10 either way (we draw 6 at 2×, 10 at 1×).
- How often flyers overlap in practice was not simulated. The host tests
  and the device run will show it; it is faithful either way.
