"""Import static Sogou SSF horizontal skins for the patched Weasel renderer.

No executables or archive paths are extracted. Only validated image pixels and
a small, versioned manifest are written. Activation is explicit and reversible.
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tempfile
import zipfile

from PIL import Image

MAX_ARCHIVE = 32 * 1024 * 1024
MAX_EXPANDED = 64 * 1024 * 1024


def decode_ini(data):
    encodings = ["utf-16"] if data.startswith((b"\xff\xfe", b"\xfe\xff")) else ["utf-8-sig", "gb18030"]
    for encoding in encodings:
        try:
            return data.decode(encoding)
        except UnicodeError:
            pass
    raise ValueError("skin.ini encoding is not UTF-8, UTF-16 or GB18030")


def numbers(value, count):
    result = tuple(int(v.strip()) for v in value.split(","))
    if len(result) != count or any(v < 0 or v > 8192 for v in result):
        raise ValueError(f"Invalid geometry: {value}")
    return result


def read_skin(source):
    source = Path(source)
    if source.stat().st_size > MAX_ARCHIVE:
        raise ValueError("SSF exceeds 32 MiB")
    with zipfile.ZipFile(source) as archive:
        members = archive.infolist()
        if len(members) > 512 or sum(m.file_size for m in members) > MAX_EXPANDED:
            raise ValueError("SSF expanded size/count exceeds limit")
        names = {}
        for member in members:
            name = member.filename.replace("\\", "/")
            path = PurePosixPath(name)
            if path.is_absolute() or ".." in path.parts or ":" in name:
                raise ValueError("Unsafe archive path")
            key = str(path).casefold()
            if key in names:
                raise ValueError("Duplicate archive path")
            names[key] = member
        configs = [key for key in names if PurePosixPath(key).name == "skin.ini"]
        if len(configs) != 1:
            raise ValueError("SSF must contain exactly one skin.ini")
        ini = configparser.ConfigParser(interpolation=None, strict=True)
        ini.read_string(decode_ini(archive.read(names[configs[0]])))
        if not ini.has_section("Scheme_H1") or not ini.has_option("Scheme_H1", "pic"):
            raise ValueError("Only static, single-background Scheme_H1 skins are supported (H2/animation are not)")
        section = ini["Scheme_H1"]
        picture = section["pic"].replace("\\", "/")
        picture_path = PurePosixPath(picture)
        if picture_path.is_absolute() or ".." in picture_path.parts or ":" in picture:
            raise ValueError("Unsafe picture reference")
        key = str(PurePosixPath(configs[0]).parent / picture).casefold()
        if key not in names:
            raise ValueError("Referenced background image is missing")
        with Image.open(io.BytesIO(archive.read(names[key]))) as original:
            if original.format not in {"PNG", "BMP"} or getattr(original, "n_frames", 1) != 1:
                raise ValueError("Only static PNG/BMP backgrounds are supported")
            if not (1 <= original.width <= 8192 and 1 <= original.height <= 8192) or original.width * original.height > 16_000_000:
                raise ValueError("Background dimensions exceed limit")
            image = original.convert("RGBA")
        horizontal = numbers(section["layout_horizontal"], 3)
        vertical = numbers(section["layout_vertical"], 3)
        if horizontal[0] != 0 or vertical[0] != 0:
            raise ValueError("Only stretch mode 0 is supported; tiled backgrounds are not")
        left, right = horizontal[1:]
        top, bottom = vertical[1:]
        if left + right >= image.width or top + bottom >= image.height:
            raise ValueError("Background has no stretchable center")
        # Sogou margin order: top, bottom, left, right. Candidate top is
        # the gap below preedit, not a second absolute window coordinate.
        preedit = numbers(section["pinyin_marge"], 4)
        candidate = numbers(section["zhongwen_marge"], 4)
        display = ini["Display"] if ini.has_section("Display") else {}
        def color(name, default):
            value = int(display.get(name, default), 0)
            if not 0 <= value <= 0xFFFFFF:
                raise ValueError("Invalid COLORREF")
            return str(value | 0xFF000000)
        font_size = int(display.get("font_size", "16"))
        if not 6 <= font_size <= 72:
            raise ValueError("Unsupported font size")
        manifest = {
            "version": "1", "image": "background.png",
            "left": str(left), "right": str(right), "top": str(top), "bottom": str(bottom),
            "inset_left": str(max(preedit[2], candidate[2])),
            "inset_right": str(max(preedit[3], candidate[3])),
            "inset_top": str(preedit[0]), "inset_bottom": str(candidate[1]),
            # SSF uses pixel sizes at 96 DPI; Weasel stores typographic points.
            "spacing": str(candidate[0]), "font_point": str(max(6, round(font_size * 72 / 96))),
            "text_color": color("pinyin_color", "0x333333"),
            "candidate_text_color": color("zhongwen_color", "0x333333"),
            "hilited_candidate_text_color": color("zhongwen_first_color", "0x993366"),
        }
        warnings = ["Horizontal H1 only; vertical, split H2 windows and the Sogou status bar are not imported.",
                    "Native Weasel candidate spacing and selection remain; margins are conservatively combined, not pixel-identical to Sogou."]
        return image, manifest, warnings


def import_skin(source, destination):
    image, manifest, warnings = read_skin(source)
    destination = Path(destination).resolve()
    if destination.exists():
        raise ValueError("Destination already exists; choose a new directory")
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".ssf-", dir=destination.parent))
    try:
        image.save(staging / "background.png")
        config = configparser.ConfigParser(interpolation=None)
        config["skin"] = manifest
        with (staging / "skin.ini").open("w", encoding="utf-16") as handle:
            config.write(handle)
        report = {"source": str(Path(source).resolve()), "sha256": hashlib.sha256(Path(source).read_bytes()).hexdigest(), "warnings": warnings}
        (staging / "import-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        staging.rename(destination)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return destination, warnings


def activate(destination, rime_dir):
    rime_dir = Path(rime_dir).resolve()
    rime_dir.mkdir(parents=True, exist_ok=True)
    target = rime_dir / "sogou-skin.ini"
    if target.exists():
        backup = rime_dir / "sogou-skin.ini.bak"
        if backup.exists():
            raise ValueError("Previous activation backup exists; restore/remove it before activating again")
        shutil.copy2(target, backup)
    config = configparser.ConfigParser(interpolation=None)
    config["skin"] = {"manifest": str(Path(destination).resolve() / "skin.ini")}
    temp = target.with_suffix(".tmp")
    with temp.open("w", encoding="utf-16") as handle:
        config.write(handle)
    os.replace(temp, target)


def main():
    # Windows redirected consoles may otherwise fail after a successful import.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ssf", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--activate", type=Path, metavar="RIME_USER_DIR", help="Enable in patched Weasel; official Weasel ignores it")
    args = parser.parse_args()
    try:
        directory, warnings = import_skin(args.ssf, args.output)
        if args.activate:
            activate(directory, args.activate)
        print(f"Imported: {directory}")
        for warning in warnings:
            print(f"NOTE: {warning}")
    except (ValueError, OSError, zipfile.BadZipFile, configparser.Error) as error:
        parser.exit(1, f"Import failed: {error}\n")


if __name__ == "__main__":
    main()
