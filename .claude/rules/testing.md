---
paths:
  - "firmware/test/**"
  - "tools/tests/**"
---

# Testing gotchas

Moved verbatim from AGENTS.md.

- A mutant that sends every case to the fallback path is equivalent, not a
  gap: it passes because the old arithmetic runs. Change the rounding without
  forcing the fallback. A guard justified by an error bound (a limit, a
  reseed interval) can survive a mutant when failures are one in millions:
  say so in the PR rather than hunting for a test.
- Host tests cannot prove the 16 KB loop stack: `ulimit -s` sees only the
  shallow first level of a random recursion, and AddressSanitizer inflates
  frames. Use `stack_used_by` in `test_hacks.c` (a painted pthread stack) and,
  on the device, `uxTaskGetStackHighWaterMark` in the stats line (temporary).
- `pio test` runs every registered hack for 3000 frames under ASan (about 15 s);
  a hack that is slow on the host slows the whole suite.
- Host leak tests: LeakSanitizer does not run on macOS, so compare
  `__sanitizer_get_current_allocated_bytes()`
  (`<sanitizer/allocator_interface.h>`) before and after many restarts, as in
  `test_hacks.c`.
  Take both readings at rest (after `runner_destroy`): a reading while a hack
  runs counts random-sized live state (Loop swings about 1 MB by mode), so
  the verdict depends on test order.
- AddressSanitizer cannot see an index past an array inside a struct
  (`pp->Cells[x][25][z]`). Build with `-fsanitize=address,array-bounds
  -fno-sanitize-recover=all` to check such a claim, and prove first that it
  fires on a small probe.
- `pio test` hides `printf` and stderr from tests. To see two values, assert
  `TEST_ASSERT_EQUAL_UINT64(a, b)` temporarily: the failure line prints both.
- To survey many hacks on the host (flushes per frame, say), add a temporary
  test that `fopen`s `getenv("SURVEY_OUT")` and writes lines; run
  `SURVEY_OUT=<file> pio test -e native -f test_push_present`, then
  `git checkout` the test. `PLATFORMIO_BUILD_FLAGS=-D...` varies a constant.
- `test_frames_of_every_hack_but_maze_match_main` stops at the first moved
  hash, so the others hide behind it. To list all of them, temporarily append
  `name idx got=0x...` to a buffer and `TEST_FAIL_MESSAGE` it (`TEST_MESSAGE`
  is hidden too), then `git checkout` the file. The failure prints decimal;
  `python3 -c "print(hex(N))"` gives the `kBaseline` form.
- A hack allocates its state on the first `runner_step`, not at
  `runner_start`: a test that reads a hack's globals (`pacman_games[0]`)
  before one step gets NULL, and ASan reports a SEGV on a write near address 0.
- A mutation that keeps the file's size (`if (0)` to `if (1)`), restored in
  the same second, leaves a stale `.pyc` that the next `pytest` run loads:
  delete `tools/__pycache__/<module>*.pyc` after a mutation script, then
  rerun before trusting a failure.
- On a busy Mac (load average over 10) the suite can look hung. Before
  killing it, `sample $(pgrep -f .pio/build/native/program) 3` shows where it
  is: it was in Unknown Pleasures, slow but moving.
- Mutation-checking wrap-around code: a mutation that just reorders unsigned
  arithmetic can be equivalent and survive. Use `int64_t` to break it for real.
- A guard condition already implied elsewhere (`color_valid &&` where an
  invalid side is fully cleared anyway) leaves an equivalent mutant that
  survives: drop it, so every part of the guard can be killed.
- After a mutation check, `grep` that the change applied: a `sed` that matches
  nothing leaves the tests green and proves nothing. Restore the file and
  confirm with `cmp` or `git diff`.
- Restoring after a mutation: commit (or copy) the real change first.
  `git checkout <file>` reverts everything uncommitted in it, fix included.
  The scratchpad mutation script restores from its in-memory original.
- Mutating a regex or any pattern with backslashes: write the mutation as a
  scratchpad script (as the loop-of-mutations gotcha says) and `assert old in
  text` first. Shell quoting doubles the backslashes, nothing changes, and the
  passing run looks like a surviving mutant.
- Unity marks a failing test `[FAILED]` at the end of its line, not `:FAIL`. A
  mutation script that greps for `:FAIL` reports every mutant as surviving.
