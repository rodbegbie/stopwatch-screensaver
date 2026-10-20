# stopwatch-screensaver

[xscreensaver](https://www.jwz.org/xscreensaver/) "hacks" running on the
[M5Stack StopWatch](https://shop.m5stack.com/products/m5stack-stopwatch-dev-kit-esp32-s3)
(SKU C152, ESP32-S3, 466×466 round AMOLED), through a small X11 shim. A
learning project in embedded development.

![The StopWatch running a rotation of xscreensaver hacks](docs/video.avif)

The hack sources are copied byte for byte from xscreensaver 6.17, and the
shim is extended to run them rather than the hacks being edited. A few are
built through a thin wrapper: single precision for the `double`-heavy ones
(the ESP32-S3's FPU handles only `float`), a build-time patched copy of Maze
to fit its maze in memory, and a safe `free` for Blaster. The full list, in
button order, is `g_hacks[]` in `firmware/src/hacks/registry.c`. The
[porting assessment](docs/porting-assessment.md) rates every xscreensaver hack
by porting effort and lists the measurements taken on the device.

Gears, Morph3D and CubicGrid are OpenGL hacks. They run through a small GL
layer over [TinyGL](https://github.com/C-Chads/tinygl), a software OpenGL by
Fabrice Bellard (this project uses TinyGL; see `THIRD_PARTY_NOTICES.md`),
drawing at roughly 4 to 30 fps depending on the hack and the random scene it
starts with.

Frame rates run from under 1 to dozens of fps, mostly set by the delay each
hack asks for: some hold each finished picture for seconds. Only the rows a
hack drew are sent to the display, so a hack that draws little costs little.

## How it works

- `firmware/src/core/` is a 466×466 RGB565 canvas with clipped drawing
  primitives, held in PSRAM.
- `firmware/src/x11shim/` implements the Xlib calls hacks use, drawing into
  that canvas (including pixmaps and clip masks), and serves each hack's
  default settings as its resources.
- `firmware/src/glshim/` and `firmware/src/tinygl/` are the OpenGL layer and
  the TinyGL copy beneath it, for the GL hacks.
- `firmware/src/hacks/` holds the copied hack sources. They compile
  unmodified against the shim's own `screenhack.h`.
- `firmware/src/runner/` starts, steps and switches hacks, painting each
  hack's background colour before it starts, as xscreensaver does.
- `firmware/src/main.cpp` pushes the canvas to the display each frame,
  draws the name, badge, QR code, fps and battery overlays, and handles the
  buttons and touch. A small task on core 0 watches both buttons, so a press
  made while a hack is in a long draw step is kept, not lost.

`tools/` has the scripts that fetch xscreensaver, score each hack for porting
effort, and check licence notices. The same firmware code builds natively on the
Mac, which is how the tests run and how frames are dumped to PNG without any
hardware.

## Prerequisites

- macOS with the Xcode command line tools
- [`uv`](https://docs.astral.sh/uv/)
- The StopWatch connected over USB-C

## Setup

Everything installs inside this folder.

```bash
uv run tools/fetch_xscreensaver.py      # downloads xscreensaver into vendor/
uv venv .venv
VIRTUAL_ENV=.venv uv pip install platformio
source tools/env.sh                     # keeps PlatformIO inside this folder
cd firmware
pio test -e native                      # host tests (canvas, shim, hacks)
pio run -e stopwatch -t upload          # build and flash the device
```

The scripts in `tools/` have their own tests, run from the repo root with
`uv run --with pytest --with pillow pytest tools/tests`.

The serial port is set in `firmware/platformio.ini`
(`/dev/cu.usbmodem112401`). Change it if your device shows up elsewhere. If
the upload cannot connect, hold the power button for about 2 seconds until
the green LED lights, then try again.

To see a frame without the device:

```bash
cd firmware && pio run -e dump
.pio/build/dump/program 0 900 /tmp/pyro.raw
cd .. && uv run tools/rgb565_to_png.py /tmp/pyro.raw 466 466 /tmp/pyro.png
```

## Controls

The device starts on a random hack and moves on to the next one every 90
seconds. Button A starts the next hack and button B the previous one, wrapping
around at either end, and either press restarts the 90 second count. The
hack's name shows for a few seconds as it starts, and a tap on the screen
cycles the readout: nothing (the default at boot), a name badge, a QR code, the
frame rate, then the battery level. The badge is "Rod" over "Begbie" in large letters,
centred on the screen, in place of the hack's name; build with
`-DBADGE_NAME=\"Ann\"` to change it. If `vendor/pixChicago.ttf` is present
the badge is drawn in that pixel font, rasterised at build time (the font is
not in the repository, because its licence is unclear; `PIX_CHICAGO_TTF`
names another path). Without it the badge uses M5GFX's built-in 8x8 font.
The QR code is white-backed and covers
most of the screen, pointing at `https://linkedin.com/in/rodbegbie`; build with
`-DQR_URL=\"https://example.com\"` to change it. The frame rate and battery
sit below the hack's name. The choice stays when the hack changes.

Build flags change the defaults (set them with `PLATFORMIO_BUILD_FLAGS`, and
rebuild without them afterwards, since the define sticks to the build):

- `-DSTART_HACK=\"galaxy\"` always starts on that hack.
- `-DROTATE_SECONDS=5` changes the rotation time, and `0` turns it off.
- `-DBADGE_NAME=\"Ann\"` and `-DQR_URL=\"https://example.com\"` set the name
  badge text and the QR code's target, as described above.
- `-DBADGE_SCALE=7` sets the badge's text size, a whole number (7 with
  pixChicago, 8 without).

A press made during a long draw step takes effect when the step ends. Extra
presses in that wait count as one, and holding a button does not repeat.

## Flying Toasters (optional)

`ad-flyingtoasters` is our own recreation of the After Dark 2.0 screensaver.
Its art is Berkeley Systems' and is never committed. The build extracts it
from your own copy of the After Dark 2.0 disk image, which is available from
the [Internet Archive](https://archive.org/details/after-dark-2.0v-800k).
Save the image as `vendor/after-dark-v2.0v-800k`, or point `AFTER_DARK_IMAGE`
at it. Without the disk the hack is left out and the build says so.

## Porting a hack

To add another hack, pick an easy one from the
[porting assessment](docs/porting-assessment.md), then follow the
`port-hack` skill in `.claude/skills/port-hack/`, which gives the order of
work and a recipe for each trap. `AGENTS.md` holds the rules (copied hacks stay
byte-identical, every copied file gets a licence entry) and the gotchas.

## Restoring the original firmware

Flashing replaces the conference firmware. Two ways back:

- **Browser installer:** open <https://workos.com/init/badge/install> in
  desktop Chrome or Edge. This works because the partition table is
  unchanged (`app3M_fat9M_16MB`).
- **Exact restore from a backup:** if you took one before the first flash
  (`backups/stopwatch-original-16MB.bin`, git-ignored), write it back with:

```bash
source tools/env.sh
python .platformio/packages/tool-esptoolpy/esptool.py \
  --port /dev/cu.usbmodem112401 --baud 921600 \
  write_flash 0x0 backups/stopwatch-original-16MB.bin
```

This restores the whole flash, including the badge profile.

## Cleaning up

```bash
rm -rf .venv .platformio firmware/.pio vendor backups
```

## Licence

Our code is MIT licensed (see `LICENSE`). Files copied from xscreensaver
keep their own notices and are listed in `THIRD_PARTY_NOTICES.md`. TinyGL is
under its own zlib-style licence, which requires crediting it: this product
uses TinyGL, and the firmware runs an altered copy (see
`THIRD_PARTY_NOTICES.md`).
