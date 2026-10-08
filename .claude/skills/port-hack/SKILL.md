---
name: port-hack
description: Use when adding an xscreensaver hack to the StopWatch firmware, choosing which hack to port next, or when a ported hack crashes, reboots, runs slowly, leaks memory or shows wrong colours on the device.
---

# Porting an xscreensaver hack

The rules live in `AGENTS.md` ("Rules", "Adding a hack", "Gotchas") and are not
repeated here. This skill sets the order of work and the points to stop.

## 1. Pick, then read the source

Take S-rated rows from `docs/porting-assessment.md` first. The rating counts
missing shim calls, not memory, speed or stack, so read the hack's source
before copying it:

| The source has | Do | Seen in |
| --- | --- | --- |
| A state struct or arrays over a few MB (check `sizeof`, `[1000][1000]`) | Patch a copy at build time; `hacks/` stays byte-identical | Maze |
| Many `double`s, `sqrt`, `sin`/`cos`, `pow` | `hacks/<name>_single.c` wrapper; compare double and float frames | Galaxy |
| Locals over a few KB | `-fstack-usage` check (AGENTS.md); loop stack is 16 KB | Rorschach |
| `malloc`, `calloc` or `realloc` of 100 KB or more, or `exit()` on allocation failure | Find where the allocator puts it (PSRAM or internal heap), then read heap and PSRAM in the serial log | Not yet confirmed on the device |
| Pixmaps, clip masks or a logo image | Shim has them; pictures are raw blobs, never a PNG decoder | Maze |
| Threads, GL, Xft, shared memory | Skip it | L and XL rows |
| A licence header | Read it. Add its own section to `THIRD_PARTY_NOTICES.md` only if the wording differs from jwz's | Maze, Pedal |

## 2. Set up

A worktree off `origin/main`. A fresh worktree lacks the ignored `vendor/`,
`.venv/` and `.platformio/`: symlink them from the main checkout. The ignore
patterns end in `/`, so the symlinks show as untracked: `git add` named paths,
never `-A`. Adding them to `.git/info/exclude` changes state shared with every
worktree, so ask Rod first.

A worktree session refuses `source`, `$VAR` in a command and `&&` chains with a
computed path. Run plain commands with literal paths, for example
`PLATFORMIO_CORE_DIR=<worktree>/.platformio ../.venv/bin/pio test -e native`.

## 3. Red, copy, green

Follow "Adding a hack" in AGENTS.md. Show the registry test failing before
copying. Fill each shim gap test-first, then break the code on purpose and
watch the test fail. Leak-test any hack that restarts itself, and prove the
test sees restarts with a deliberate per-restart leak.

## 4. Verify on the host

Run `pio test -e native`, `uv run --with pytest --with pillow pytest tools/tests`
and `uv run tools/check_notices.py`. Dump frames at several counts and look at
them. A hue-sweeping palette stays a rainbow when its bytes are swapped, so
judge colour on a fixed colour or image you know the answer to.

## 5. Device

Flash only after Rod says so in this conversation; approval for one flash is
not approval for the next. Pin the hack with `START_HACK`, send upload output
to a file and look for "Hash of data verified". Capture 150 s or more of
serial in the background, then read `step`, `push`, `wait`, heap and PSRAM, and
any "Stack canary". Report "compiles", "host tests pass" and "runs on the
device" separately; only Rod can say what the screen shows. If it is slow,
measure, try one change, flash again and compare; if it is still slow, say
so and suggest dropping it. Write what you measured into
`tools/assessment_measured.md`, and mark anything you could not measure as such.

## 6. Deliver

Commit named paths, with the trailer. Push, retrying once on a GitHub
rejection and never forcing. Open the draft PR with `entire trail create`, put
what is unverified in its body, then handle findings with `entire trail
finding`. Rod approves and merges. Afterwards delete the branch and
fast-forward `main`. Put new gotchas in `AGENTS.md` in the same PR, and file
deferred ideas as issues.

## Red flags

| Thought | Reality |
| --- | --- |
| "It is rated S, so I need not read it" | Maze was rated L for Xlib gaps; the real blocker was a 20 MB `calloc`. |
| "It compiles, so it works" | Rorschach compiled and rebooted on its first frame. |
| "The colours look right" | Only for colours you know. Every hack was byte swapped until Maze. |
| "Rod approved flashing earlier" | Ask again for each flash. |
