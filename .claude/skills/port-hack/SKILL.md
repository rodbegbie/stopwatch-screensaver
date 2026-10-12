---
name: port-hack
description: Use when adding one or several xscreensaver hacks to the StopWatch firmware, choosing which hack to port next, or when a ported hack crashes, reboots, runs slowly, leaks memory or shows wrong colours on the device.
---

# Porting an xscreensaver hack

The rules live in `AGENTS.md` ("Rules", "Gotchas") and the path-scoped notes in
`.claude/rules/`. This skill sets the order of work and the points to stop. The
numbered steps are in `adding-a-hack.md` and the recipes it names are in
`techniques.md`, both next to this file. For several hacks in one go, read
`batches.md` too.

## 1. Pick, then read the source

Take S-rated rows from `docs/porting-assessment.md` first, and prefer a low
**Speed** band: the table runs each unported S hack on the host. The effort
rating counts missing shim calls, not memory, speed or stack (Braid was S, and
would have shown as high). A high band is a hack that will probably run slowly
on the board; a low band is not a guarantee, because a hack still doing
software `double` maths runs far slower on the device than on the host. Read
the hack's source before copying it, and look for:

| The source has | Technique (seen in) |
| --- | --- |
| Arrays or a state over a few MB | Build-time patch (Maze) |
| `sin`, `cos`, `sqrt`, `pow`, `double` in a hot loop | Single precision (Galaxy, Substrate) |
| Locals over a few KB | Stack check (Rorschach) |
| An allocation of 100 KB or more, or `exit()` on failure | Large allocations (Substrate) |
| Pixmaps, clip masks, a logo image | Images (Maze) |
| A hack that restarts itself | Restart leak test (Maze, Substrate) |
| A `free` that relies on state `draw` sets up (a lazy `initted`) | Stopping before the first draw (Blaster) |
| Hairlines, fades or `glPolygonOffset` in a GL hack | Thin lines in a GL hack |
| Threads, GL, Xft, shared memory | Skipping a hack |
| A licence header | Read it; add a notices section only if it differs from jwz's |

## 2. Set up

Work in a worktree off `origin/main`. Its setup and the commands it refuses are
in "Worktree setup".

## 3. Red, copy, green

Follow `adding-a-hack.md`. Show the registry test failing before
copying. Fill each shim gap test-first, then break the code on purpose and
watch the test fail ("Tests that cannot be fooled"). Filling a missing
function is a shim gap. Changing what every hack sees (the runner, a default, a
shared helper) is not: stop and ask Rod first. A speed-up in the shared GL
layer counts too, even when every pinned frame hash still matches (HexTrail's
deferred depth clear): ask before writing it, and name it in the PR body.

## 4. Verify on the host

Run `pio test -e native`, `uv run --with pytest --with pillow pytest tools/tests`
and `uv run tools/check_notices.py`, and dump frames at several counts. Host
frames cannot show a byte swap, which happens in the push to the display: judge
colour on the device, on a colour you know the answer to.

## 5. Device

Flash only after Rod says so in this conversation; approval for one flash is
not approval for the next. See "Device capture". Report "compiles", "host tests
pass" and "runs on the device" separately; only Rod can say what the screen
shows. If it is slow, measure, change one thing, flash again and compare; if it
is still slow, say so and suggest dropping it. A hack that runs at a good rate
can still go: Rod dropped Boing (8.3 fps, "pretty boring on a small screen")
and asked for HexTrail to be tuned because it fell under 10 fps before the
rotation moved on. Judge a hack on a 466 px round screen, and by its worst
window as well as its mean. Record the numbers in
`tools/assessment_measured.md.in`, marking anything unmeasured as such.

## 6. Deliver

Commit named paths. Open the draft PR with `entire trail create` and put what
is unverified in its body. After a rebase, a push needs `--force-with-lease`:
ask Rod each time. Rod approves and merges; then delete the branch and
fast-forward `main`. New gotchas go in the matching `.claude/rules/` file (or
`AGENTS.md` if they apply everywhere) in the same PR, and deferred
ideas become issues.

## Red flags

| Thought | Reality |
| --- | --- |
| "It is rated S, so I need not read it" | Maze was rated L for Xlib gaps; the real blocker was a 20 MB `calloc`. |
| "It compiles, so it works" | Rorschach compiled and rebooted on its first frame. |
| "The colours look right" | Not from a host frame: every hack was byte swapped until Maze. |
| "My test passes, so it tests something" | A test through Pyro passed with the runner broken. Break the code on purpose. |
| "AGENTS.md says to fix shim gaps, so I can change the runner" | A new function is a gap. A change every hack sees needs Rod. |
| "Rod approved flashing earlier" | Ask again for each flash. |
| "Single precision is done, so the hack is as fast as it gets" | HexTrail gained 0.3 fps from it and 2.3 from a depth clear in the GL layer. Read what the shared code does each frame. |
| "The mean is above 10 fps" | HexTrail's mean was 15.3 and its worst window 10.8; its step grew all cycle. Read the lowest window. |
