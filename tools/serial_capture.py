"""Reads the StopWatch's serial log into a file for a number of seconds.

    .venv/bin/python -I tools/serial_capture.py <out-file> <seconds> [port]

Opening the port resets the board (a random hack starts, the overlay returns to
nothing), and a flash or a crash makes the port go away and come back, so this
reopens it in a loop. Run it in the background with absolute paths; background
shells ignore `cd`. The `run <hack> fps= step= push= ...` lines are explained in
AGENTS.md ("Serial log").
"""

import sys
import time

import serial

DEFAULT_PORT = "/dev/cu.usbmodem112401"


def capture(out: str, seconds: float, port: str = DEFAULT_PORT) -> None:
    end = time.time() + seconds
    with open(out, "wb") as f:
        while time.time() < end:
            try:
                with serial.Serial(port, 115200, timeout=1) as s:
                    while time.time() < end:
                        f.write(s.read(4096))
                        f.flush()
            except (serial.SerialException, OSError):
                time.sleep(0.5)


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    capture(sys.argv[1], float(sys.argv[2]), *(sys.argv[3:]))
