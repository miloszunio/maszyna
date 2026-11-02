#!/usr/bin/env python3
"""Odczyt paczek .tch tak jak heightmap::pack_file::read (master): naglowek, indeks, CRC, wysokosci."""
import glob
import os
import struct
import sys
import zlib

import numpy as np


def fourcc(s):
    return s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24)


TAGS = {fourcc(k.encode()): k for k in ("CHNK", "HGHT", "HBAS", "SPLT", "GRAS", "HADJ", "HOLE")}


def read_pack(path):
    import zstandard
    d = open(path, "rb").read()
    magic, ver, kx, kz, side, size, index = struct.unpack_from("<IIiiIfQ", d, 0)
    assert magic == fourcc(b"TCH1") and ver == 1 and side == 16 and abs(size - 250) < 1e-6
    out = {}
    for i in range(side * side):
        toff, cnt, _ = struct.unpack_from("<QII", d, index + 16 * i)
        if not toff:
            continue
        slices = {}
        for k in range(cnt):
            tag, codec, off, stored, sz, crc, _ = struct.unpack_from("<IIQIIII", d, toff + 32 * k)
            raw = d[off:off + stored]
            data = zstandard.ZstdDecompressor().decompress(raw, max_output_size=sz) if codec == 1 else raw
            assert len(data) == sz and (zlib.crc32(data) & 0xFFFFFFFF) == crc, ("crc", path, i, TAGS.get(tag))
            slices[TAGS.get(tag, tag)] = data
        cells, paint = struct.unpack_from("<HH", slices["CHNK"], 0)
        n = (cells + 1) ** 2
        h = decode(slices["HGHT"], n)
        out[(kx * side + i % side, kz * side + i // side)] = dict(cells=cells, h=h, slices=list(slices))
    return out


def decode(data, n):
    if data[0] == 1:
        return np.frombuffer(data, "<f4", n, 1).astype(float)
    base = struct.unpack_from("<i", data, 1)[0]
    diff = np.frombuffer(data, "<u2", n, 5).astype(np.int64)
    steps = np.cumsum(diff) & 0xFFFF
    return (steps + base) * 0.005


if __name__ == "__main__":
    folder = sys.argv[1]
    tot = 0
    lo, hi = 1e9, -1e9
    kinds = {}
    for p in sorted(glob.glob(os.path.join(folder, "*.tch"))):
        ch = read_pack(p)
        tot += len(ch)
        for c in ch.values():
            lo, hi = min(lo, c["h"].min()), max(hi, c["h"].max())
            kinds[c["cells"]] = kinds.get(c["cells"], 0) + 1
    print(f"chunkow {tot}, siatki {kinds}, wysokosci {lo:.1f}..{hi:.1f} m")
