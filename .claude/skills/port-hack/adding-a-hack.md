# Adding a hack

Moved verbatim from AGENTS.md. The order of work is in `SKILL.md`.

The `port-hack` skill orders this work, says what to read for before copying a
hack, and holds a recipe per trap. The steps:

1. Check its row in `docs/porting-assessment.md` (effort and Speed band) and
   read its licence header.
2. Update the expected list in `firmware/test/test_hacks/test_hacks.c` and see
   it fail.
3. Copy the file to `firmware/src/hacks/<name>/<name>.c`, add the notices row
   and an `extern` plus entry in `hacks/registry.c`.
4. Fill shim gaps test-first in `firmware/test/test_xshim/`. The compile
   check in `score_hacks.py` shows what is missing.
5. Host tests pass, dump a frame and look at it, then (with Rod's go-ahead)
   flash and measure fps and free heap/PSRAM over serial.
6. Regenerate the assessment and add measurements to
   `tools/assessment_measured.md`, then re-run the speed backtest: each port
   adds a point to the check of the Speed bands, which were fitted to the first
   29 hacks. To make a port a real test, commit its host time and predicted
   range first (`docs/speed-predictions.md` shows the form), flash it as copied,
   and add its name to `HOLDOUT` in `tools/speed_backtest.py`. The first four
   (Mountain, Epicycle, Kaleidescope, Celtic) all fell where predicted.
7. If a hack is slow and `double`-heavy (many `double`s, `sqrt`, `sin`/`cos`,
   `pow`), try a single-precision wrapper like `hacks/galaxy_single.c` (for a
   plain screenhack, `hacks/substrate_single.c`; the recipe is in the skill's
   `techniques.md`), and compare double and float frames at several frame
   counts. A resource
   override can skip a hack's restart cleanup: Galaxy leaked at `count: 2`, so
   leak-test any override.
8. If Rod drops a port after seeing it (Celtic): remove it from `g_hacks[]`, the
   expected list, its hash and its leak test; keep the source and notices row;
   add `name: reason` (short: it is printed as a bullet) to
   `tools/failed_ports.txt`; file an issue.
