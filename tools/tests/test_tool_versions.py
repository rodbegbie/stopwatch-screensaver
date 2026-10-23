"""The same tool is pinned in several places; they must name one version."""

import re
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRE_COMMIT = (ROOT / ".pre-commit-config.yaml").read_text()
WORKFLOW = (ROOT / ".depot/workflows/ci.yml").read_text()


def pre_commit_version(repo: str) -> str:
    block = re.search(
        rf"repo: https://github\.com/{repo}\n(.*?)(?=\n  - repo:|\Z)",
        PRE_COMMIT,
        re.DOTALL,
    )
    assert block, f"{repo} is not in .pre-commit-config.yaml"
    frozen = re.search(r"# frozen: v?(\S+)", block.group(1))
    assert frozen, f"{repo} has no '# frozen: vX' comment"
    return frozen.group(1)


def dev_pin(name: str) -> str:
    pyproject = tomllib.loads((ROOT / "pyproject.toml").read_text())
    for dep in pyproject["dependency-groups"]["dev"]:
        if match := re.fullmatch(rf"{name}==(\S+)", dep):
            return match.group(1)
    raise AssertionError(f"{name} is not pinned with == in the dev group")


def test_ruff_matches_pre_commit():
    assert dev_pin("ruff") == pre_commit_version("astral-sh/ruff-pre-commit")


def test_workflow_python_matches_pyproject():
    requires = tomllib.loads((ROOT / "pyproject.toml").read_text())["project"][
        "requires-python"
    ]
    pinned = re.fullmatch(r"==(\d+\.\d+\.\d+)", requires)
    assert pinned, f"requires-python is {requires!r}, not an exact ==X.Y.Z pin"
    workflow = re.search(r'PYTHON_VERSION: "(\S+)"', WORKFLOW)
    assert workflow, "ci.yml has no PYTHON_VERSION"
    assert workflow.group(1) == pinned.group(1)


def test_workflow_uv_matches_pre_commit():
    pinned = re.search(r'UV_VERSION: "(\S+)"', WORKFLOW)
    assert pinned, "ci.yml has no UV_VERSION"
    assert pinned.group(1) == pre_commit_version("astral-sh/uv-pre-commit")
