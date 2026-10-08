import io
import sys
import tarfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import fetch_xscreensaver as fx  # noqa: E402

FIXTURE = (Path(__file__).parent / "fixtures" / "download.html").read_text()


def make_tar(path: Path, members: dict[str, bytes], symlinks=None):
    with tarfile.open(path, "w:gz") as tar:
        for name, data in members.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
        for name, target in (symlinks or {}).items():
            info = tarfile.TarInfo(name)
            info.type = tarfile.SYMTYPE
            info.linkname = target
            tar.addfile(info)


def test_find_versions_ignores_dmg_and_apk():
    assert fx.find_versions(FIXTURE) == ["6.16"]


def test_versions_sort_numerically():
    html = (
        '<a href="xscreensaver-6.9.tar.gz">a</a>'
        '<a href="xscreensaver-6.16.tar.gz">b</a>'
    )
    assert fx.find_versions(html) == ["6.9", "6.16"]
    assert fx.pick_version(html, None) == "6.16"


def test_pick_version_no_link_raises():
    with pytest.raises(fx.FetchError):
        fx.pick_version("<html></html>", None)


def test_pick_version_unknown_wanted_raises():
    with pytest.raises(fx.FetchError):
        fx.pick_version(FIXTURE, "1.00")


def test_pick_version_wanted_found():
    assert fx.pick_version(FIXTURE, "6.16") == "6.16"


def test_safe_extract_ok(tmp_path):
    tar = tmp_path / "a.tar.gz"
    make_tar(tar, {"xscreensaver-9.9/a.txt": b"hi"})
    dest = tmp_path / "out"
    dest.mkdir()
    top = fx.safe_extract(tar, dest)
    assert top == dest / "xscreensaver-9.9"
    assert (top / "a.txt").read_bytes() == b"hi"


def test_safe_extract_rejects_traversal(tmp_path):
    tar = tmp_path / "evil.tar.gz"
    make_tar(tar, {"../evil.txt": b"x"})
    dest = tmp_path / "out"
    dest.mkdir()
    with pytest.raises(fx.FetchError):
        fx.safe_extract(tar, dest)
    assert not (tmp_path / "evil.txt").exists()


def test_safe_extract_rejects_absolute_and_symlink_escape(tmp_path):
    dest = tmp_path / "out"
    dest.mkdir()
    absolute = tmp_path / "abs.tar.gz"
    make_tar(absolute, {"/tmp/xs-evil.txt": b"x"})
    with pytest.raises(fx.FetchError):
        fx.safe_extract(absolute, dest)
    link = tmp_path / "link.tar.gz"
    make_tar(link, {"top/a": b"x"}, symlinks={"top/l": "../../escape"})
    with pytest.raises(fx.FetchError):
        fx.safe_extract(link, dest)


def test_main_skips_existing_dir_without_force(tmp_path, monkeypatch, capsys):
    (tmp_path / "xscreensaver-6.16").mkdir()
    (tmp_path / "xscreensaver-6.16" / "keep").write_text("k")
    monkeypatch.setattr(fx, "fetch_text", lambda url: FIXTURE)

    def boom(*a, **k):
        raise AssertionError("must not download")

    monkeypatch.setattr(fx, "download", boom)
    assert fx.main(["--dest", str(tmp_path)]) == 0
    assert (tmp_path / "xscreensaver-6.16" / "keep").exists()
    assert "already present" in capsys.readouterr().out


def test_main_warns_when_not_6_16(tmp_path, monkeypatch, capsys):
    html = '<a href="xscreensaver-6.17.tar.gz">x</a>'
    monkeypatch.setattr(fx, "fetch_text", lambda url: html)
    (tmp_path / "xscreensaver-6.17").mkdir()
    assert fx.main(["--dest", str(tmp_path)]) == 0
    assert "re-check" in capsys.readouterr().err


def test_main_network_failure_is_clean_error(tmp_path, monkeypatch, capsys):
    def fail(url):
        raise fx.FetchError("network down")

    monkeypatch.setattr(fx, "fetch_text", fail)
    assert fx.main(["--dest", str(tmp_path)]) == 1
    assert "network down" in capsys.readouterr().err


def _serve_recording_user_agent():
    import http.server
    import threading

    seen = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            seen.append(self.headers.get("User-Agent", ""))
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"ok")

        def log_message(self, *args):
            pass

    server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, seen


def test_requests_send_descriptive_user_agent(tmp_path):
    server, seen = _serve_recording_user_agent()
    try:
        url = f"http://127.0.0.1:{server.server_port}/x"
        assert fx.fetch_text(url) == "ok"
        fx.download(url, tmp_path / "f")
    finally:
        server.shutdown()
    assert len(seen) == 2
    assert all("Python-urllib" not in ua and "stopwatch-screensaver" in ua for ua in seen)
