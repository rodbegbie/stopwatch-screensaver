# Speed predictions for four ports

A test of the Speed bands in `docs/speed-backtest.md`, which were fitted to the
29 hacks that had already been ported. Four more are ported here, two from the
low band and two from the high band, and what the device does is compared with
what the host probe predicted. The predictions below were written and
committed before any of the four was ported, flashed or measured.

## Method

- Host time is `tools/probe_hacks.py`'s median of three runs of the unmodified
  hack, taken on `main` at `7c90bf1`.
- The device took between 61 and 1,465 times the host time across the 29
  hacks (the highest for hacks that still do software `double` maths), so each
  prediction is the host time times 60 to times 1,500.
- The step is what the serial log prints as `step=`, in the second 5-second
  window onward, with the hack pinned and the rotation off. A hack whose step
  varies is judged on the median across windows.
- The step to compare is the one **as first flashed**: the unmodified hack,
  with no wrapper and no optimisation. Braid's step changed 3 to 4 times with
  its optimisations, so a later number would not test the probe.

## Predictions

| Hack | Band | Host ms | Predicted device step | Why this one |
| --- | --- | --- | --- | --- |
| mountain | low | 0.0005 | 0.03-0.75 ms | A plain low: no `double`, 282 lines, xlockmore |
| epicycle | low | 0.0009 | 0.05-1.4 ms | The caveat: a low host time with 31 `double`s |
| kaleidescope | high | 0.0695 | 4-104 ms | The band edge: no `double`, no flags |
| celtic | high | 1.1246 | 67-1,690 ms | Heavy: Braid-sized host time, 42 `double`s |

The two high ranges are wide, so almost any result confirms kaleidescope. The
claims that can fail are these:

1. Both low-band hacks have a device step of 20 ms or less.
2. Epicycle's step is 1.4 ms or less, despite its `double`s.
3. Celtic's step is 50 ms or more.
4. The steps rank in the order mountain and epicycle, then kaleidescope, then
   celtic (mountain against epicycle is not claimed, as their host times are
   within a factor of two).
5. Every prediction range above contains the measured step.

A hack that does not link, crashes on the device or needs shim work to run is
a result too, and is reported as one.

## Results

Each hack was ported unmodified, flashed pinned with the rotation off, and read
for 180 seconds (29 to 35 five-second windows, the first dropped). The figures
are in `tools/assessment_measured.md`.

| Hack | Host ms | Predicted | Device step, median (range) | Device / host |
| --- | --- | --- | --- | --- |
| mountain | 0.0005 | 0.03-0.75 ms | 0.1 ms (0-0.4) | 200 |
| epicycle | 0.0009 | 0.05-1.4 ms | 0.4 ms (0.3-2.1) | 440 |
| kaleidescope | 0.0695 | 4-104 ms | 8.3 ms (7.0-9.8) | 119 |
| celtic | 1.1246 | 67-1,690 ms | 832 ms (29-1,025) | 740 |

The ratio here uses the median step. `docs/speed-backtest.md` uses the midpoint
of the measured range, which gives 469 for celtic and 1,333 for epicycle (its
2.1 ms window pulls the midpoint up).

Verdicts on the claims:

1. **Held.** Mountain's median is 0.1 ms and epicycle's 0.4 ms, both far under
   20 ms.
2. **Held on the median, not on every window.** Epicycle's median is 0.4 ms,
   under 1.4 ms despite its 31 `double`s. Two of its 34 windows read 1.5 and
   2.1 ms, above the range; the other 32 were 0.3 to 0.8 ms. They are probably
   the windows containing a restart, but that was not checked.
3. **Held.** Celtic's median is 832 ms. One window read 29 ms.
4. **Held.** 0.1 and 0.4 ms, then 8.3 ms, then 832 ms.
5. **Held on the medians.** Every median is inside its range, apart from the two
   epicycle windows above.

After the four were measured, Rod looked at them on the display and took Celtic
out of the rotation: some of its pictures drew quickly and others barely drew
anything. It is still in the tree, unregistered, and listed in
`tools/failed_ports.txt`; issue #37 tracks looking for optimisations. Its
result stays in the backtest, probed standalone, because it is the one slow
hack in this set.

All five claims passed. What that does and does not show:

- Four hacks with wide ranges can only fail a prediction, not confirm the
  bands. The informative claims were that the low-band hacks would be quick and
  that celtic would be slow, and both held. Kaleidescope's range spans a factor
  of 25, so it was close to unfalsifiable.
- Kaleidescope is a high-band hack that runs well (8.3 ms, 31 fps). It is the
  same kind of result as coral, drift, cloudlife and substrate in the fitted
  set: the high band catches every slow hack, and some that are not. Over all 33
  hacks the high band holds 13, and 8 of them have a device step of 50 ms or
  more.
- The ratio of device to host time again followed the `double` count: 119 and
  200 for the two hacks with none, 440 for epicycle (31) and 740 for celtic
  (42). Four points are an anecdote, not a fit.
- The probe says nothing about memory. Celtic held about 300 KB of the 325 KB
  of free internal heap in its first picture and its `assert()` aborts on a
  failed allocation, which the host time gave no hint of.
- I chose these four knowing their host times and the 29 earlier results, so
  this is a test of the bands, not of my choice of hacks.
- No hack landed near a band edge except kaleidescope (0.0695 against 0.05),
  so how well the thresholds themselves hold is still unmeasured.
