"""Tests of the catalogue tool: the readers, the classifier and the committed output.

    python -I -m unittest discover -s plugins/cartographer/tools -p "test_*.py"

Everything runs without game data except test_regenerates, which is skipped only when no extracted D2R `data`
folder is found (see catalogue.data_root; CARTOGRAPHER_NO_DATA=1 forces that). A $D2R_DATA that is wrong fails it.
The other output tests check the committed catalogue.tsv.
"""
from __future__ import annotations

import contextlib
import io
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import catalogue as cat  # noqa: E402
import d2r_readers as rd  # noqa: E402


def dt1(tiles: dict[tuple[int, int, int], bytes]) -> bytes:
    """A DT1 holding tiles (orientation, main, sub) -> 25 subtile flags."""
    head = bytearray(276)
    struct.pack_into("<2i", head, 268, len(tiles), 276)
    out = bytearray(head)
    for (o, m, s), flags in tiles.items():
        t = bytearray(96)
        struct.pack_into("<3i", t, 20, o, m, s)
        t[40:65] = flags
        out += t
    return bytes(out)


def ds1(w: int, h: int, floor: dict[tuple[int, int], int],
        files=("c:\\d2\\data\\global\\tiles\\act3\\a.tg1",)) -> bytes:
    """A version 18 DS1 of w x h tiles with one wall layer (empty) and one floor layer; floor cell -> raw value."""
    out = bytearray(struct.pack("<3i", 18, w - 1, h - 1))
    out += struct.pack("<2i", 3, 0)  # act, tag type
    out += struct.pack("<i", len(files)) + b"".join(f.encode() + b"\0" for f in files)
    out += struct.pack("<2i", 1, 1)  # walls, floors
    out += bytes(w * h * 4 * 2)  # the wall layer: tiles, then orientations
    grid = [0] * (w * h)
    for (x, y), v in floor.items():
        grid[y * w + x] = v
    return bytes(out + struct.pack(f"<{w * h}I", *grid))


OPEN, BLOCKED = bytes(25), bytes([1] * 25)
FLOOR_OPEN, FLOOR_BLOCKED = 1 << 20, 2 << 20  # main index 1 and 2


def jungle_row(spec, fragment):
    """The spec row whose pattern contains `fragment` (with {m} written as in the source)."""
    return next(r for r in spec.rows if fragment in r[0])


class Readers(unittest.TestCase):
    def test_dt1_tiles(self):
        t = rd.dt1_tiles(dt1({(0, 1, 0): OPEN, (0, 2, 0): BLOCKED}))
        self.assertEqual(t[(0, 1, 0)], OPEN)
        self.assertEqual(t[(0, 2, 0)], BLOCKED)

    def test_ds1_header_and_dt1_names(self):
        data = ds1(4, 3, {})
        v, w, h, act, _tag, _o, walls, floors = rd.ds1_header(data)
        self.assertEqual((v, w, h, act, walls, floors), (18, 4, 3, 3, 1, 1))
        self.assertEqual(rd.header_dt1s(data), ["act3/a.dt1"])

    def test_layout_flags_and_unwalkable(self):
        data = ds1(2, 1, {(0, 0): FLOOR_OPEN, (1, 0): FLOOR_BLOCKED | rd.UNWALKABLE})
        lay = rd.layout(data, {"x.dt1": {(0, 1, 0): OPEN, (0, 2, 0): BLOCKED}})
        self.assertEqual(lay.flags[(0, 0)], OPEN)
        self.assertEqual(lay.flags[(1, 0)], BLOCKED)
        self.assertEqual(lay.keys[(0, 0)], ("f", (("x.dt1", 0, 1, 0),)))
        self.assertEqual(lay.unwalkable, frozenset({(1, 0)}))


