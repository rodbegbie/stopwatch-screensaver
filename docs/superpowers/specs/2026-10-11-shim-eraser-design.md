# Shim eraser: a dwell on the finished picture, then a quick wipe

Issue #86. Written 2026-10-11 after talking it through with Rod.

## Why

xscreensaver hacks end a picture by calling `erase_window`. Upstream's
version waits, then wipes the picture away with an effect. Our shim clears
the window at once and returns NULL, so the order is reversed: the picture
goes, then the wait. Celtic waits 5 s (`delay2`) after the call. On the board
that 5 s is black, and the finished design never gets looked at.

Rod saw Celtic on another device hold the design on screen and then wipe it.
That is the behaviour to bring back.

## Goals

- The first `erase_window` call draws nothing and returns a state, so a
  hack's wait happens with the finished picture still on screen.
- Later calls draw the next step of a wipe. The call after the wipe's end
  clears the window and returns NULL, as upstream does.
- A few cheap wipes, chosen at random, each about one second.
- No leak when a hack stops mid-wipe, for every hack, including the six
  that never call `eraser_free`.

## Non-goals

- Upstream's twelve effects, or its `eraseMode` and `eraseSeconds`
  resources.
- A look-alike of `utils/erase.c`. Shim code is ours: nothing is copied.
- Editing any copied hack. They stay byte-identical.

## Anti-goals

- A slow wipe. Rod's one wince: "a reeaeaallly slow wipe". Anything that
  takes visibly longer than about a second, or that drops the frame rate
  during the wipe, is a failure even if it is correct.
- A wipe that adds a full-frame push per step where a partial push would do.

## What the callers do

All eleven registered callers already follow upstream's contract: store the
return in `st->eraser`, call again while it is non-NULL, wait 10 to 33 ms
between steps. Read for this design:

- Celtic, Coral, Critical, Epicycle, Helix, Maze, Pedal, Rorschach and
  XSpirograph call `erase_window` directly.
- `xlockmore.c` calls it for every xlockmore hack with `needs_clear`, waits
  33 ms while the state is live, and clears directly when nothing has been
  drawn yet.
- Only Pedal, Celtic, Epicycle and `xlockmore.c` call `eraser_free`.
  Coral, Rorschach, Critical, Maze, Helix and XSpirograph do not, so a
  stopped hack would leak a live state.
- Rorschach and Critical start drawing the next picture in the call that
  starts the erase. The first call draws nothing, so this is harmless
  there. A later step could wipe over fresh drawing, which is upstream's
  behaviour too.

## Design

### The state and the contract

