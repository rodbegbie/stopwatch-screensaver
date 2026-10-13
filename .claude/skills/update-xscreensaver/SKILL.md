---
name: update-xscreensaver
description: Use when jwz releases a new xscreensaver version, or when asked to move vendor/ to a newer release, check ported hacks against upstream changes, or score a release's new hacks.
---

# Updating the xscreensaver release

The firmware's hacks are byte-identical copies from one release, named in
`tools/xs_vendor.py`. Moving to a new one, in order (issue #91 did 6.16 to
6.17):

1. Read the release's entry in
   <https://www.jwz.org/xscreensaver/changelog.html> for new hacks and
   framework changes. Branch `chore/<issue>-xscreensaver-<x-y>`.
1. `uv run tools/fetch_xscreensaver.py` unpacks it beside the old one in
   `vendor/` (it warns that the version differs from the surveyed one: that
   is expected here). Keep the old tree until the end.
1. `uv run tools/vendor_diff.py vendor/xscreensaver-<old>
   vendor/xscreensaver-<new>` lists the copied files upstream changed or
   removed, the data the build reads from `vendor/` that changed or went, and
   the new hacks. Read each listed file's `diff -u`.
1. Set `VERSION` in `tools/xs_vendor.py`. Every script takes the path from
   it, and `test_xs_vendor.py` fails on a hard-coded `xscreensaver-N.N`.
1. Copy each changed file over ours and check it with `cmp`. Make it build
   by extending the shim, never by editing it (see `port-hack`). Upstream
   framework headers (`screenhackI.h`, `xlockmore.h`, `utils/`) are not
   copied: diff them by hand, since a new macro there (6.17's `_()` and
   `gettext`, now `x11shim/include/nls.h`) reaches the hacks through our
   stand-ins. A shim change every hack sees is Rod's call.
1. A build input that moved: fix the pre-script in `firmware/` that names
   it, and its notices section.
1. `pio test -e native` (a changed hack's frame hash may move: look at the
   frames, then re-pin as `hacks.md` says), `pio run -e stopwatch`,
   `uv run pytest tools/tests`, `uv run tools/check_notices.py`.
1. `uv run tools/score_hacks.py` (the full run, not `--no-probe`)
   regenerates `docs/porting-assessment.md` from the new tree: new hacks get
   a row, and a ported-elsewhere change can move a band (6.17's Barcode went
   M to L for `utf8wc.h`). Name new S and M hacks in the PR.
1. Replace the old version in prose: `git grep -n '<old>' -- ':!vendor'`
   (README, THIRD_PARTY_NOTICES.md, AGENTS.md, `docs/gl-budget.md`). Leave
   the version strings in `test_fetch.py` and `test_score.py`: they are
   sorting data.
1. Ask Rod before deleting the old tree from `vendor/`; other worktrees
   may still read it.
