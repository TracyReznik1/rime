"""User-owned skin library. No administrator rights, processes or YAML edits."""
from __future__ import annotations
import configparser
import io
import json
import os
from pathlib import Path
import tempfile
import uuid

from import_sogou_skin import import_skin


def user_directory():
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Rime\Weasel") as key:
            value, _ = winreg.QueryValueEx(key, "RimeUserDir")
            if value:
                return Path(value)
    except OSError:
        pass
    return Path(os.environ["APPDATA"]) / "Rime"


def atomic_write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=".skin-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def notify_renderers():
    if os.name == "nt":
        import ctypes
        user32 = ctypes.windll.user32
        message = user32.RegisterWindowMessageW("Rime.Weasel.SogouSkinChanged.v1")
        user32.PostMessageW(0xFFFF, message, 0, 0)


class SkinStore:
    def __init__(self, user_dir):
        self.user_dir = Path(user_dir).resolve()
        self.root = self.user_dir / "skins"
        self.active = self.user_dir / "sogou-skin.ini"
        self.backup = self.user_dir / "sogou-skin.previous.ini"

    def entries(self):
        if not self.root.exists():
            return []
        result = []
        for path in self.root.iterdir():
            if path.is_symlink() or not path.is_dir() or len(path.name) != 32:
                continue
            try:
                uuid.UUID(hex=path.name)
                data = json.loads((path / "library.json").read_text(encoding="utf-8"))
                if (path / "skin.ini").is_file() and (path / "background.png").is_file():
                    result.append({"id": path.name, "name": str(data["name"]), "path": path})
            except (ValueError, KeyError, OSError):
                continue
        return sorted(result, key=lambda item: (item["name"].casefold(), item["id"]))

    def entry(self, identity):
        for entry in self.entries():
            if entry["id"] == identity:
                return entry
        raise ValueError("皮肤不存在或已损坏，请重新导入。")

    def import_file(self, source):
        source = Path(source)
        identity = uuid.uuid4().hex
        destination, warnings = import_skin(source, self.root / identity)
        # Do not retain local source paths in the managed library.
        report_path = destination / "import-report.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        report["source"] = source.name
        atomic_write(report_path, json.dumps(report, ensure_ascii=False, indent=2).encode("utf-8"))
        atomic_write(destination / "library.json", json.dumps(
            {"version": 1, "name": source.stem, "id": identity}, ensure_ascii=False).encode("utf-8"))
        return self.entry(identity), warnings

    def current_manifest(self):
        try:
            ini = configparser.ConfigParser(interpolation=None)
            ini.read_string(self.active.read_text(encoding="utf-16"))
            return ini.get("skin", "manifest", fallback="")
        except (OSError, UnicodeError, configparser.Error):
            return ""

    def _activate_manifest(self, manifest):
        ini = configparser.ConfigParser(interpolation=None)
        ini["skin"] = {"manifest": str(manifest)}
        stream = io.StringIO()
        ini.write(stream)
        new = stream.getvalue().encode("utf-16")
        old = self.active.read_bytes() if self.active.exists() else "[skin]\nmanifest=\n".encode("utf-16")
        if old != new:
            atomic_write(self.backup, old)
            atomic_write(self.active, new)
        notify_renderers()

    def apply(self, identity):
        self._activate_manifest(self.entry(identity)["path"] / "skin.ini")

    def default(self):
        self._activate_manifest("")

    def undo(self):
        if not self.backup.is_file():
            raise ValueError("还没有可恢复的上一次配色。")
        previous = self.backup.read_bytes()
        current = self.active.read_bytes() if self.active.exists() else "[skin]\nmanifest=\n".encode("utf-16")
        atomic_write(self.active, previous)
        atomic_write(self.backup, current)
        notify_renderers()

    def remove(self, identity):
        entry = self.entry(identity)
        if self.current_manifest() and Path(self.current_manifest()).resolve() == entry["path"] / "skin.ini":
            raise ValueError("请先应用另一款皮肤或恢复默认，再移除此皮肤。")
        # Reversible removal; never recursively delete user-provided paths.
        trash = self.root / ".trash"
        trash.mkdir(parents=True, exist_ok=True)
        entry["path"].rename(trash / (identity + "-" + uuid.uuid4().hex))
