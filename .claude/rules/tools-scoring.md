---
paths:
  - "tools/**"
  - "docs/**"
---

# Scorer and speed-prediction gotchas

Moved verbatim from the Gotchas section of AGENTS.md.

- `score_hacks.py` rates effort by call sites, not loop trips: Flame (all
  `double` maths) is rated S but runs at 2-7 fps, and Braid was rated S and
  runs at 3-9 fps. The Speed column runs the hack on the host instead and ranks
  the device step well (Spearman about 0.9 over the 29 hacks it was fitted to,
  and four more ported since fell where predicted), but a low band does not
  clear a hack that does software `double` maths: the device ran 60-1,500
  times the host time, most for the double-bound ones. It says nothing about
  memory: Celtic as copied held about 300 KB of the 325 KB of free heap and
  ran at 1.2 fps (it is registered again after issue #37, see `hacks.md`). A
  failed port that has its source and a measured row (Hypnowheel, Boing)
  still counts in `speed_backtest.py`, probed standalone. Measure on the
  device, and read
  `docs/speed-backtest.md` before trusting a band. A hack with gaps (M and
  above) cannot be built, so it has no Speed.
- A `failed_ports.txt` name is the scorer's: the source file's stem
  (`companion` for Companion Cube), as the assessment's rows spell it. Check
  the cross appears after regenerating.
- `score_hacks.py --no-probe` also blanks the Speed column of the generated
  assessment ("high (2 ms)" becomes "-"): commit the output of a full run. The
  backtest reads a measured row only when its name is the bare hack name
  (`| Deluxe |`); a row with a note in brackets is skipped, so put the build
  that is registered in the bare row.
- `score_hacks.py` counts a hack as ported if `&<name>_hack` appears anywhere
  in the text of `g_hacks[]`, `#ifdef` or not, and then refuses one that
  `failed_ports.txt` also lists. A probe-only entry (Hypnowheel's
  `HYPNOWHEEL_PROBE`) goes through a macro defined above the array.
- `speed_backtest.py` stops with "hypnowheel: does not compile ... 'rotator.h'
  file not found": a dropped GL port counts in the backtest and is probed
  standalone, without the GL helpers' include path. `docs/speed-backtest.md`
  was last regenerated before Hypnowheel was ported, and was not regenerated
  for the CWaves batch either. The scorer also lists calls inside code that is
  never compiled (`XCreateBitmapFromData` in Cynosure's `DO_STIPPLE` block) as
  gaps.
- `score_hacks.py` skips the names in `OWN_HACKS` (our own hacks with no
  xscreensaver source, now `ad_flyingtoasters`) and still reports any other
  registry name it cannot scan as a typo. `speed_backtest.py` runs on the host
  only the registered hacks that have a device-measured row, so a `dump` build
  without the After Dark disk (which lacks the toasters) still works.
