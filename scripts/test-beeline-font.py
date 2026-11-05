#!/usr/bin/env python3
"""Regression checks for the actual font generator; no hardware simulation here."""
import csv
import importlib.util
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("beeline_font", HERE / "generate-beeline-font.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class FontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.selection = module.load_source()

    def test_all_codes_and_composition(self):
        outputs = module.make_outputs(self.selection)
        rows = list(csv.DictReader(io.StringIO(outputs[module.ROOT / "docs/firmware-font-codes.csv"])))
        self.assertEqual(len(rows), 46)
        self.assertEqual(len({r["icone_icomoon"] for r in rows}), 35)
        self.assertEqual({r["codepoint"] for r in rows},
                         {f"U+{c:04X}" for c in range(0xE900, 0xE92E)})
        self.assertEqual(sum(bool(r["comando_firmware"]) for r in rows), 6)
        fork = [r for r in rows if r["icone_icomoon"] == "nav_fork_straight"]
        self.assertEqual(len(fork), 3)

    def test_generated_files_match(self):
        for path, content in module.make_outputs(self.selection).items():
            self.assertEqual(path.read_text(), content, str(path))

    def test_mask_bit_order_bounds_and_budget(self):
        icons = {i["properties"]["name"]: i for i in self.selection["icons"]}
        masks = []
        for command, name in module.ACTIVE:
            mask = module.mask_for(icons[name]["properties"]["code"])
            bits = module.packed_mask(mask)
            self.assertEqual(len(bits), 1152)
            left, top, right, bottom = mask.getbbox()
            self.assertGreaterEqual(left, 4)
            self.assertGreaterEqual(top, 4)
            self.assertLessEqual(right, 92)
            self.assertLessEqual(bottom, 92)
            for y in range(96):
                for x in range(96):
                    actual = bool(bits[y * 12 + x // 8] & (0x80 >> (x % 8)))
                    self.assertEqual(actual, bool(mask.getpixel((x, y))), (command, x, y))
            masks.append(bits)
        self.assertEqual(len(set(masks)), 6)
        self.assertEqual(sum(map(len, masks)), 6912)

    def test_modified_map_is_rejected(self):
        original = module.SOURCE
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            shutil.copy2(original / "gps-signs.ttf", source / "gps-signs.ttf")
            data = json.loads((original / "selection.json").read_text())
            data["icons"][0]["properties"]["codes"][0] = 0xE92D
            (source / "selection.json").write_text(json.dumps(data))
            try:
                module.SOURCE = source
                with self.assertRaises(ValueError):
                    module.load_source()
            finally:
                module.SOURCE = original

    def test_modified_outline_is_rejected(self):
        original = module.SOURCE
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            shutil.copy2(original / "gps-signs.ttf", source / "gps-signs.ttf")
            data = json.loads((original / "selection.json").read_text())
            data["icons"][0]["icon"]["paths"][0] = "M0 0 L1 0 L1 1 Z"
            (source / "selection.json").write_text(json.dumps(data))
            try:
                module.SOURCE = source
                with self.assertRaises(ValueError):
                    module.load_source()
            finally:
                module.SOURCE = original


if __name__ == "__main__":
    unittest.main(verbosity=2)