`erase_window(dpy, w, st)` in `x11shim/helpers.c` (or a new
`x11shim/erase.c`, the builder's choice) becomes a small state machine:

- `st == NULL`: allocate a state, pick an effect at random, draw nothing,
  return the state. No start time yet.
- `st != NULL`, first step: record the start time now, draw ratio 0, return
  `st`.
- `st != NULL`, time left: draw the next step of the effect, return `st`.
- `st != NULL`, time up: finish with `XClearWindow`, free the state, return
  NULL.

The start time is taken on the second call, not the first, because the dwell
sits between them. Celtic returns after the first call and is not called
again for `delay2` (5 s). A start time taken on the first call would find
the one-second wipe already over, and the picture would just vanish.

`eraser_free(st)` frees the state and takes it off the display's list.

### Progress comes from the clock, not the call count

Each step computes its ratio from elapsed time over the duration. A caller
that steps every 10 ms and one that steps every 33 ms both finish in about
one second, and a slow frame cannot stretch the wipe. This is what keeps the
wipe from running slow on a busy board. The duration is one constant, about
one second.

The shim's `double_time()` (`x11shim/doubletime.c`) reads the wall clock and
already has an override, `xshim_set_clock`. The host tests and the `dump`
program never sleep for a hack's returned delay, so on the wall clock a
wipe would not end inside a 3000-frame run, and pinned hashes would depend on
host speed. `test_hacks` already installs a fake clock through
`xshim_set_clock` that moves 1/30 s a reading, so the eraser reads
`double_time()` and is deterministic there. `dump` installs no clock, so it
gets the same fake one. This is my call, and it replaces my first idea of a
separate eraser clock hook, which the existing fake clock made unnecessary.

### Effects

Four, each a handful of fills in the window's background pixel:

- Venetian blinds: bars grow from thin slats to solid.
- Circle wipe: a growing disc, centred on the window, clearing outward.
- Fizzle: a dissolve that fills random blocks until all are cleared.
- Random lines: lines in the background colour, thickening over time.

The fill colour is the display's `window_background`, which
`XSetWindowBackground` sets and `XClearWindow` paints, so the wipe and the
final clear agree. The wipe fills the whole window and is not clipped to the
round panel (my call: the runner's corner clip already handles the push).
The canvas tracks dirty rows (`canvas_mark_dirty`), so the plan's first
check is that the fill calls used here mark them.

Every step fills rectangles or lines rather than touching pixels one at a
time, so the cost is bounded by the number of fills, and rows dirtied stay
low where the effect allows. Venetian blinds and random lines dirty all
rows by their nature. This is acceptable for one second, and the push cost
is measured (see below) rather than assumed.

### Cleanup for hacks that never free

Live erasers are listed on the display. `runner.c`'s `stop()` releases them
after the hack's free callback, beside the existing `xshim_release_pixmaps`
call, which does the same for pixmaps ("as an X server does when a client
disconnects"). A hack that did call `eraser_free` has already removed its
state from the list, so there is no double free.

This adds one line to the runner. It does not change what any hack sees, so
Rod's earlier go-ahead for the feature covers it.

## Pinned frame hashes

Any hack whose 200-frame test clears inside the window changes: Celtic's
picture finishes inside it. Each moved frame is looked at before the hash is
re-pinned, and Rod is asked before each re-pin. Per `AGENTS.md`, pin a hash
only from a build that is trusted by eye, after `srandom(1)`.

## Testing

- Host tests first (Unity, AddressSanitizer), clock injected.
- The first call draws nothing and returns non-NULL.
- Drawing between the first call and the end changes pixels, and the wipe
  ends with a fully cleared window and NULL.
- A step interval of 10 ms and one of 33 ms both end within a fixed
  tolerance of the duration. Mutation check: make progress count calls and
  watch this fail.
- Stopping mid-wipe through `runner_destroy` leaks nothing, for a hack that
  never calls `eraser_free` (compare
  `__sanitizer_get_current_allocated_bytes()` at rest, as `test_hacks.c`
  does).
- Clip arithmetic uses `int64_t`. Assert pixels, not just survival: a clip
  bug on an inner row wraps unseen by AddressSanitizer.
- Mutation-check each new test: break the rule, watch it fail, then confirm
  with `git diff` that the change applied and was restored.

## Measuring on the board

The push cost of each effect has not been measured, and cannot be from the
host. Compile, host tests and a dump-frame check come first and are reported
separately from "runs on the device". Flashing needs Rod's explicit go-ahead
each time. When he gives it: flash a Celtic-pinned build, read the `run`
lines through a wipe, and report the lowest `fps=` window as well as the
mean. If any effect drops the frame rate or pushes more rows than it should,
drop that effect rather than slow the wipe.

## Left to the builder

- The file layout inside `x11shim/`.
- Exact effect parameters (slat count, fizzle block size, line widths).
- Which existing time source to use, and how to inject it for tests.
- Whether a fifth effect is worth adding. Not required.

## Decisions made without Rod

- Four effects, about one second, random pick (from the issue).
- Time-based progress, so a slow frame cannot stretch the wipe, started on
  the second call so the dwell is not counted.
- The eraser reads `double_time()`; `dump` gets the fake clock that
  `test_hacks` already uses.
- No clipping to the round panel.
- Cleanup through the display and the runner's `stop()`, because the six
  non-freeing hacks cannot be edited.
