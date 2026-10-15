---
name: worktree-session
description: Use when working in a git worktree of this repo (EnterWorktree, an agent with isolation worktree, .claude/worktrees/): missing vendor/.venv/.platformio symlinks, commands the harness refuses, pio and uv paths, ExitWorktree and removing a worktree.
---

# Working in a git worktree

Moved verbatim from AGENTS.md.

- In a git worktree the ignored `vendor/`, `.venv/` and `.platformio/` are
  missing. Symlink them from the main checkout. `.gitignore`'s trailing slashes
  don't match symlinks, so they would show as untracked, but the shared
  `.git/info/exclude` already lists the three names. That file is shared by
  every worktree: ask Rod before adding to it.
  The harness refuses `source tools/env.sh` and a computed `PATH` there: run
  `PLATFORMIO_CORE_DIR=<worktree>/.platformio ../.venv/bin/pio ...` instead,
  and keep commands plain (no scripts, loops, `$VAR` in a command or `&&`
  chains with a computed path). A worktree an agent works in (`isolation:
  "worktree"`) starts from `origin/main`, not from your branch, and has no
  vendor or toolchain until you link them.
  It also refuses heredocs, `git -C` and `cd` into another worktree: use the
  Edit and Write tools. It refuses `git ... | tar` and `cd <dir> && git ...`
  too: use `git archive -o f.tar <sha>` then `tar -x -f f.tar -C <dir>`, and run
  git from the worktree root. A background shell starts there as well, so use
  `pio test -d <abs>/firmware` with absolute paths. Parallel calls can race
  (a `uv run` in the root disturbed `.venv` while `pio` ran:
  "../.venv/bin/pio: no such file"), so run
  `uv` and `pio` one at a time. To fast-forward `main` or remove the worktree,
  leave it first with `ExitWorktree keep`. If no checkout has `main` out,
  `git fetch origin main:main` fast-forwards it from anywhere.
  Before `ExitWorktree remove` on a merged branch, check
  `git merge-base --is-ancestor <tip> origin/main`; the tool's "discarded N
  commits" count ignores merges. `.platformio`, `.venv` and `vendor` there are
  symlinks, so removal leaves the real ones alone. After a merge,
  `ExitWorktree keep` names a branch (`worktree-<name>`) that may not exist:
  remove the worktree with `git worktree remove --force <path>` and delete the
  real branch, as separate commands, because a failed `&&` link skips the rest
  silently. Check `git worktree list` afterwards. In a session resumed from a
  summary, `ExitWorktree` is a no-op ("no active EnterWorktree session"):
  remove the worktree with `git worktree remove --force <path>` from the main
  checkout instead. To delete the merged branch
  you are standing on, `git switch --detach origin/main` in the worktree first.
  The shell's directory resets after each command and after `ExitWorktree`, so
  use absolute paths (`../.venv/bin/pio` failed from the wrong place and the run
  looked like a test failure). A loop over several mutations is refused as a
  command: write it as a script in the scratchpad that edits the file, runs
  `pio test` through `subprocess` and restores the file in `finally`, then run
  it with `python3 -I`.
  Stopping such a script (TaskStop) kills it before `finally` runs: check
  `git diff` afterwards and restore the file by hand.
  Also refused: a heredoc (use the Write tool for a scratchpad script), `$(...)`
  inside a git command, and the quoted `PLATFORMIO_BUILD_FLAGS` above combined
  with a redirect or `NO_COLOR=1`. Flashing worked as
  `PLATFORMIO_CORE_DIR=<wt>/.platformio PLATFORMIO_BUILD_FLAGS="..."
  .venv/bin/pio run -d firmware -e stopwatch -t upload` with no redirect: the
  540 KB of output lands in a tool-results file, so grep that for "Hash of data
  verified". Edit and Write refuse the main checkout's path while you are in a
  worktree. `pio test -f test_x` runs the whole suite (`test_hacks` takes 3
  minutes, the full native run 7), and a failing summary reads "2 failed, 101
  succeeded": match `[FAILED]` or the return code, not "succeeded".
- `uv run ty check` (the pre-commit `ty` hook) fails in a worktree with
  "Failed to spawn: ty": the shared `.venv` has no `ty`. Run
  `uvx ty check --python .venv` instead (it passes on a clean tree), then
  commit with `SKIP=ty git commit ...`, and say in the PR that the hook was
  skipped. `ruff-format` rewrites a file and aborts the commit when a line is
  too long: restage the file and commit again.
- Also refused in a worktree session: a shell variable used as a program's
  argument (`S=...; prog $S/x`), a `for` loop that runs `uv` or `pio`, and
  `entire trail finding resolve -m "..."` text containing `*` or backticks.
  Spell out absolute paths, run one command per call, and keep `-m` text to
  plain words.
