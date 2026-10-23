import subprocess
from pathlib import Path

FIRMWARE_SRC = Path(__file__).resolve().parents[2] / "firmware" / "src"
INCLUDES = [FIRMWARE_SRC, FIRMWARE_SRC / "x11shim" / "include"]


def implicit_declarations(tmp_path: Path, source: str) -> str:
    """Compile source against the shim with an empty <sys/time.h>.

    macOS's <sys/time.h> declares time(); glibc's does not. The empty stub
    makes this host behave like glibc, so a header that relies on it fails.
    """
    stub = tmp_path / "stub" / "sys"
    stub.mkdir(parents=True)
    (stub / "time.h").write_text("")
    src = tmp_path / "t.c"
    src.write_text(source)
    cmd = ["cc", "-fsyntax-only", "-Werror=implicit-function-declaration"]
    cmd += ["-DSTANDALONE", f"-I{tmp_path / 'stub'}"]
    cmd += [f"-I{d}" for d in INCLUDES] + [str(src)]
    return subprocess.run(cmd, capture_output=True, text=True, check=False).stderr


def test_screenhack_h_declares_time_without_sys_time_h(tmp_path):
    source = '#include "screenhack.h"\nlong f(void) { return (long)time(0); }\n'
    assert "time" not in implicit_declarations(tmp_path, source)