class BadInput(unittest.TestCase):
    def tmp(self) -> Path:
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        return Path(d.name)

    def test_a_corrupt_dt1_is_named(self):
        d = self.tmp()
        tiles = d / "global" / "tiles"
        tiles.mkdir(parents=True)
        (tiles / "a.ds1").write_bytes(b"")
        r = cat.Reader(d)
        with mock.patch.object(cat, "header_dt1s", return_value=["bad.dt1"]):
            (tiles / "bad.dt1").write_bytes(b"")
            with self.assertRaises(cat.CatalogueError) as e:
                r.layout("a.ds1")
        self.assertIn("bad.dt1", str(e.exception))
        self.assertIn("DT1", str(e.exception))

    def test_missing_data_folder_names_the_path(self):
        d = self.tmp() / "nowhere"
        with self.assertRaises(cat.CatalogueError) as e:
            cat.generate(d)
        self.assertIn("nowhere", str(e.exception))

    def test_a_wrong_d2r_data_is_an_error_not_no_data(self):
        d = self.tmp() / "nowhere"
        with mock.patch.dict(os.environ, {"D2R_DATA": str(d), "CARTOGRAPHER_NO_DATA": ""}):
            with self.assertRaises(cat.CatalogueError) as e:
                cat.data_root()
        self.assertIn("D2R_DATA", str(e.exception))

    def test_no_data_is_explicit(self):
        with mock.patch.dict(os.environ, {"CARTOGRAPHER_NO_DATA": "1"}):
            self.assertIsNone(cat.data_root())
            with self.assertRaises(cat.CatalogueError):
                cat.generate()

    def test_main_prints_a_message_not_a_traceback(self):
        d = self.tmp() / "nowhere"
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(cat.main(["--check", "--data", str(d)]), 2)
        self.assertIn("nowhere", err.getvalue())

    def test_corrupt_ds1_names_the_file(self):
        root = self.tmp()
        (root / "global" / "tiles" / "act3").mkdir(parents=True)
        (root / "global" / "tiles" / "act3" / "bad.ds1").write_bytes(ds1(4, 4, {})[:20])
        with self.assertRaises(cat.CatalogueError) as e:
            cat.Reader(root).layout("act3/bad.ds1")
        self.assertIn("bad.ds1", str(e.exception))


class Sockets(unittest.TestCase):
    def lay(self, blocked: set[tuple[int, int]], w=3, h=3):
        floor = {(x, y): FLOOR_BLOCKED if (x, y) in blocked else FLOOR_OPEN for x in range(w) for y in range(h)}
        return rd.layout(ds1(w, h, floor), {"x.dt1": {(0, 1, 0): OPEN, (0, 2, 0): BLOCKED}})

    def test_all_open(self):
        self.assertEqual(cat.ds1_sockets(self.lay(set()), 3, 3, 3, 3), dict(N="p", E="p", S="p", W="p"))

    def test_blocked_side_is_closed(self):
        west = {(0, y) for y in range(3)}
        self.assertEqual(cat.ds1_sockets(self.lay(west), 3, 3, 3, 3), dict(N="p", E="p", S="p", W="x"))

    def test_macro_cells_along_a_side(self):
        # 2 x 1 cells of 3 x 3 tiles; the second cell's north row is blocked
        lay = self.lay({(x, 0) for x in range(3, 6)}, 6, 3)
        s = cat.ds1_sockets(lay, 6, 3, 3, 3)
        self.assertEqual((s["N"], s["S"], s["E"], s["W"]), ("px", "pp", "p", "p"))


