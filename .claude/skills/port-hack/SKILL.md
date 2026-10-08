---
name: port-hack
description: Use when adding an xscreensaver hack to the StopWatch firmware, choosing which hack to port next, or when a ported hack crashes, reboots, runs slowly, leaks memory or shows wrong colours on the device.
---

# Porting an xscreensaver hack

The rules live in `AGENTS.md` ("Rules", "Adding a hack", "Gotchas"). This skill
sets the order of work and the points to stop. The recipes it names are in
`techniques.md`, next to this file.

## 1. Pick, then read the source

Take S-rated rows from `docs/porting-assessment.md` first. The rating counts
missing shim calls, not memory, speed or stack, so read the hack's source
before copying it, and look for:

| The source has | Technique (seen in) |
| --- | --- |
| Arrays or a state over a few MB | Build-time patch (Maze) |
| `sin`, `cos`, `sqrt`, `pow`, `double` in a hot loop | Single precision (Galaxy, Substrate) |
| Locals over a few KB | Stack check (Rorschach) |
| An allocation of 100 KB or more, or `exit()` on failure | Large allocations (Substrate) |
| Pixmaps, clip masks, a logo image | Images (Maze) |
| A hack that restarts itself | Restart leak test (Maze, Substrate) |
| Threads, GL, Xft, shared memory | Skipping a hack |
| A licence header | Read it; add a notices section only if it differs from jwz's |

## 2. Set up

Work in a worktree off `origin/main`. Its setup and the commands it refuses are
in "Worktree setup".

## 3. Red, copy, green

Follow "Adding a hack" in AGENTS.md. Show the registry test failing before
copying. Fill each shim gap test-first, then break the code on purpose and
watch the test fail ("Tests that cannot be fooled"). Filling a missing
function is a shim gap. Changing what every hack sees (the runner, a default, a
shared helper) is not: stop and ask Rod first.

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
is still slow, say so and suggest dropping it. Record the numbers in
`tools/assessment_measured.md`, marking anything unmeasured as such.

## 6. Deliver

Commit named paths. Open the draft PR with `entire trail create` and put what
is unverified in its body. After a rebase, a push needs `--force-with-lease`:
ask Rod each time. Rod approves and merges; then delete the branch and
fast-forward `main`. New gotchas go in `AGENTS.md` in the same PR, and deferred
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
