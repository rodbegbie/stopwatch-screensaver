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

Not yet measured.
