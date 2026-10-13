import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import vendor_diff as vd


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


@pytest.fixture
def repo(tmp_path):
    old, new = tmp_path / "vendor/xs-1.0", tmp_path / "vendor/xs-1.1"
    for tree in (old, new):
        write(tree / "hacks/same.c", "same\n")
        write(tree / "hacks/config/same.xml", "<x/>\n")
    write(old / "hacks/glx/moved.c", "v1\n")
    write(new / "hacks/glx/moved.c", "v2\n")
    write(old / "hacks/gone.c", "gone\n")
    write(old / "hacks/images/sprites.png", "png1\n")
    write(new / "hacks/images/sprites.png", "png2\n")
    write(old / "hacks/glx/words.txt", "w\n")
    write(new / "hacks/glx/fresh.c", "new hack\n")
    write(new / "hacks/config/fresh.xml", "<x/>\n")
    write(new / "hacks/helper.c", "not a hack: no config\n")
    write(tmp_path / "firmware/src/hacks/same/same.c", "same\n")
    write(tmp_path / "firmware/src/hacks/moved/moved.c", "v1\n")
    write(tmp_path / "firmware/src/hacks/gone/gone.c", "gone\n")
    write(tmp_path / "firmware/src/hacks/ours.c", "ours\n")
    write(tmp_path / "firmware/sprites.py", 'for name in ("sprites",):\n')
    write(tmp_path / "tools/words.py", 'source = "words.txt"\n')
    write(old / "hacks/config/sprites.xml", "<old/>\n")
    write(new / "hacks/config/sprites.xml", "<new/>\n")
    return tmp_path, old, new


def test_a_copied_file_that_changed_upstream_is_listed(repo):
    root, old, new = repo
    report = vd.compare(root, old, new)
    assert ("firmware/src/hacks/moved/moved.c", "hacks/glx/moved.c") in report.changed


def test_a_copied_file_that_upstream_removed_is_listed(repo):
    root, old, new = repo
    assert vd.compare(root, old, new).removed == [
        ("firmware/src/hacks/gone/gone.c", "hacks/gone.c")
    ]


def test_unchanged_and_own_files_are_not_listed(repo):
    root, old, new = repo
    report = vd.compare(root, old, new)
    listed = [f for f, _ in report.changed + report.removed]
    assert "firmware/src/hacks/same/same.c" not in listed
    assert "firmware/src/hacks/ours.c" not in listed


def test_a_build_input_that_changed_or_went_is_listed(repo):
    root, old, new = repo
    assert vd.compare(root, old, new).build_inputs == [
        "hacks/glx/words.txt (removed)",
        "hacks/images/sprites.png (changed)",
    ]


def test_new_hacks_are_those_with_a_new_config(repo):
    root, old, new = repo
    assert vd.compare(root, old, new).new_hacks == ["fresh"]


def test_main_prints_each_section(repo, capsys):
    root, old, new = repo
    assert vd.main([str(old), str(new), "--root", str(root)]) == 0
    out = capsys.readouterr().out
    assert "hacks/glx/moved.c" in out
    assert "hacks/gone.c" in out
    assert "sprites.png" in out
    assert "fresh" in out
