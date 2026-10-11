# Porting a batch of hacks

For several hacks on one branch, with Rod away. The per-hack steps are in
`SKILL.md` and `adding-a-hack.md`; this is what a batch adds. Per-hack findings
live in `.claude/rules/hacks.md`, not here.

## Order of work

1. Read every header before copying anything: a `(GPL)` tag or an xlock notice
   changes the notices row, and a licence that is not a permission notice means
   stop and ask.
2. Triage by gap count. Do the easy ones first: they pay for shim gaps the
   others need. Leave GL and anything under a few fps to last.
3. One branch, a commit per hack, shim code test-first and mutation-checked.
   No PR until Rod says.
4. `uv run tools/wire_hack.py <name> <Class> "<copyright>"` copies the file and
   registers it with a frame hash of `0` (skipped), so every commit stays green.
   Then `pio run -e dump`, fill the gaps, dump frames at several counts and
   look at them.
5. Flash and measure each hack as soon as it runs
   (`bash tools/flash_and_capture.sh <hack> 30`). Report measured, unjudged and
   unseen separately.
6. Pin all the new hashes in one run, after looking at the frames:
   `HACK_PINS_OUT=<file> pio test -e native -f test_hacks`.
7. Soak the whole registry before handing over: `-DROTATE_SECONDS=5` for five
   laps, then look for a reboot, a panic or a "Stack canary", and compare PSRAM
   on each visit of the new hacks (it should match exactly).
8. Measured rows, rules, a regenerated assessment, and a notes file for Rod: what
   to look at, which choices are his, what was not measured. His review changes
   the numbers (a hack dropped, one retuned), so rewrite the notes after it or
   put the final state in the PR body, which is the record. A stale notes file
   misleads.

## Traps

| Trap | Do |
| --- | --- |
| A change that is not pixel-identical by construction (single precision, a faster fill) and you only checked fps | Look at a frame. A hack drew garbage for hours at "20 fps" |
| `#define double float` before a header that has `double` in a struct a shim `.c` shares | Include that header first, then the define; a flat-screen test guards it |
| A hack that keeps state in statics across starts | Its pin depends on earlier tests; reset the static in a wrapper, and test both outcomes |
| A mode picked with `random()` | A boot is deterministic, so measure the other mode by forcing it |
| Constants in pixels or frames, tuned for another window size or frame rate | Scale them at build time (`tools/fireworkx_patch.py`), not by editing the copy |
| A hack that treats `malloc(0) == NULL` as out of memory | The board returns NULL for zero bytes, the host a pointer: it stops drawing on the board only. Reproduce with an allocator hook that fails for zero |
| A stop that leaks | `MallocStackLogging=1 leaks --atExit -- .pio/build/dump/program <index> 0 out.raw` names the line (rebuild `-e dump` first) |

## Finding the cost

Skip one thing behind a temporary flag and read `step=`; revert it. Guessing
was wrong three times running. The usual causes here:

- full-frame passes over PSRAM: a clear, a copy, a blur, a 565 conversion (render
  smaller, alias a pixmap to the window, erase only what was drawn)
- the shim's polygon fill, which divides in double (`hacks/float_polygon.h`)
- soft double: `xtensa-esp32s3-elf-nm <object> | grep ' U __.*df'`; `frand` hides
  a `1.0` in its macro body (`hacks/single_frand.h`)
- a pause the hack asks for (`wait=`): override `*delay` and say the pace changed
- a GL hack's clear and push, which scale with the dirty box, and work the GL
  layer does that nothing reads (a depth clear with the depth test off):
  `gl_budget.py` shows vertices submitted against triangles drawn

## Choices for Rod

When look and speed trade off, leave a build flag per option, measure each, and
register the one nearer his bar. After his hands-on notes, expect to retune or
drop: from seven ports he kept six and dropped Boing, and asked for HexTrail to
be faster. Keep a flashable rotation build ready (`-DROTATE_SECONDS=25`, the new
hacks first) and say which build the board holds when you stop.

## Tools

| Need | Use |
| --- | --- |
| Register a hack | `tools/wire_hack.py` |
| Flash a pinned build and read the serial log | `tools/flash_and_capture.sh`, `tools/serial_capture.py` |
| Does a test notice a break | `tools/mutate.py <file> 'old=>new' --cmd '...'` |
| Pin hashes | `HACK_PINS_OUT` (above) |