class Classifier(unittest.TestCase):
    def test_masks(self):
        self.assertEqual(cat.mask_of("NSEW"), 15)
        self.assertEqual([cat.mask_of(c) for c in ("W", "E", "S", "N")], [1, 2, 4, 8])
        self.assertEqual(cat.letters_of(11), "NEW")

    def test_valid_mask_order(self):
        self.assertTrue(cat.valid_mask("NSE"))
        self.assertFalse(cat.valid_mask("EN"))
        self.assertFalse(cat.valid_mask(""))

    def test_named_mask(self):
        m, letters = cat.named_mask(r"^Act 3 - Jungle {m}$", "Act 3 - Jungle NEW")
        self.assertEqual(letters, "NEW")
        self.assertIsNone(cat.named_mask(r"^Act 3 - Jungle {m}$", "Act 3 - Jungle W E")[0])  # a transition, not a mask
        self.assertIsNone(cat.named_mask(r"^Act 3 - Jungle {m}$", "Act 3 - Jungle Head")[0])

    def test_shape(self):
        self.assertEqual([cat.shape_tag(m) for m in (1, 3, 12, 5, 7, 15)],
                         ["deadend", "straight", "straight", "corner", "tee", "cross"])

    def test_row_from_name_alone(self):
        p = cat.Piece(536, "Act 3 - Jungle SEW", 32, 32, ("act3/jungle/rivsew.ds1",))
        spec = next(s for s in cat.TILESETS if s.name == "Jungle")
        pattern, role, extra = jungle_row(spec, "Jungle {m}")
        m, letters = cat.named_mask(pattern, p.name)
        row = cat.make_row(spec, p, m, letters, role, extra, None, lambda _p: True)
        self.assertEqual((row["edgeN"], row["edgeE"], row["edgeS"], row["edgeW"]), ("x", "p", "p", "p"))
        self.assertEqual((row["w"], row["h"], row["source"], row["builder"]), (1, 1, "name", "outdoor"))
        self.assertEqual(row["hd"], "hd/env/preset/act3/jungle/rivsew.json")

    def test_missing_hd_is_marked(self):
        p = cat.Piece(916, "Act 5 - Barricade 2", 16, 16, ("expansion/siege/a.ds1", "expansion/siege/b.ds1"))
        spec = next(s for s in cat.TILESETS if s.name == "Barricade")
        m, letters = cat.named_mask(spec.rows[0][0], p.name)
        row = cat.make_row(spec, p, m, letters, "path", (), None, lambda path: path.endswith("a.json"))
        self.assertEqual(row["hd"].split(";")[1], "MISSING:hd/env/preset/expansion/siege/b.json")

    def test_hd_path(self):
        self.assertEqual(cat.hd_path("act3/jungle/rivw.ds1"), "hd/env/preset/act3/jungle/rivw.json")

    def test_overrides_win_and_are_checked(self):
        rows = [dict(lvlprest=1, role="path", weight=1, source="name", check="ok", w=1, h=1, leveltype=3)]
        problems = cat.apply_overrides(rows, [dict(lvlprest="1", role="boss", weight="5"),
                                              dict(lvlprest="2", role="boss"),
                                              dict(lvlprest="1", role="nonsense"),
                                              dict(lvlprest="1", check="ok")])
        self.assertEqual((rows[0]["role"], rows[0]["weight"], rows[0]["source"]), ("boss", 5, "override"))
        self.assertEqual(len(problems), 3)

    def test_bad_override_number_is_a_problem_not_a_traceback(self):
        rows = [dict(lvlprest=1, role="path", weight=1, source="name", check="ok", w=1, h=1, leveltype=3)]
        problems = cat.apply_overrides(rows, [dict(lvlprest="1", weight="many")])
        self.assertEqual(len(problems), 1)
        self.assertIn("weight", problems[0])
        self.assertEqual((rows[0]["weight"], rows[0]["source"]), (1, "name"))

    def test_variants_survive_when_the_sockets_come_from_the_ds1(self):
        class Two:  # two DS1 files whose north side differs
            def layout(self, f):
                blocked = {(x, 0) for x in range(3, 6)} if f.endswith("b.ds1") else set()
                floor = {(x, y): FLOOR_BLOCKED if (x, y) in blocked else FLOOR_OPEN
                         for x in range(6) for y in range(3)}
                return rd.layout(ds1(6, 3, floor), {"x.dt1": {(0, 1, 0): OPEN, (0, 2, 0): BLOCKED}})
        p = cat.Piece(573, "Act 3 - Jungle Head", 6, 3, ("act3/jungle/a.ds1", "act3/jungle/b.ds1"))
        spec = next(s for s in cat.TILESETS if s.name == "Jungle")._replace(cell=3)
        pattern, role, extra = jungle_row(spec, "Head|Tail")
        m, letters = cat.named_mask(pattern, p.name)
        row = cat.make_row(spec, p, m, letters, role, extra, Two(), lambda _p: True)
        self.assertEqual(row["check"], "variants")
        self.assertEqual(row["source"], "ds1")

    def test_variants_win_over_ds1_in_a_named_piece(self):
        class Two:  # two DS1 files; the first has every side open, the second blocks the north side
            def layout(self, f):
                blocked = {(x, 0) for x in range(3)} if f.endswith("b.ds1") else set()
                floor = {(x, y): FLOOR_BLOCKED if (x, y) in blocked else FLOOR_OPEN
                         for x in range(3) for y in range(3)}
                return rd.layout(ds1(3, 3, floor), {"x.dt1": {(0, 1, 0): OPEN, (0, 2, 0): BLOCKED}})
        p = cat.Piece(1, "Act 3 - Jungle S", 3, 3, ("act3/jungle/a.ds1", "act3/jungle/b.ds1"))
        spec = next(s for s in cat.TILESETS if s.name == "Jungle")._replace(cell=3)
        pattern, role, extra = jungle_row(spec, "Jungle {m}")
        m, letters = cat.named_mask(pattern, p.name)
        row = cat.make_row(spec, p, m, letters, role, extra, Two(), lambda _p: True)
        self.assertEqual(row["check"], "variants")

    def test_no_data_accepts_only_1_or_true(self):
        for v, skipped in (("1", True), ("true", True), ("0", False), ("", False)):
            with mock.patch.dict(os.environ, {"CARTOGRAPHER_NO_DATA": v, "D2R_DATA": ""}):
                if skipped:
                    self.assertIsNone(cat.data_root(), v)
                else:
                    try:
                        cat.data_root()
                    except cat.CatalogueError:
                        pass  # asked for no data folder found: an error, not a skip

    def test_transition_name_is_checked_against_the_ds1(self):
        class One:  # every side open
            def layout(self, f):
                return rd.layout(ds1(3, 3, {(x, y): FLOOR_OPEN for x in range(3) for y in range(3)}),
                                 {"x.dt1": {(0, 1, 0): OPEN}})
        spec = next(s for s in cat.TILESETS if s.name == "Jungle")._replace(cell=3)
        pattern, role, extra = jungle_row(spec, "(?P<to>")
        for name, check in (("Act 3 - Jungle NSEW N", "ok"), ("Act 3 - Jungle EW S", "name=SEW")):
            p = cat.Piece(1, name, 3, 3, ("act3/jungle/a.ds1",))
            m, letters = cat.named_mask(pattern, p.name)
            row = cat.make_row(spec, p, m, letters, role, extra, One(), lambda _p: True)
            self.assertEqual(row["check"], check, name)

    def test_every_tileset_has_a_builder(self):
        self.assertEqual({s.builder for s in cat.TILESETS}, {"maze", "outdoor"})
        self.assertEqual(len({s.name for s in cat.TILESETS}), len(cat.TILESETS))


