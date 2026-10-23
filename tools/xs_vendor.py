"""The xscreensaver release the firmware's hacks are copied from. Change the
version and its tarball's sha256 here when upgrading (see
`.claude/skills/update-xscreensaver/`)."""

from pathlib import Path

VERSION = "6.17"
SHA256 = "258ccca06de822d500c249ac87aa16f0f6218ec200eadcdbe6678c7c52718271"
UPSTREAM = Path("vendor") / f"xscreensaver-{VERSION}"


def upstream(repo: Path) -> Path:
    return repo / UPSTREAM
