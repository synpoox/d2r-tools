"""Readers the catalogue tool needs: D2R's tab-separated tables, DT1 subtile flags and DS1 tile layers.

Adapted from the DS1/DT1 readers in synpoox/pd2-resurrected `tools/presets` (own code, copied here so the plugin has
no cross-repo dependency). Only what the catalogue uses: the header of a DS1, the DT1 paths it lists, the 25 walk
flags of every DT1 tile, and the DS1's tiles with those flags OR'd per tile.
"""
from __future__ import annotations

import functools
import struct
from dataclasses import dataclass, field
from pathlib import Path

UNWALKABLE = 0x20000  # the DS1 cell flag that marks a tile unwalkable
Tile = tuple  # (DT1 path or None, orientation, main, sub)
Key = tuple  # ("w" or "f", tiles)


def norm(path: str) -> str:
    return path.replace("\\", "/").lower()


class Table:
    """A D2R excel .txt: `header` and `rows` (lists of strings, padded to the header)."""
    def __init__(self, path: Path):
        lines = path.read_bytes().decode("latin-1").replace("\r\n", "\n").split("\n")
        while lines and not lines[-1]:
            lines.pop()
        self.header = lines[0].split("\t")
        self.rows = [(line.split("\t") + [""] * len(self.header))[:len(self.header)] for line in lines[1:]]


def load(data: Path, table: str) -> Table:
    return Table(data / "global" / "excel" / f"{table}.txt")


def ds1_header(data: bytes) -> tuple[int, int, int, int, int, int, int, int]:
    """(version, width, height, act 0-4, tag type, offset of the tile layers, wall layers, floor layers). Width and
    height count tiles; the layers are walls (a tile grid, then an orientation grid, per layer), floors, shadow,
    tags."""
    version, width, height = struct.unpack_from("<3i", data, 0)
    o, act, tag = 12, 0, 0
    if version >= 8:
        act = min(struct.unpack_from("<i", data, o)[0], 4)
        o += 4
    if version >= 10:
        tag = struct.unpack_from("<i", data, o)[0]
        o += 4
    if version >= 3:
        files = struct.unpack_from("<i", data, o)[0]
        o += 4
        for _ in range(files):
            o = data.index(b"\0", o) + 1
    if 9 <= version <= 13:
        o += 8
    walls, floors = 1, 1
    if version >= 4:
        walls = struct.unpack_from("<i", data, o)[0]
        o += 4
        if version >= 16:
            floors = struct.unpack_from("<i", data, o)[0]
            o += 4
    return version, width + 1, height + 1, act, tag, o, walls, floors


def dt1_tiles(data: bytes) -> dict[tuple[int, int, int], bytes]:
    """The (orientation, main, sub) of every tile in a DT1 -> its 25 subtile flags (bit 0 blocks walking), the
    first tile's for a key held twice."""
    count, at = struct.unpack_from("<2i", data, 268)
    out = {}
    for k in range(count):
        out.setdefault(struct.unpack_from("<3i", data, at + k * 96 + 20), data[at + k * 96 + 40:at + k * 96 + 65])
    return out


def header_dt1s(data: bytes) -> list[str]:
    """The DT1 paths (under global/tiles) a DS1's header lists: the editor's .tg1 names."""
    version = struct.unpack_from("<i", data)[0]
    o = 12 + 4 * (version >= 8) + 4 * (version >= 10)
    out = []
    if version >= 3:
        n, o = struct.unpack_from("<i", data, o)[0], o + 4
        for _ in range(n):
            end = data.index(b"\0", o)
            name, o = norm(data[o:end].decode("latin-1")), end + 1
            if "/tiles/" in name:
                out.append(name.split("/tiles/", 1)[1].rsplit(".", 1)[0] + ".dt1")
    return out