- A mutant can die by crash, not assertion: an ASan abort ends the run with
  "20 test cases: 19 succeeded" (fewer cases than usual, no `[FAILED]`). Read
  the summary line and treat anything but the full passing count as killed.
- To test code inside a wrapped hack (a `static inline` blend, say), give the
  test its own dir and `#define <name>_hack <name>_hack_under_test` before
  `#include "hacks/<name>_direct.c"`: the wrapper exports only that one symbol,
  so it links beside the registered copy. See `test_interaggregate_blend`.
- `pio test -e native -f test_<dir>` runs one suite (the xshim and
  single-hack tests take about a second); `test_hacks` alone takes about
  2m20s and the whole `pio test -e native` about 5 minutes.
- `pio test` hides which test crashed and AddressSanitizer's report: after a
  failure, run `.pio/build/native/program` from `firmware/` for the per-test
  PASS/FAIL lines and the ASan stack.
- A mutation script must run the unmutated tree first and stop if it fails:
  a test that fails for another reason (Phosphor's included the paged poem
  header while the wrapper used the scrolling one) makes every mutant look
  killed. Print the baseline's result, not only the mutants'.
- `test_push_overlap` runs each flagged hack on a fake clock (`0.02` s a
  frame). XAnalogTV's warm-up squash follows the clock, and on the real clock
  the plain and noisy runs squashed at different frames, so the "ignores the
  canvas it started from" check failed on two rows (a diagnostic that
  printed the differing pixels showed the band; the same frames matched once
  the clock was fixed, with no change to the hack).

## CI on Depot (Linux)

- The suite is written against macOS. On Depot's Linux runners it needed: an
  Arm runner (`depot-ubuntu-24.04-arm`; x86_64 gcc fails in the SSE headers
  that Fireworkx pulls in), `gcc` and `g++` symlinked to clang (the tests
  include clang's `sanitizer/allocator_interface.h`), `-lm` in
  `PLATFORMIO_BUILD_FLAGS` (macOS links libm implicitly), and `<time.h>` in the
  shim's `screenhack.h` (macOS's `<sys/time.h>` declares `time()`, glibc's does
  not). `tools-tests` aliases `cc` to clang because `score_hacks.py` passes
  the clang-only `-ferror-limit=0`.
- "Program received signal SIGHUP (Hangup)" from `pio test` is not a hangup:
  PlatformIO prints any non-zero exit code as that signal number, and 1 is
  SIGHUP. On Linux it was LeakSanitizer exiting 1 at the end of a suite (it
  does not run on macOS), reporting leaks in unmodified hacks such as
  `gears.c`. CI sets `ASAN_OPTIONS=detect_leaks=0`. Run the binary directly
  (`.pio/build/native/program`) to see the real report.
- `test_frames_of_every_hack_but_maze_match_main` is skipped when
  `XS_SKIP_PINNED_FRAMES` is set, which CI does: the hashes were taken on macOS
  and `ya_random` is the C library's `random()`, which differs on glibc. A
  local run still checks them. Do not re-pin them from a Linux run.
- The stack probe measures an idle thread first and subtracts it, because
  the painted stack also holds the thread's own data, more of it on Linux
  (Pacman read 18 KB there against 12 KB on macOS).
- Shards: `test_hacks` and `test_push_present` take about 12 and 13 minutes
  each (they run every hack under ASan) and the other 32 suites about 4, so
  `firmware-tests` is a matrix of three jobs: `-f test_hacks`,
  `-f test_push_present` and `-i test_hacks -i test_push_present`. A new suite
  lands in the last one. Keep each shard's JUnit file and report `key`
  distinct, or Depot drops the later uploads as duplicates. Separate jobs are
  needed: every suite builds to the same `.pio/build/native/program`, so two
  `pio test` processes in one checkout overwrite each other's binary.
- `pio test --junit-output-path` does not create the directory.
- Running it: `depot ci run` uploads your uncommitted changes as a patch, so
  run it from a clean `git worktree` of the pushed commit to leave unrelated
  edits out. `depot ci status <run> --wait` in the background, then
  `depot ci diagnose --run <run>` and `depot ci logs <attempt>`. Logs from a
  step's stdout and stderr interleave, so a warning's position says nothing
  about which suite printed it.
