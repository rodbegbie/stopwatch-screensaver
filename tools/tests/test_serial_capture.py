import sys
from pathlib import Path
from typing import ClassVar

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import serial
import serial_capture


class FakePort:
    opened_with: ClassVar[list[tuple[bool, bool]]] = []
    opens = 0

    def __init__(self, *args, **kwargs):
        self.dtr = True
        self.rts = True
        self.port = None
        self.baudrate = None
        self.timeout = None
        self.args = args
        if args:
            self.port = args[0]
            self._open()

    def _open(self):
        FakePort.opens += 1
        FakePort.opened_with.append((self.dtr, self.rts))
        if FakePort.opens == 1:
            raise serial.SerialException("port went away")

    def open(self):
        self._open()

    def read(self, n):
        return b"run Fake fps=1.0\n"

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def test_the_port_is_opened_with_dtr_and_rts_low(tmp_path, monkeypatch):
    FakePort.opened_with = []
    FakePort.opens = 0
    monkeypatch.setattr(serial, "Serial", FakePort)
    monkeypatch.setattr(serial_capture.time, "sleep", lambda s: None)
    out = tmp_path / "log"
    serial_capture.capture(str(out), 0.05, "/dev/fake")
    assert FakePort.opened_with
    assert all(opened == (False, False) for opened in FakePort.opened_with)
    assert b"run Fake" in out.read_bytes()
