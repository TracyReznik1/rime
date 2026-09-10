import configparser
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from skin_store import SkinStore
import test_sogou_import as fixtures


class StoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.store = SkinStore(self.root / "用户目录")
        self.store.user_dir.mkdir()
        self.yaml = self.store.user_dir / "weasel.custom.yaml"
        self.yaml.write_bytes(b'patch:\n  "style/color_scheme": google\n')
        self.original = self.yaml.read_bytes()
        self.fixture = fixtures.ImportTests()
        self.fixture.root = self.root
        self.notify = patch("skin_store.notify_renderers")
        self.notify.start()
        self.addCleanup(self.notify.stop)

    def test_switch_default_and_undo_preserve_yaml(self):
        a, _ = self.store.import_file(self.fixture.archive())
        b, _ = self.store.import_file(self.fixture.archive())
        self.store.apply(a["id"])
        first = self.store.active.read_bytes()
        self.store.apply(b["id"])
        self.store.undo()
        self.assertEqual(first, self.store.active.read_bytes())
        self.store.default()
        self.assertEqual("", self.store.current_manifest())
        self.store.undo()
        self.assertEqual(first, self.store.active.read_bytes())
        self.assertEqual(self.original, self.yaml.read_bytes())

    def test_bad_import_does_not_change_activation(self):
        a, _ = self.store.import_file(self.fixture.archive())
        self.store.apply(a["id"])
        before = self.store.active.read_bytes()
        with self.assertRaises(Exception):
            self.store.import_file(self.yaml)
        self.assertEqual(before, self.store.active.read_bytes())
        self.assertEqual(1, len(self.store.entries()))

    def test_remove_is_scoped_reversible_and_blocks_active(self):
        a, _ = self.store.import_file(self.fixture.archive())
        self.store.apply(a["id"])
        with self.assertRaises(ValueError):
            self.store.remove(a["id"])
        with self.assertRaises(ValueError):
            self.store.remove("../../outside")
        self.store.default()
        self.store.remove(a["id"])
        self.assertEqual([], self.store.entries())
        self.assertTrue(next((self.store.root / ".trash").iterdir()).joinpath("background.png").exists())
        self.assertEqual(self.original, self.yaml.read_bytes())

    def test_existing_external_activation_can_be_restored(self):
        external = "[skin]\nmanifest=D:\\外部皮肤\\skin.ini\n".encode("utf-16")
        self.store.active.write_bytes(external)
        a, _ = self.store.import_file(self.fixture.archive())
        self.store.apply(a["id"])
        self.store.undo()
        self.assertEqual(external, self.store.active.read_bytes())


if __name__ == "__main__":
    unittest.main()
