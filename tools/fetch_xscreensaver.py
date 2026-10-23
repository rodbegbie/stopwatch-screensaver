"""Fetch and extract the latest xscreensaver source into vendor/.

Run with: uv run tools/fetch_xscreensaver.py [--version X.YY] [--force]
[--cache-dir DIR]
"""

import argparse
import hashlib
import re
import shutil
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request
from pathlib import Path, PurePosixPath

import xs_vendor

BASE_URL = "https://www.jwz.org/xscreensaver/"
DOWNLOAD_PAGE = BASE_URL + "download.html"
SURVEYED_VERSION = xs_vendor.VERSION
USER_AGENT = (
    "stopwatch-screensaver-fetch/1.0 (personal project; fetches xscreensaver source)"
)
TARBALL_LINK = re.compile(
    r"""href=["']?xscreensaver-(\d+(?:\.\d+)+)\.tar\.gz""", re.IGNORECASE
)


class FetchError(Exception):
    pass


def _version_key(version: str) -> tuple[int, ...]:
    return tuple(int(p) for p in version.split("."))


def find_versions(html: str) -> list[str]:
    return sorted(set(TARBALL_LINK.findall(html)), key=_version_key)


def pick_version(html: str, wanted: str | None) -> str:
    versions = find_versions(html)
    if not versions:
        raise FetchError("no xscreensaver tarball link found on the download page")
    if wanted is None:
        return versions[-1]
    if wanted not in versions:
        raise FetchError(f"version {wanted} not listed (found: {', '.join(versions)})")
    return wanted


def safe_extract(tar_path: Path, dest: Path) -> Path:
    try:
        return _extract(tar_path, dest)
    except tarfile.ReadError as err:
        raise FetchError(f"{tar_path.name} is not a readable tarball: {err}") from err


def _extract(tar_path: Path, dest: Path) -> Path:
    with tarfile.open(tar_path) as tar:
        tops = set()
        for member in tar.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts:
                raise FetchError(f"unsafe path in tarball: {member.name}")
            if name.parts:
                tops.add(name.parts[0])
        if len(tops) != 1:
            raise FetchError(f"expected one top-level directory, found {sorted(tops)}")
        try:
            tar.extractall(dest, filter="data")
        except (tarfile.TarError, OSError) as err:
            raise FetchError(f"unsafe tarball: {err}") from err
    return dest / tops.pop()


def _open(url: str, timeout: int):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    return urllib.request.urlopen(request, timeout=timeout)


def fetch_text(url: str) -> str:
    try:
        with _open(url, 30) as resp:
            return resp.read().decode("utf-8", "replace")
    except (urllib.error.URLError, TimeoutError) as err:
        raise FetchError(f"could not fetch {url}: {err}") from err


def download(url: str, target: Path) -> str:
    sha = hashlib.sha256()
    try:
        with _open(url, 60) as resp, target.open("wb") as out:
            while chunk := resp.read(1 << 16):
                sha.update(chunk)
                out.write(chunk)
    except (urllib.error.URLError, TimeoutError) as err:
        raise FetchError(f"could not download {url}: {err}") from err
    return sha.hexdigest()


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain_tarball(url: str, tar_path: Path, expected: str | None) -> str:
    """Return the sha256 of the tarball at tar_path, downloading it if needed.

    With an expected hash, a file already there that differs is replaced, and
    a download that differs is refused before it reaches tar_path.
    """
    if tar_path.is_file():
        sha256 = sha256_of(tar_path)
        if expected is None or sha256 == expected:
            return sha256
        print(
            f"warning: {tar_path.name} has sha256 {sha256}, not the pinned "
            f"{expected}; downloading it again",
            file=sys.stderr,
        )
    part = tar_path.with_name(tar_path.name + ".part")
    try:
        sha256 = download(url, part)
        if expected is not None and sha256 != expected:
            raise FetchError(f"{url} has sha256 {sha256}, not the pinned {expected}")
        part.replace(tar_path)
    finally:
        part.unlink(missing_ok=True)
    return sha256


def run(args: argparse.Namespace) -> None:
    cache = Path(args.cache_dir) if args.cache_dir else None
    if (
        args.version
        and cache
        and (cache / f"xscreensaver-{args.version}.tar.gz").is_file()
    ):
        version = args.version
    else:
        version = pick_version(fetch_text(DOWNLOAD_PAGE), args.version)
    if version != SURVEYED_VERSION:
        print(
            f"warning: surveyed against {SURVEYED_VERSION}, fetching {version}; "
            "re-check the call survey and porting assessment",
            file=sys.stderr,
        )
    dest = Path(args.dest)
    target = dest / f"xscreensaver-{version}"
    if target.exists() and not args.force:
        print(f"xscreensaver-{version} already present at {target}, skipping")
        return
    url = f"{BASE_URL}xscreensaver-{version}.tar.gz"
    expected = xs_vendor.SHA256 if version == xs_vendor.VERSION else None
    if expected is None:
        print(f"warning: no pinned sha256 for {version}, not verified", file=sys.stderr)
    with tempfile.TemporaryDirectory() as tmp:
        store = cache or Path(tmp)
        store.mkdir(parents=True, exist_ok=True)
        tar_path = store / f"xscreensaver-{version}.tar.gz"
        sha256 = obtain_tarball(url, tar_path, expected)
        print(f"version {version}\nurl {url}\nsha256 {sha256}")
        extracted = safe_extract(tar_path, Path(tmp) / "x")
        dest.mkdir(parents=True, exist_ok=True)
        if target.exists():
            shutil.rmtree(target)
        shutil.move(str(extracted), str(target))
    print(f"extracted to {target}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", help="specific version, e.g. 6.17")
    parser.add_argument("--force", action="store_true", help="replace existing")
    parser.add_argument("--dest", default="vendor", help="default: vendor")
    parser.add_argument(
        "--cache-dir",
        help="keep the tarball here and reuse it instead of downloading",
    )
    args = parser.parse_args(argv)
    try:
        run(args)
    except FetchError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
