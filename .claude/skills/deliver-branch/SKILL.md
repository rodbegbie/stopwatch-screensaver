---
name: deliver-branch
description: Use when opening, updating, rebasing, merging or cleaning up a branch or PR in this repo: entire trail create, PR body sync, findings, stacked PRs, rebase conflicts, deleting a merged branch.
---

# Delivering a branch

Moved verbatim from AGENTS.md.

- `entire trail create --title ... --type feature --body ...` pushes the branch
  (`--type` takes only `bug`, `feature` or `task`; `chore` is refused)
  and opens a linked DRAFT PR. The PR body is not synced from the trail: update
  both (`entire trail update --body`, `gh pr edit N --body-file`). Put
  `Fixes #N` in the body, then read the PR back (`gh pr view N --json body`):
  #27 merged with the generic body, so `Fixes #23` never reached GitHub and
  the issue stayed open until it was closed by hand.
- Rod approves and merges (merge commit). Afterwards delete the merged branch
  (remote and local) without asking and fast-forward `main`. Confirm the merge
  after a fresh `git fetch`: a stale `origin/main` says "not merged". Remove
  the branch's worktree first (a checked-out branch cannot be deleted), and
  note that `git branch -d` refuses while local `main` is behind, so check
  `git merge-base --is-ancestor <tip> origin/main` and then use `-D`. `main` is
  usually checked out in Rod's main checkout, so `git fetch origin main:main`
  is refused there: run `git merge --ff-only origin/main` in it, only if
  `git status` shows no tracked changes.
- Merge with `gh pr merge N --merge` once Rod approves. His approval is
  enough: do not wait for the Entire Gates check to finish. A stacked PR's gate
  can fail "Up to date: 1 commit behind" once its parent merges: if approvals
  and findings pass and the PR is mergeable, merge it, with no rebase.
- `entire agent-help` says trails are unavailable; `entire trail create` works.
  Write a PR body to a file for `gh pr edit --body-file` (an apostrophe in an
  inline `--body '...'` must be `'"'"'`).
- `gh pr edit N --body-file` replaces the whole body, including the
  `<!-- entire-trail-link-start -->` ... `-end -->` block at the top. Keep that
  block in the file, or the PR loses its trail.
- Findings: `entire trail finding list N`, then `... resolve N <id> -m "..."`.
  Bot reviews can lag about 20 minutes. `N` is the trail number (PR #12 was
  trail 6), not the PR number. For a false positive or upstream behaviour in a
  byte-identical hack, use `entire trail finding dismiss N <id> -m "<reason>"`.
- In a worktree session create the trail with a one-line `--body`, then set the
  real body with `gh pr edit N --body-file` (keep the trail-link block), and
  check `closingIssuesReferences` with `gh pr view N --json` (it can lag a
  few seconds).
- `entire trail update --body` takes no number and acts on the current
  branch (`entire trail update 8` errors).
- Stacked PR: `entire trail create --base <parent-branch>`. When the parent
  merges, `gh pr edit N --base main` before deleting its branch, then
  `git rebase --onto origin/main <old parent tip>`. The rebased push needs
  `--force-with-lease` (ask Rod first), unless the branch was never pushed.
- `docs/porting-assessment.md` conflicts on rebase: `git checkout --theirs`
  it to continue, then rerun `tools/score_hacks.py` and amend the result in.
- Two ports in flight both append to `g_hacks[]`, so whichever merges second
  conflicts in `registry.c`, the expected list in `test_hacks.c`,
  `THIRD_PARTY_NOTICES.md`, `tools/assessment_measured.md` (rows, paragraphs and
  the hack count) and the three `build_src_filter` lines in `platformio.ini`,
  as well as the generated assessment. Keep both sides, put the newer hack
  last, and regenerate the assessment. A rebased published branch needs
  `--force-with-lease` (ask Rod), and GitHub can reject that push once too.
- A finding that says the code "does not exist" can be stale after a rebase
  (the skill PR was reviewed before the Blaster and Substrate code it referred
  to had landed). Check it against `main` before dismissing, and put the
  evidence in the dismissal. Entire's approvals gate once showed "no reviewers
  have approved" right after a force push, though Rod had approved; his word is
  enough, so say so and go ahead.