def ds1_links(data: bytes, waypoint_objects: set[tuple[int, int]]) -> list[str]:
    """What a DS1 carries that links its map to another area: a waypoint object (type 2, `waypoint_objects` holds
    its (act 1-5, objpreset index) pairs), and every wall tile of orientation 10 or 11, the engine's special tiles
    that a level's Warp0..7 and Vis links come from (docs/reverse-engineering/cartographer/35-map-link-tiles.md)."""
    version = struct.unpack_from("<i", data)[0]
    if version < 4:
        raise ValueError(f"DS1 version {version}: only 4 and up keep the tile layers in the order ds1_links reads")
    version, w, h, act, tag, o, walls, floors = ds1_header(data)
    n = w * h
    out = []
    for k in range(walls):
        cells = struct.unpack_from(f"<{n}I", data, o + 2 * k * n * 4)
        orient = struct.unpack_from(f"<{n}I", data, o + (2 * k + 1) * n * 4)
        out += [f"tile {orient[i] & 0xFF}/{cells[i] >> 20 & 0x3F}/{cells[i] >> 8 & 0xFF}"
                for i in range(n) if cells[i] and orient[i] & 0xFF in (10, 11)]
    o += (2 * walls + floors + 1 + (tag in (1, 2))) * n * 4  # walls, floors, shadow, tags
    count = struct.unpack_from("<i", data, o)[0]
    for i in range(count):
        kind, ident = struct.unpack_from("<2i", data, o + 4 + i * (20 if version > 5 else 16))
        if kind == 2 and (act + 1, ident) in waypoint_objects:
            out.append(f"waypoint object {act + 1}/{ident}")
    return sorted(set(out))


class Tiles(dict):
    """DT1 path -> dt1_tiles, read from D2R's tiles on first use (an absent file holds nothing)."""
    def __init__(self, root: Path):
        super().__init__()
        self.root = root

    def __missing__(self, path: str) -> dict:
        f = self.root / path
        self[path] = dt1_tiles(f.read_bytes()) if f.is_file() else {}
        return self[path]

    def of(self, paths: list[str]) -> dict[str, dict]:
        return {p: self[p] for p in paths}


@dataclass
class Layout:
    width: int  # tiles
    height: int
    keys: dict[tuple[int, int], Key]  # (x, y) -> key; tiles with nothing are left out
    flags: dict[tuple[int, int], bytes] = field(default_factory=dict)  # (x, y) -> its layers' subtile flags, OR'd
    unwalkable: frozenset = frozenset()  # the tiles a wall or floor cell marks unwalkable (UNWALKABLE)


def layout(data: bytes, dt1s: dict[str, dict]) -> Layout:
    """The DS1's tiles, named in the DT1 files `dt1s` (path -> dt1_tiles)."""
    version, w, h, _act, _tag, o, walls, floors = ds1_header(data)
    if version < 4:
        raise ValueError(f"DS1 version {version}: only 4 and up keep the tile layers in the order layout() reads")
    n = w * h
    grids = [struct.unpack_from(f"<{n}I", data, o + k * n * 4) for k in range(2 * walls + floors)]
    # A key held by several DT1s is taken from the first in path order; the game picks among them by rarity, so the
    # flags (and the cliff test) may come from another DT1 than the one it draws.
    order = sorted(dt1s)
    named: dict[tuple, Tile] = {}  # each distinct (orientation, main, sub) is looked up once

    def tile(orientation: int, cell: int) -> Tile:
        t = (orientation, cell >> 20 & 0x3F, cell >> 8 & 0xFF)
        if t not in named:
            named[t] = (next((p for p in order if t in dt1s[p]), None), *t)
        return named[t]
    keys, flags, ored = {}, {}, {}
    for i in range(n):
        wall = tuple(tile(grids[2 * k + 1][i] & 0xFF, grids[2 * k][i]) for k in range(walls) if grids[2 * k][i])
        floor = tuple(tile(0, g[i]) for g in grids[2 * walls:] if g[i])
        if wall or floor:
            keys[(i % w, i // w)] = ("w", wall) if wall else ("f", floor)
            if (both := wall + floor) not in ored:
                f = [dt1s[t[0]][t[1:]] for t in both if t[0]]
                ored[both] = bytes(functools.reduce(lambda u, v: [x | y for x, y in zip(u, v)], f, bytes(25)))
            flags[(i % w, i // w)] = ored[both]
    cells = [grids[2 * k] for k in range(walls)] + list(grids[2 * walls:])
    unwalkable = frozenset((i % w, i // w) for i in range(n) if any(g[i] & UNWALKABLE for g in cells))
    return Layout(w, h, keys, flags, unwalkable)
