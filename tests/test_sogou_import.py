import configparser
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
import zipfile

from PIL import Image

spec = importlib.util.spec_from_file_location("import_skin", Path(__file__).resolve().parents[1] / "tools/import_sogou_skin.py")
skin = importlib.util.module_from_spec(spec)
spec.loader.exec_module(skin)

INI = """[General]
skin_name=测试
[Display]
font_size=16
zhongwen_color=0x123456
[Scheme_H1]
pic=bg.png
layout_horizontal=0,10,10
layout_vertical=0,5,5
pinyin_marge=8,2,12,4
zhongwen_marge=3,5,10,8
"""


class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def archive(self, ini=INI, extra=None, encoding="utf-8"):
        buffer = io.BytesIO()
        Image.new("RGBA", (50, 30), (200, 100, 100, 128)).save(buffer, format="PNG")
        source = self.root / "test.ssf"
        with zipfile.ZipFile(source, "w") as archive:
            archive.writestr("skin.ini", ini.encode(encoding))
            archive.writestr("bg.png", buffer.getvalue())
            for name, content in (extra or {}).items():
                archive.writestr(name, content)
        return source

    def test_import_preserves_alpha_and_color(self):
        directory, warnings = skin.import_skin(self.archive(), self.root / "out")
        self.assertTrue(warnings)
        with Image.open(directory / "background.png") as image:
            self.assertEqual(image.getpixel((0, 0)), (200, 100, 100, 128))
        config = configparser.ConfigParser()
        config.read(directory / "skin.ini", encoding="utf-16")
        self.assertEqual(config["skin"]["candidate_text_color"], str(0xFF123456))
        self.assertEqual(config["skin"]["inset_left"], "12")
        self.assertEqual(config["skin"]["font_point"], "12")

    def test_chinese_encodings(self):
        for encoding in ["gb18030", "utf-16", "utf-8-sig"]:
            with self.subTest(encoding=encoding):
                skin.read_skin(self.archive(encoding=encoding))

    def test_path_traversal_rejected_without_output(self):
        for name in ["../escape", "C:/escape", "/escape", "a/../../escape"]:
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    skin.import_skin(self.archive(extra={name: b"x"}), self.root / "out")
                self.assertFalse((self.root / "out").exists())

    def test_duplicate_case_insensitive_names(self):
        with self.assertRaises(ValueError):
            skin.read_skin(self.archive(extra={"BG.PNG": b"x"}))

    def test_bad_references_and_layout(self):
        for ini in [INI.replace("bg.png", "../bg.png"),
                    INI.replace("bg.png", "missing.png"),
                    INI.replace("0,10,10", "1,10,10"),
                    INI.replace("0,10,10", "0,40,40"),
                    INI.replace("Scheme_H1", "Scheme_H2"),
                    INI.replace("font_size=16", "font_size=999")]:
            with self.subTest(ini=ini):
                with self.assertRaises(ValueError):
                    skin.read_skin(self.archive(ini))

    def test_existing_output_never_overwritten(self):
        output = self.root / "out"
        output.mkdir()
        (output / "keep").write_text("original")
        with self.assertRaises(ValueError):
            skin.import_skin(self.archive(), output)
        self.assertEqual((output / "keep").read_text(), "original")

    def test_activation_does_not_modify_rime_yaml(self):
        rime = self.root / "rime"
        rime.mkdir()
        existing = rime / "weasel.custom.yaml"
        existing.write_text("patch: {}")
        directory, _ = skin.import_skin(self.archive(), self.root / "out")
        skin.activate(directory, rime)
        first = (rime / "sogou-skin.ini").read_bytes()
        skin.activate(directory, rime)
        self.assertEqual((rime / "sogou-skin.ini.bak").read_bytes(), first)
        with self.assertRaises(ValueError):
            skin.activate(directory, rime)
        self.assertEqual(existing.read_text(), "patch: {}")


if __name__ == "__main__":
    unittest.main()
