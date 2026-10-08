## Measured on the device

Only Pyro has been run so far (default settings, 466×466 canvas pushed to
the display every frame).

| Measurement | Value |
| --- | --- |
| Frame rate | about 23.5 fps steady |
| Frame rate during a hack restart | about 17 fps, recovering in a few seconds |
| Canvas cost | about 515 KB of PSRAM, with about 7.4 MB still free |
| Memory after 20 restarts | unchanged (no leak) |

Pyro's own delay setting asks for 100 steps per second, so the frame rate
is limited by pushing 434 KB to the display each frame. Pyro's `init`
builds two 6284-entry sine and cosine tables in double precision, which is
the cause of the dip at restart.
