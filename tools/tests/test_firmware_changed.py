"""CI runs the firmware tests only when a change can alter their result."""

import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import firmware_changed


@pytest.mark.parametrize(
    "path",
    [
        "firmware/src/runner/runner.c",
        "firmware/platformio.ini",
        "firmware/test/test_hacks/test_hacks.c",
        "firmware/badge_font.py",
        "tools/tinygl_patch.py",
        "tools/xs_vendor.py",
        "tools/fetch_xscreensaver.py",
        ".depot/workflows/ci.yml",
        "tools/firmware_changed.py",
    ],
)
def test_paths_that_can_change_firmware_results(path):
    assert firmware_changed.affects_firmware([path])


@pytest.mark.parametrize(
    "path",
    [
        "pyproject.toml",
        "uv.lock",
        "renovate.json",
        ".pre-commit-config.yaml",
        "README.md",
        "docs/porting-assessment.md",
        "tools/tests/test_score.py",
        ".claude/rules/testing.md",
        "THIRD_PARTY_NOTICES.md",
    ],
)
def test_paths_that_cannot(path):
    assert not firmware_changed.affects_firmware([path])


def test_one_relevant_path_among_many_is_enough():
    assert firmware_changed.affects_firmware(["uv.lock", "firmware/src/main.cpp"])


def test_no_changes_means_no_run():
    assert not firmware_changed.affects_firmware([])


def test_cli_prints_true_for_firmware_change():
    out = run_cli("firmware/src/main.cpp\nuv.lock\n")
    assert out == "firmware=true\n"


def test_cli_prints_false_for_tooling_only_change():
    assert run_cli("uv.lock\npyproject.toml\n") == "firmware=false\n"


def test_cli_prints_false_for_empty_diff():
    assert run_cli("") == "firmware=false\n"


def run_cli(stdin: str) -> str:
    script = Path(firmware_changed.__file__)
    return subprocess.run(
        [sys.executable, str(script)],
        input=stdin,
        capture_output=True,
        text=True,
        check=True,
    ).stdout
