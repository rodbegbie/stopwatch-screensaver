# Shim eraser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** `erase_window` keeps the finished picture on screen until the
hack's own wait ends, then wipes it away in about a second.

**Architecture:** A state machine in a new `x11shim/erase.c`. Progress is
time-based, read from `double_time()`, and the clock starts on the second
call so a hack's dwell is not counted. Four effects fill the window in its
background pixel. Live states are listed on the `Display` and released when
the runner stops a hack, because six hacks never call `eraser_free`.

**Tech Stack:** C, the x11shim, Unity with AddressSanitizer on the host
(`pio test -e native`).

**Spec:** `docs/superpowers/specs/2026-10-11-shim-eraser-design.md`

## Global Constraints

- Copied hacks stay byte-identical to `vendor/xscreensaver-*/hacks/`: edit
  none, and `cmp` any you touch by mistake.
- Shim code is ours: do not copy `utils/erase.c` or any xscreensaver
  `utils/` file.
- Four effects, random pick, about one second (`ERASE_SECONDS` 1.0).
- The wipe fills the whole window in `dpy->window_background` and is not
  clipped to the round panel.
- Clip and radius arithmetic uses `int64_t`: the ESP32's `long` is 32 bits.
- Tests assert pixels, not just survival; mutation-check each new test.
- Comments only where a future reader could not work it out; none otherwise.
- Commits end with
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Use named
  paths with `git add`, never `-A`.
- Never flash, erase or open the serial port without Rod's explicit go-ahead.
- Run `pio`, `uv` and `markdownlint` one at a time. In this worktree use
  `PLATFORMIO_CORE_DIR=<worktree>/.platformio ../.venv/bin/pio ...` from
  `firmware/`, with `vendor/`, `.venv/` and `.platformio/` symlinked from the
  main checkout first (the worktree gotcha in `AGENTS.md`).
- Pinned hashes move: look at each moved frame and ask Rod before re-pinning.
- Report "compiles", "host tests pass" and "runs on the device" separately.

## Review Focus

- A hack stopped mid-wipe that never calls `eraser_free` (Coral, Maze,
  Helix): no leak. Task 2.
- A caller stepping every 10 ms, every 33 ms, or after a 5 s dwell: the wipe
  ends about one second after its first step, never skipped. Task 1.
