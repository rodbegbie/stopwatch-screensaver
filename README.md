# stopwatch-screensaver

xscreensaver "hacks" running on the M5Stack StopWatch (SKU C152, ESP32-S3,
466×466 round AMOLED), through a small X11 shim. A learning project in
embedded development.

Right now it runs five hacks, unmodified from xscreensaver 6.16: **Pyro**
(fireworks), **HyperCube**, **XSpirograph**, **Petri** (mould growth) and
**Helix**, at about 22 fps (the line-heavy ones about 10). The
[porting assessment](docs/porting-assessment.md) rates every other hack by
porting effort.

## How it works

- `firmware/src/core/` is a 466×466 RGB565 canvas with clipped drawing
  primitives, held in PSRAM.
- `firmware/src/x11shim/` implements the Xlib calls hacks use, drawing into
  that canvas, and serves each hack's default settings as its resources.
- `firmware/src/hacks/` holds the copied hack sources. They compile
  unmodified against the shim's own `screenhack.h`.
- `firmware/src/runner/` starts, steps and switches hacks.
- `firmware/src/main.cpp` pushes the canvas to the display each frame and
  handles the buttons.

The same code builds natively on the Mac, which is how the tests run and how
frames are dumped to PNG without any hardware.

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

Button A starts the next hack and button B the previous one, wrapping
around at either end. The order is Pyro, HyperCube, XSpirograph, Petri, Helix.

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
keep their own notices and are listed in `THIRD_PARTY_NOTICES.md`.
