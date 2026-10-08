## Measured on the device

Five hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM).

| Hack | Frame rate | Extra PSRAM while running |
| --- | --- | --- |
| Pyro | about 23 fps | about 80 KB |
| HyperCube | about 22 fps | none measurable |
| Petri | about 22-24 fps | about 1.3 MB |
| XSpirograph | about 9.6 fps | none measurable |
| Helix | about 10-15 fps | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

Pushing the 434 KB canvas to the display takes most of a frame, so about 23
fps is the ceiling for any hack today. XSpirograph is the exception that
proves it: it draws 1000 lines per frame, and writing those pixels into the
PSRAM canvas is slow, which halves its frame rate. Helix, another line-heavy
hack, behaves the same way. Hacks that draw many primitives per frame are the
ones to profile first.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.
