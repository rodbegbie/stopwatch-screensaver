"""The xscreensaver release the firmware's hacks are copied from. Change the
version here when upgrading (see `.claude/skills/update-xscreensaver/`)."""

from pathlib import Path

VERSION = "6.17"
UPSTREAM = Path("vendor") / f"xscreensaver-{VERSION}"


def upstream(repo: Path) -> Path:
    return repo / UPSTREAM