- A non-black background (Substrate's white canvas): the wipe and the final
  clear both leave the background colour. Tasks 1 and 3.
- One step arriving more than a second late: it finishes cleanly with NULL,
  no overshoot. Task 1.
- `erase_window` called again straight after it returned NULL: starts a fresh
  wipe with a new dwell. Task 1.

---

### Task 1: The state machine, the dwell and one effect

**Files:**

- Create: `firmware/src/x11shim/erase.c`, `firmware/test/test_erase/test_erase.c`
- Modify: `firmware/src/x11shim/helpers.c` (delete the two stub functions),
  `firmware/src/x11shim/include/erase.h` (update the comment)

**Interfaces:**

- Produces: `eraser_state *erase_window(Display *, Window, eraser_state *)`
  and `void eraser_free(eraser_state *)`, unchanged signatures.
- Produces (test hooks, declared in `x11shim/xshim.h` beside
  `xshim_release_pixmaps`): `void xshim_erase_force_effect(int index)` where
  `-1` means random, and `int xshim_erase_effect_count(void)`.
- Internal: a table `static const EraseEffect k_effects[]` where an effect
  draws the cumulative state for a `double ratio` in `[0, 1]`.

Model the new suite on `test/test_span_erase/test_span_erase.c`. Each test
installs a clock it controls with `xshim_set_clock`.

- [ ] **Step 1: Write the failing tests** in `test_erase.c`, on a 466×466
  canvas painted a known non-background colour, with
  `xshim_erase_force_effect(0)`:
  - `test_first_call_draws_nothing_and_returns_a_state`: pixels unchanged,
    return non-NULL.
  - `test_dwell_is_not_counted`: call once, advance the clock 5 s, call
    again: still non-NULL, and the canvas is not yet cleared.
  - `test_wipe_ends_one_second_after_its_first_step`: after the first step,
    advance 10 ms a step, then 33 ms a step, in two runs. Each run returns
    NULL at the first step at or after 1.0 s, within one step interval, and
    the canvas is then entirely the background pixel.
  - `test_a_late_step_finishes_cleanly`: first step, advance 10 s, one more
    call returns NULL and the canvas is all background.
  - `test_uses_the_window_background`: `XSetWindowBackground` to a non-zero
    pixel, run to the end: every canvas pixel equals that pixel.
  - `test_a_second_wipe_starts_fresh`: after NULL, `erase_window(…, NULL)`
    returns non-NULL and draws nothing.
  - `test_a_step_marks_rows_dirty`: after `canvas_clear_dirty`, a mid-wipe
    step leaves at least one `canvas_dirty_row` true.
- [ ] **Step 2: Run to verify they fail**

  Run: `pio test -e native -f test_erase`
  Expected: FAIL (the stub clears at once and returns NULL).
- [ ] **Step 3: Implement** the state machine in `erase.c` as the spec's
  four-state description, with `ERASE_SECONDS` 1.0 and the start time taken
  on the second call. Venetian blinds is effect 0: bars of the background
  colour whose height grows with the ratio. Add the two test hooks. Delete
  the stubs from `helpers.c`; update the `erase.h` comment to describe the
  new contract.
- [ ] **Step 4: Run to verify they pass**, same command. Expected: PASS.
- [ ] **Step 5: Mutation-check.** In a scratchpad script (see the
  loop-of-mutations gotcha in `AGENTS.md`), with `assert old in text` first:
  (a) record the start time on the first call, (b) advance progress by one
  per call instead of by the clock, (c) skip the final `XClearWindow`. Each
  must fail a named test above. Restore and confirm with `git diff`.
- [ ] **Step 6: Commit**
  `git add firmware/src/x11shim/erase.c firmware/src/x11shim/helpers.c
  firmware/src/x11shim/include/erase.h firmware/src/x11shim/xshim.h
  firmware/test/test_erase/test_erase.c`

### Task 2: Release live erasers when a hack stops

**Files:**

- Modify: `firmware/src/x11shim/xshim.h` (a list head on `XshimDisplay`,
  `void xshim_release_erasers(Display *)`), `firmware/src/x11shim/erase.c`,
  `firmware/src/x11shim/xshim.c` (`xshim_close_display`),
  `firmware/src/runner/hack_runner.c` (`stop()`)
- Test: `firmware/test/test_erase/test_erase.c`

**Interfaces:**

- Consumes: Task 1's `erase_window` and `eraser_free`.
- Produces: `void xshim_release_erasers(Display *dpy)`, which frees every
  live state on the display. `eraser_free` removes its state from the list,
  so a hack that does free causes no double free.

- [ ] **Step 1: Write the failing tests:**
  - `test_stopping_mid_wipe_frees_a_state_the_hack_never_frees`: a stub
    `HackEntry` (via `runner_create_with`) whose draw calls `erase_window`
    once and whose free does not call `eraser_free`. Compare
    `__sanitizer_get_current_allocated_bytes()` at rest (after
    `runner_destroy`) before and after 50 start/stop cycles, as
    `test_hacks.c` does. Expected equal.
  - `test_eraser_free_then_release_does_not_double_free`: create, free,
    call `xshim_release_erasers`; AddressSanitizer stays quiet.
- [ ] **Step 2: Run to verify they fail**: `pio test -e native -f test_erase`
  (the first leaks).
- [ ] **Step 3: Implement** the list, `xshim_release_erasers`, and call it
  beside `xshim_release_pixmaps` in `stop()` and in `xshim_close_display`.
- [ ] **Step 4: Run to verify they pass**, then `pio test -e native -f
  test_runner_xsft` and `-f test_xshim` for the runner and display changes.
- [ ] **Step 5: Mutation-check:** drop the call in `stop()`; the leak test
  must fail. Restore and confirm with `git diff`.
- [ ] **Step 6: Commit** the five named files.

### Task 3: Circle, fizzle and random-lines effects

**Files:**

- Modify: `firmware/src/x11shim/erase.c`
- Test: `firmware/test/test_erase/test_erase.c`

**Interfaces:**

- Consumes: the effect table and `xshim_erase_force_effect` from Task 1.
- Produces: effects 1 (circle wipe, centred, radius squared in `int64_t`),
  2 (fizzle, random blocks until all are cleared), 3 (random lines in the
  background colour, thickening); `xshim_erase_effect_count()` returns 4.

- [ ] **Step 1: Write the failing tests**, run for each effect index in a
  loop:
  - `test_effect_count_is_four`.
  - `test_cleared_area_grows_and_ends_complete`: at ratios 0.25, 0.5 and
    0.75 the count of background pixels is strictly greater than at the
    previous sample and less than the whole canvas; at the end it is all.
  - `test_steps_stay_inside_the_canvas`: every `canvas_dirty_row` x-extent
    after a step lies within `[0, 465]`, and a step at ratio 0.5 takes under
    5 ms of host time (a bound that catches a per-pixel loop, not a
    benchmark).
  - `test_effects_use_the_window_background` with a non-zero background.
- [ ] **Step 2: Run to verify they fail**, then **Step 3: implement** the
  three effects. Fizzle seeds its order from `random()` once per wipe so
  repeated steps do not re-fill; keep each step to rectangle or line fills.
- [ ] **Step 4: Run to verify they pass**.
- [ ] **Step 5: Mutation-check:** shrink the circle radius by a factor of
  two; the area test must fail. Restore and confirm with `git diff`.
- [ ] **Step 6: Commit** `erase.c` and `test_erase.c`.

### Task 4: Hashes, the dump clock and the docs

**Files:**

- Modify: `firmware/native/dump_main.c` (install the 1/30 s fake clock),
  `firmware/test/test_hacks/test_hacks.c` (re-pinned hashes, with Rod's
  approval), `.claude/rules/x11shim.md` line 29 and any `#86` mention in
  `.claude/rules/hacks.md` and `AGENTS.md`

**Interfaces:**

- Consumes: Tasks 1 to 3.

- [ ] **Step 1: Install the clock in `dump_main.c`**: `xshim_set_clock` with
  a function that adds 1/30 s a reading, set before `runner_start`.
- [ ] **Step 2: Find what moved.** Run `pio test -e native -f test_hacks`
  with `NO_COLOR=1` (about 2m20s; check `sample` if it looks hung). List the
  failing hash assertions.
- [ ] **Step 3: Look at every moved hash.** For each, `pio run -e dump`,
  dump the hack at 200 frames from this build and from `main` (`git archive`
  export in the scratchpad), convert with
  `uv run tools/rgb565_to_png.py`, and compare side by side. Expected: only
  hacks that erase inside 200 frames move (Celtic). Anything else is a bug.
- [ ] **Step 4: Ask Rod** before re-pinning, showing the frames. Then
  re-pin from a build he has accepted, after `srandom(1)`.
- [ ] **Step 5: Update the docs**: rewrite the `erase_window` bullet in
  `x11shim.md` to say what it does now and name the four effects.
- [ ] **Step 6: Commit** the named files.

### Task 5: Verify and measure

- [ ] **Step 1:** `pio test -e native` (about 5 minutes, one run), then
  `uv run tools/check_notices.py`, then `markdownlint` on the spec, the plan
  and `.claude/rules/x11shim.md`. All pass.
- [ ] **Step 2:** `pio run -e stopwatch`. Report it as "compiles" only.
- [ ] **Step 3: Ask Rod** for a go-ahead to flash. If given, check
  `lsof /dev/cu.usbmodem112401`, flash a build pinned to Celtic with
  `-DROTATE_SECONDS=0`, capture the log through one wipe, and report the
  mean and lowest `fps=` window and the `rows=` during the wipe. If an
  effect drops the frame rate, drop that effect rather than slow the wipe.
  Ask Rod to look at the screen and say whether the dwell, the wipe and
  its speed look right.
- [ ] **Step 4:** remove `docs/superpowers/` from the branch tip before
  merge, as with #77, and follow the `deliver-branch` skill.
