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
