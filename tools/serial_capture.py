"""Reads the StopWatch's serial log into a file for a number of seconds.

    .venv/bin/python -I tools/serial_capture.py <out-file> <seconds> [port]

Opening the port resets the board (a random hack starts, the overlay returns to
nothing), and a flash or a crash makes the port go away and come back, so this
reopens it in a loop. DTR and RTS are set low before each open: a reopen loop
without that once parked the chip in its bootloader. Run it in the background with absolute paths; background
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
            s = serial.Serial()
            s.port = port
            s.baudrate = 115200
            s.timeout = 1
            s.dtr = False
            s.rts = False
            try:
                s.open()
                while time.time() < end:
                    f.write(s.read(4096))
                    f.flush()
            except (serial.SerialException, OSError):
                time.sleep(0.5)
            finally:
                s.close()


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    capture(sys.argv[1], float(sys.argv[2]), *(sys.argv[3:]))