# ---- the committed catalogue -------------------------------------------------------------------------------------

def committed() -> list[dict]:
    lines = (HERE / "catalogue.tsv").read_text(encoding="utf-8").split("\n")
    head = lines[0].split("\t")
    return [dict(zip(head, ln.split("\t"))) for ln in lines[1:] if ln]


def mask_of_row(r: dict) -> int:
    return sum(cat.BITS[s] for s in "NESW" if r["edge" + s] == "p")


# D2's own order: the jungle block LvlPrest 529 + mask is "Jungle <letters>"; hand-checked, outdoor plan 1.3
HAND_CHECKED = {529 + m: m for m in range(1, 16)}
CLEARING_MASKS = (1, 2, 3, 4, 5, 6, 8, 9, 10, 12)  # W E EW S SW SE N NW NE NS
for _base in (575, 585, 595):  # Webby, Boggy, Pygmy
    HAND_CHECKED.update({_base + k: m for k, m in enumerate(CLEARING_MASKS)})


class Committed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = committed()
        cls.by_id = {int(r["lvlprest"]): r for r in cls.rows}

    def test_columns(self):
        self.assertEqual(list((HERE / "catalogue.tsv").read_text(encoding="utf-8").split("\n")[0].split("\t")),
                         cat.COLUMNS)

    def test_masks_530_to_604_match_the_hand_checked_masks(self):
        for pid, mask in HAND_CHECKED.items():
            r = self.by_id[pid]
            self.assertEqual(mask_of_row(r), mask, f"{pid} {r['name']}")
            self.assertEqual(r["check"], "ok", f"{pid} {r['name']}: the DS1 disagrees with the name")
            self.assertEqual(r["source"], "name")
            self.assertEqual((r["w"], r["h"]), ("1", "1"))

    def test_jungle_blocks_and_clearings_have_their_roles(self):
        for pid in HAND_CHECKED:
            self.assertEqual(self.by_id[pid]["role"], "path" if pid < 545 else "clearing")

    def test_transitions_are_borders_from_the_ds1(self):
        for pid in range(545, 575):
            r = self.by_id[pid]
            self.assertEqual((r["role"], r["source"]), ("border", "ds1"), r["name"])
        self.assertEqual((self.by_id[573]["w"], self.by_id[573]["h"]), ("2", "1"))  # Head, 64 x 32
        self.assertEqual((self.by_id[574]["w"], self.by_id[574]["h"]), ("2", "1"))  # Tail

    def test_variants_are_kept(self):
        # the two DS1s of Head and Tail disagree on a side; the first file's edges are shipped, flagged
        self.assertEqual((self.by_id[573]["check"], self.by_id[574]["check"]), ("variants", "variants"))
        self.assertEqual(sum(1 for r in self.rows if r["check"] == "variants"), 2)
        report = (HERE / "catalogue-report.txt").read_text(encoding="utf-8")
        self.assertIn("2 with variants", report)
        self.assertIn("  573 Act 3 - Jungle Head: variants", report)

    def test_transition_names_agree_with_the_ds1(self):
        for pid in range(545, 573):
            r = self.by_id[pid]
            self.assertEqual(r["check"], "ok", f"{pid} {r['name']}: the DS1 disagrees with the letters of the name")

    def test_full_maze_families_follow_base_plus_mask(self):
        # The game indexes a maze family as base + mask in LvlPrest order; this is why maze exits come from names.
        # Act 5 Lava (1053-1058, six pieces) has no single base, so it is not a family of this kind.
        checked = 0
        for tileset in sorted({r["tileset"] for r in self.rows if r["builder"] == "maze"} - {"Act 5 Lava"}):
            fam = [r for r in self.rows if r["tileset"] == tileset and r["role"] == "path"]
            bases = {int(r["lvlprest"]) - mask_of_row(r) for r in fam}
            self.assertEqual(len(bases), 1, f"{tileset}: LvlPrest minus mask is not one base: {sorted(bases)}")
            checked += 1
        self.assertEqual(checked, 14)

    def test_mask_zero_is_never_a_piece(self):
        self.assertNotIn(529, self.by_id)  # Act 3 Town

    def test_hd_is_filled_per_piece(self):
        known_missing = {916: 1, 919: 1}  # vanilla ships no HD preset for newbarrbb2 / newbarrab2
        for r in self.rows:
            ds1_files, hd = r["ds1"].split(";"), r["hd"].split(";")
            self.assertEqual(len(hd), len(ds1_files), r["name"])
            for f, h in zip(ds1_files, hd):
                self.assertIn(h, (cat.hd_path(f), "MISSING:" + cat.hd_path(f)), r["name"])
            miss = sum(1 for h in hd if h.startswith("MISSING:"))
            self.assertEqual(miss, known_missing.get(int(r["lvlprest"]), 0), r["name"])

    def test_rows_are_well_formed(self):
        builders = {s.name: s.builder for s in cat.TILESETS}
        for r in self.rows:
            self.assertEqual(r["builder"], builders[r["tileset"]])
            self.assertIn(r["role"], cat.ROLES)
            self.assertEqual((len(r["edgeN"]), len(r["edgeS"])), (int(r["w"]),) * 2, r["name"])
            self.assertEqual((len(r["edgeE"]), len(r["edgeW"])), (int(r["h"]),) * 2, r["name"])
            self.assertTrue(set(r["edgeN"] + r["edgeE"] + r["edgeS"] + r["edgeW"]) <= set("xpc"), r["name"])
            self.assertTrue(r["ds1"] and r["hd"], r["name"])
        self.assertEqual(len(self.by_id), len(self.rows))

    def test_mazes_have_named_exits(self):
        r = self.by_id[53]  # Act 1 - Cave W
        self.assertEqual((r["tileset"], r["builder"], mask_of_row(r)), ("Cave", "maze", 1))

    def test_barricade_variants_come_from_the_ds1(self):
        for pid in range(915, 931):
            self.assertEqual(self.by_id[pid]["source"], "ds1")
            self.assertEqual(self.by_id[pid]["tileset"], "Barricade")


class Regenerates(unittest.TestCase):
    def test_regenerates(self):
        data = cat.data_root()  # raises when D2R_DATA is set but wrong
        if data is None:
            self.skipTest("no extracted D2R data folder (set D2R_DATA), or CARTOGRAPHER_NO_DATA is set")
        files, problems = cat.generate(data)
        self.assertEqual(problems, [])
        for name, text in files.items():
            self.assertEqual((HERE / name).read_text(encoding="utf-8"), text, f"{name} is stale: run catalogue.py")


if __name__ == "__main__":
    unittest.main()
