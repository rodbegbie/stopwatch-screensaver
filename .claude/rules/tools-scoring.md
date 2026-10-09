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
  memory: Celtic held about 300 KB of the 325 KB of free heap and its
  `assert()` aborts on a failed allocation, and it ran at 1.2 fps, so it is
  shelved (issue #37): its source stays in `firmware/src/hacks/celtic/`, it is
  not in `g_hacks[]`, and `failed_ports.txt` lists it. A failed port that has
  its source and a measured row still counts in `speed_backtest.py`, probed
  standalone. Measure on the device, and read
  `docs/speed-backtest.md` before trusting a band. A hack with gaps (M and
  above) cannot be built, so it has no Speed.
