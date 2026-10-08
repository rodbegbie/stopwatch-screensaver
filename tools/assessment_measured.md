## Measured on the device

Seven hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM).

| Hack | Frame rate | Extra PSRAM while running |
| --- | --- | --- |
| Pyro | about 23 fps | about 80 KB |
| HyperCube | about 22 fps | none measurable |
| Petri | about 22-24 fps | about 1.3 MB |
| XSpirograph | about 9.6 fps | none measurable |
| Helix | about 10-15 fps | none measurable |
| Rorschach | about 9 fps | none measurable |
| Pedal | 1 fps by design (not a speed limit) | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

Pushing the 434 KB canvas to the display takes most of a frame, so about 23
fps is the ceiling for any hack today. XSpirograph is the exception that
proves it: it draws 1000 lines per frame, and writing those pixels into the
PSRAM canvas is slow, which halves its frame rate. Helix, another line-heavy
hack, behaves the same way. Hacks that draw many primitives per frame are the
ones to profile first.

Pedal asks the runner for a one-second delay after each picture, so its
frame rate says nothing about speed. Rorschach asks for 20 ms between frames
and holds each finished picture for 5 seconds; its 9 fps is below the ceiling
and has not been profiled.

Rorschach keeps a 9.6 KB array on the stack, which overflowed the Arduino
loop task's default 8 KB stack and rebooted the device on the first frame. The
firmware now sets a 16 KB loop stack (about 8 KB less free heap). Any hack with
large local arrays can hit the same limit.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.
