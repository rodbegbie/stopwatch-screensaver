"""CI runs the firmware tests only when a change can alter their result."""

import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import firmware_changed

SCRIPT = Path(firmware_changed.__file__)


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


def lock(**versions: str) -> str:
    return "version = 1\n\n" + "".join(
        f'[[package]]\nname = "{name}"\nversion = "{version}"\n\n'
        for name, version in versions.items()
    )


@pytest.mark.parametrize("package", ["platformio", "pillow", "machfs"])
def test_upgrading_a_build_package_counts(package):
    base = lock(**{package: "1.0.0", "ruff": "0.1.0"})
    head = lock(**{package: "1.1.0", "ruff": "0.1.0"})
    assert firmware_changed.build_packages_changed(base, head)


@pytest.mark.parametrize("package", ["ruff", "ty", "pytest", "pre-commit", "click"])
def test_upgrading_anything_else_does_not(package):
    base = lock(**{package: "1.0.0", "pillow": "13.0.0"})
    head = lock(**{package: "1.1.0", "pillow": "13.0.0"})
    assert not firmware_changed.build_packages_changed(base, head)


def test_a_build_package_added_or_removed_counts():
    assert firmware_changed.build_packages_changed(lock(ruff="1"), lock(pillow="1"))
    assert firmware_changed.build_packages_changed(lock(pillow="1"), lock(ruff="1"))


def test_a_missing_lock_on_one_side_counts():
    assert firmware_changed.build_packages_changed(None, lock(pillow="1"))
    assert firmware_changed.build_packages_changed(lock(pillow="1"), None)


def test_no_lock_on_either_side_is_no_change():
    assert not firmware_changed.build_packages_changed(None, None)


def git(repo: Path, *args: str) -> None:
    subprocess.run(
        [
            "git",
            "-c",
            "user.name=t",
            "-c",
            "user.email=t@example.com",
            "-c",
            "commit.gpgsign=false",
            *args,
        ],
        cwd=repo,
        check=True,
        capture_output=True,
    )


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    git(tmp_path, "init", "-b", "main")
    (tmp_path / "firmware").mkdir()
    (tmp_path / "firmware" / "main.c").write_text("int main(void) { return 0; }\n")
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs" / "a.md").write_text("one\ntwo\nthree\nfour\nfive\n")
    (tmp_path / "uv.lock").write_text(lock(pillow="13.0.0", ty="0.0.95"))
    git(tmp_path, "add", ".")
    git(tmp_path, "commit", "-m", "base")
    git(tmp_path, "switch", "-c", "pr")
    return tmp_path


def verdict(repo: Path) -> str:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "main"],
        cwd=repo,
        capture_output=True,
        text=True,
        check=True,
    ).stdout


def commit_all(repo: Path) -> None:
    git(repo, "add", ".")
    git(repo, "commit", "-m", "change")


def test_cli_true_for_a_firmware_change(repo):
    (repo / "firmware" / "main.c").write_text("int main(void) { return 1; }\n")
    commit_all(repo)
    assert verdict(repo) == "firmware=true\n"


def test_cli_false_for_docs_only(repo):
    (repo / "docs" / "a.md").write_text("changed\n")
    commit_all(repo)
    assert verdict(repo) == "firmware=false\n"


def test_cli_false_for_no_changes(repo):
    assert verdict(repo) == "firmware=false\n"


def test_cli_false_when_the_lock_moves_only_a_dev_tool(repo):
    (repo / "uv.lock").write_text(lock(pillow="13.0.0", ty="0.0.96"))
    commit_all(repo)
    assert verdict(repo) == "firmware=false\n"


def test_cli_true_when_the_lock_moves_pillow(repo):
    (repo / "uv.lock").write_text(lock(pillow="13.1.0", ty="0.0.95"))
    commit_all(repo)
    assert verdict(repo) == "firmware=true\n"


def test_cli_true_when_a_firmware_file_is_moved_out_of_the_way(repo):
    (repo / "docs" / "old").mkdir()
    git(repo, "mv", "firmware/main.c", "docs/old/main.c")
    commit_all(repo)
    assert verdict(repo) == "firmware=true\n"


def test_cli_ignores_changes_already_on_main(repo):
    git(repo, "switch", "main")
    (repo / "firmware" / "main.c").write_text("int main(void) { return 2; }\n")
    commit_all(repo)
    git(repo, "switch", "pr")
    (repo / "docs" / "a.md").write_text("changed\n")
    commit_all(repo)
    assert verdict(repo) == "firmware=false\n"
