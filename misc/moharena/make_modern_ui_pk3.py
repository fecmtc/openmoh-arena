#!/usr/bin/env python3
"""Build zz_moharena_ui.pk3, the files of the optional modern UI.

The pack holds the menu and HUD files (ui/modern), the two fonts with their
licence (fonts) and the hitmarker sounds (sound/prom/hitmarkers) from
assets/main. Every path in it is new, so it replaces no stock file, and the
game only reads it when it was started in the modern UI mode.

The output is byte-for-byte reproducible (stored entries, sorted paths, fixed
timestamps and attributes, LF line ends in the text files), so the rules and
the launcher can pin a single hash whatever system built it.
"""
from __future__ import annotations

import argparse
import hashlib
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PACK_NAME = "zz_moharena_ui.pk3"
DEFAULT_SOURCE = ROOT / "assets" / "main"
DEFAULT_OUTPUT = ROOT / "build" / PACK_NAME

# The folders that go into the pack, relative to assets/main.
PACK_FOLDERS = (
    "fonts",
    "sound/prom/hitmarkers",
    "ui/modern",
)
# Sample menus for people writing their own. The game never loads them.
LEFT_OUT_FOLDERS = ("ui/modern/examples",)
# What each folder may hold. Anything else stops the build, so a stray file
# cannot reach the players unnoticed.
ALLOWED_SUFFIXES = {
    "fonts": {".ttf", ".txt", ".md"},
    "sound/prom/hitmarkers": {".wav"},
    "ui/modern": {".xml", ".png", ".tga"},
}
# Git may check these out with CRLF line ends on Windows.
TEXT_SUFFIXES = {".xml", ".txt", ".md"}

ZIP_DATE_TIME = (1980, 1, 1, 0, 0, 0)


def is_under(path: str, folder: str) -> bool:
    return path == folder or path.startswith(folder + "/")


def pack_files(source: Path) -> list[tuple[str, bytes]]:
    """The pack's entries, in the order they are written."""
    entries: dict[str, bytes] = {}
    seen: dict[str, str] = {}

    for folder in PACK_FOLDERS:
        base = source / folder
        if not base.is_dir():
            raise RuntimeError(f"{base} is missing.")

        for file in base.rglob("*"):
            if not file.is_file():
                continue

            path = file.relative_to(source).as_posix()
            if any(is_under(path, left_out) for left_out in LEFT_OUT_FOLDERS):
                continue

            if not path.isascii() or path != path.strip() or any(part.startswith(".") for part in path.split("/")):
                raise RuntimeError(f"{path} is not a name the pack takes.")

            if file.suffix.lower() not in ALLOWED_SUFFIXES[folder]:
                raise RuntimeError(f"{path} is not a kind of file {folder} may hold.")

            # MOH file lookups ignore case, so two such names would be one file.
            other = seen.setdefault(path.lower(), path)
            if other != path:
                raise RuntimeError(f"{path} and {other} differ only in case.")

            data = file.read_bytes()
            if file.suffix.lower() in TEXT_SUFFIXES:
                data = data.replace(b"\r\n", b"\n")

            entries[path] = data

    if not entries:
        raise RuntimeError(f"No files found in {source}.")

    return sorted(entries.items(), key=lambda entry: (entry[0].lower(), entry[0]))


def build_pack(source: Path, output: Path) -> int:
    files = pack_files(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_name(output.name + ".tmp")
    try:
        with zipfile.ZipFile(temp, "w") as archive:
            for path, data in files:
                info = zipfile.ZipInfo(path, date_time=ZIP_DATE_TIME)
                info.compress_type = zipfile.ZIP_STORED
                info.create_system = 3
                # Regular file, rw-r--r--. Non-zero, so zipfile keeps it as is.
                info.external_attr = 0o100644 << 16
                archive.writestr(info, data)
        temp.replace(output)
    finally:
        temp.unlink(missing_ok=True)
    return len(files)


def file_digests(path: Path) -> tuple[str, str]:
    data = path.read_bytes()
    return hashlib.md5(data).hexdigest(), hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Build {PACK_NAME} (the files of the optional modern UI).")
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_SOURCE,
        help=f"The assets folder to pack. Default: {DEFAULT_SOURCE.relative_to(ROOT)}",
    )
    parser.add_argument(
        "--output",
        type=Path,
        action="append",
        help=f"Where to write the pack. Repeat to write several copies. Default: {DEFAULT_OUTPUT.relative_to(ROOT)}",
    )
    args = parser.parse_args()

    source = args.source if args.source.is_absolute() else ROOT / args.source
    for output in args.output or [DEFAULT_OUTPUT]:
        output = output if output.is_absolute() else ROOT / output
        count = build_pack(source, output)
        md5, sha256 = file_digests(output)
        print(f"{output} ({count} files, {output.stat().st_size} bytes)\n  md5    {md5}\n  sha256 {sha256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
