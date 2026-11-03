"""
teren.py - teren heightmapowy MaSzyny (folder terrain/<nazwa>/: terrain.yaml + paczki .tch) z NMT GUGiK
i torow po korekcji. Format jak scene/heightmapterrain.{h,cpp} (MaSzyna-EU07/maszyna, master; trawa 3D
'GRAS' jak w galezi Hirka feat/gl46-migration - starsze wersje ja pomijaja).

* NMT 10 m w pasie wokol torow (plik z przegladarki), lagodnie wygladzony (gauss) i probkowany
  splajnem 3. stopnia w siatce chunku (2 m przy torach, 5 m dalej),
* nasypy i przekopy pod torami: modyfikator jak modifier_shape::evaluate z edytora - torowisko
  (formation = glowka szyny - 0,2 m podsypki) na polszerokosci 2,32 m (profile_ballast: 0,5*1,435 + 0,5 + 1,1),
  skarpy 1:1,5, zasieg 60 m, zaokraglenie 1,5 m; na mostach i w tunelach teren zostaje naturalny,
* HGHT = teren z modyfikatorami, HBAS = sam NMT, modyfikatory zapisane w terrain.yaml (edytor przeliczy je sam),
* warstwa materialu: 'grass' (domyslna trawa z paczki MaSzyny, ta sama co pierwsza warstwa palety edytora),
* trawa 3D (galaz Hirka): 'meadow' wszedzie, 'weeds' przy torowisku, brak trawy na podsypce.
"""
import math
import os
import struct
import zlib

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

CHUNK = 250.0
PACK_SIDE = 16
HEIGHT_STEP = 0.005
NO_LAYER = 0xFFFF


def fourcc(s):
    return s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24)


TAG = {k: fourcc(k.encode()) for k in ("CHNK", "HGHT", "HBAS", "SPLT", "GRAS")}
MAGIC = fourcc(b"TCH1")


# ----------------------------------------------------------------------------
# NMT z przegladarki: 'NMTG' (deflate): uint32 n, potem n x (int32 kx, int32 ky, 2500 x int16 dm, -32768 = brak)
# kafle 500 m, komorki 10 m, wiersz 0 = polnoc
# ----------------------------------------------------------------------------
class Grid:
    def __init__(self, path):
        data = open(path, "rb").read()
        if data[:4] != b"NMTG":
            data = zlib.decompress(data)
        assert data[:4] == b"NMTG", "zly plik terenu"
        n = struct.unpack_from("<I", data, 4)[0]
        off = 8
        self.tiles = {}
        for _ in range(n):
            kx, ky = struct.unpack_from("<ii", data, off)
            off += 8
            h = np.frombuffer(data, "<i2", 2500, off).astype(np.float32).reshape(50, 50)
            off += 5000
            h[h == -32768] = np.nan
            h[h < 10] = np.nan        # 0 = brak danych NMT (za granica)
            self.tiles[(kx, ky)] = h / 10.0

    def window(self, Emin, Emax, Nmin, Nmax, pad=40.0):
        """Mozaika 10 m: M[r, c], komorka (r, c) ma srodek E = 5 + 10 (c0 + c), N = 5 + 10 (r0 + r)."""
        c0 = int(math.floor((Emin - pad - 5) / 10.0))
        c1 = int(math.ceil((Emax + pad - 5) / 10.0))
        r0 = int(math.floor((Nmin - pad - 5) / 10.0))
        r1 = int(math.ceil((Nmax + pad - 5) / 10.0))
        M = np.full((r1 - r0 + 1, c1 - c0 + 1), np.nan, np.float32)
        for kx in range(c0 // 50, c1 // 50 + 1):
            for ky in range(r0 // 50, r1 // 50 + 1):
                t = self.tiles.get((kx, ky))
                if t is None:
                    continue
                # wiersz r kafla -> globalny wiersz ky*50 + (49 - r)
                tile_rows = t[::-1, :]
                gr0, gc0 = ky * 50, kx * 50
                a0, a1 = max(r0, gr0), min(r1, gr0 + 49)
                b0, b1 = max(c0, gc0), min(c1, gc0 + 49)
                if a0 > a1 or b0 > b1:
                    continue
                M[a0 - r0:a1 - r0 + 1, b0 - c0:b1 - c0 + 1] = tile_rows[a0 - gr0:a1 - gr0 + 1, b0 - gc0:b1 - gc0 + 1]
        return M, r0, c0


_M64 = (1 << 64) - 1


def _hash(ix, iz):
    """jak hash w heightmap::natural_noise (scene/heightmapterrain.cpp), na uint64 z zawijaniem"""
    with np.errstate(over="ignore"):
        h = ix.astype(np.int64).view(np.uint64) * np.uint64(0x9E3779B97F4A7C15) ^ iz.astype(np.int64).view(np.uint64) * np.uint64(0xC2B2AE3D27D4EB4F)
        h ^= h >> np.uint64(31)
        h *= np.uint64(0xBF58476D1CE4E5B9)
        h ^= h >> np.uint64(29)
    return (h & np.uint64(0xFFFFFF)).astype(np.float64) / float(0xFFFFFF) * 2.0 - 1.0


def _octave(u, v):
    iu, iv = np.floor(u), np.floor(v)
    fu, fv = u - iu, v - iv
    su, sv = fu * fu * (3.0 - 2.0 * fu), fv * fv * (3.0 - 2.0 * fv)
    x, z = iu.astype(np.int64), iv.astype(np.int64)
    a = _hash(x, z) + (_hash(x + 1, z) - _hash(x, z)) * su
    b = _hash(x, z + 1) + (_hash(x + 1, z + 1) - _hash(x, z + 1)) * su
    return a + (b - a) * sv


def natural_noise(X, Z, scale, channel):
    """szum -1..1 w falach ~scale m, ten sam co heightmap::natural_noise w silniku (co do bitu)"""
    scale = max(0.5, scale)
    u = np.asarray(X, float) / scale + channel * 101.7
    v = np.asarray(Z, float) / scale - channel * 57.3
    return np.clip(0.7 * _octave(u, v) + 0.3 * _octave(u * 2.7 + 17.3, v * 2.7 - 9.1), -1.0, 1.0)


# skarpy nie od linijki (modifier::natural_shape): stopa faluje, skarpa miejscami lagodniejsza, miekka stopa, nierowna powierzchnia
NATURAL = dict(wander=0.8, scale=15.0, gentler=0.15, soften=0.3, roughness=0.08)


def soft_max(a, b, k):
    k = np.asarray(k, float)
    h = np.maximum(k - np.abs(a - b), 0.0) / np.maximum(k, 1e-9)
    return np.where(k <= 1e-6, np.maximum(a, b), np.maximum(a, b) + h * h * k * 0.25)


# ----------------------------------------------------------------------------
# odcinki torowiska (wspolrzedne scenerii x, z) z formacja i polszerokoscia
# ----------------------------------------------------------------------------
class Formation:
    # profile_ballast() z edytora: stopa podsypki 0,5*1,435 + 0,5 + 1,1 = 2,32 m od osi, spod podsypki 0,2 m pod Y toru.
    # Siatka chunku ma 2 m, a interpolacja miedzy wezlami podnosilaby teren na skraju podsypki - stad zapas:
    # torowisko szersze o 1,2 m, zeby teren nie przebijal podsypki przy jej stopie.
    HALF = 0.5 * 1.435 + 0.5 + 1.1 + 1.2
    DEPTH = 0.2

    def __init__(self, runs, origin, slope=1.5, reach=60.0, rounding=1.5, step=10.0, natural=None):
        self.slope, self.reach, self.rounding = slope, reach, rounding
        self.natural = natural
        segs = []          # ax, az, bx, bz, fa, fb, open_start, open_end
        mods = []          # do terrain.yaml
        for run in runs:
            cur = None
            next_open = False
            for pc in run:
                if pc.get("env") in ("bridge", "tunnel"):
                    if cur and len(cur["pts"]) >= 2:
                        cur["open_end"] = True
                        mods.append(cur)
                    cur = None
                    next_open = True
                    continue
                n = max(1, int(math.ceil(pc["L"] / step)))
                for i in range(n + 1):
                    t = i / n
                    P = pc["p0"] + (pc["p1"] - pc["p0"]) * t if pc["kind"] == "P" else bezier(pc, t)
                    y = pc["y0"] + (pc["y1"] - pc["y0"]) * t
                    x, z = -(P[0] - origin[0]), P[1] - origin[1]
                    if cur is None:
                        cur = dict(pts=[], open_start=next_open, open_end=False)
                        next_open = False
                    if cur["pts"] and np.hypot(cur["pts"][-1][0] - x, cur["pts"][-1][1] - z) < 0.5:
                        continue
                    cur["pts"].append((x, z, y - self.DEPTH, self.HALF))
            if cur and len(cur["pts"]) >= 2:
                mods.append(cur)
        self.mods = mods
        for m in mods:
            P = m["pts"]
            for i in range(len(P) - 1):
                segs.append((P[i][0], P[i][1], P[i + 1][0], P[i + 1][1], P[i][2], P[i + 1][2],
                             m["open_start"] and i == 0, m["open_end"] and i == len(P) - 2))
        self.S = np.array(segs, float) if segs else np.zeros((0, 8))
        mid = 0.5 * (self.S[:, 0:2] + self.S[:, 2:4])
        self.tree = cKDTree(mid) if len(mid) else None
        self.seglen = np.hypot(self.S[:, 2] - self.S[:, 0], self.S[:, 3] - self.S[:, 1]) if len(self.S) else np.zeros(0)

    def apply(self, X, Z, G):
        """Teren G w punktach (X, Z) po nasypach i przekopach; zwraca (H, odleglosc od osi)."""
        H = G.copy()
        D = np.full(G.shape, np.inf)
        if self.tree is None:
            return H, D
        reach = self.HALF + self.reach
        cx, cz = float(X.mean()), float(Z.mean())
        rad = float(np.hypot(X.max() - X.min(), Z.max() - Z.min())) / 2 + reach + float(self.seglen.max(initial=0)) / 2 + 1
        idx = self.tree.query_ball_point((cx, cz), rad)
        if not idx:
            return H, D
        S = self.S[idx]
        x, z = X.ravel(), Z.ravel()
        best = np.full(x.shape, reach)
        form = np.zeros(x.shape)
        found = np.zeros(x.shape, bool)
        for k in range(0, len(S), 128):
            s = S[k:k + 128]
            ax, az, bx, bz = s[:, 0][None], s[:, 1][None], s[:, 2][None], s[:, 3][None]
            rx, rz = bx - ax, bz - az
            L2 = rx * rx + rz * rz
            along = ((x[:, None] - ax) * rx + (z[:, None] - az) * rz) / np.maximum(L2, 1e-12)
            bad = (s[:, 6][None] > 0) & (along < 0) | (s[:, 7][None] > 0) & (along > 1)
            t = np.clip(along, 0, 1)
            off = np.hypot(x[:, None] - (ax + rx * t), z[:, None] - (az + rz * t))
            off = np.where(bad, np.inf, off)
            j = np.argmin(off, axis=1)
            o = off[np.arange(len(x)), j]
            better = o < best
            if better.any():
                tj = t[np.arange(len(x)), j]
                f = s[j, 4] + (s[j, 5] - s[j, 4]) * tj
                best = np.where(better, o, best)
                form = np.where(better, f, form)
                found |= better
        g = G.ravel()
        hw = self.HALF
        beyond = soft_max(0.0, best - hw, self.rounding)
        bend = self.rounding / self.slope * np.clip(beyond / max(self.rounding, 1e-6), 0, 1)
        slope = np.full(g.shape, self.slope)
        nat = self.natural
        fade = np.clip(beyond / 3.0, 0, 1) if nat else np.zeros(g.shape)
        if nat:
            smooth = fade * fade * (3.0 - 2.0 * fade)
            beyond = np.maximum(0.0, beyond - nat["wander"] * natural_noise(x, z, nat["scale"], 1) * smooth)
            slope = slope * (1.0 + nat["gentler"] * (0.5 + 0.5 * natural_noise(x, z, nat["scale"] * 0.7, 2)) * smooth)
            bend = bend + nat["soften"] * np.minimum(np.abs(g - form), 6.0) * smooth
        target = np.where(g > form, -soft_max(-g, -(form + beyond / slope), bend),
                          soft_max(g, form - beyond / slope, bend))
        if nat and nat["roughness"] > 0:
            face = np.clip(np.abs(target - g) / 0.5, 0, 1)
            target = target + nat["roughness"] * natural_noise(x, z, 3.0, 3) * face * fade
        h = np.where(found, target, g)
        return h.reshape(G.shape), np.where(found, best, np.inf).reshape(G.shape)


def bezier(pc, t):
    L = pc["L"]
    dh = (pc["h1"] - pc["h0"] + math.pi) % (2 * math.pi) - math.pi
    hl = L / 3.0 if abs(dh) < 1e-9 else L * (4.0 / 3.0) * math.tan(abs(dh) / 4.0) / abs(dh)
    B0, B3 = np.asarray(pc["p0"], float), np.asarray(pc["p1"], float)
    B1 = B0 + np.array([math.cos(pc["h0"]), math.sin(pc["h0"])]) * hl
    B2 = B3 - np.array([math.cos(pc["h1"]), math.sin(pc["h1"])]) * hl
    return (1 - t) ** 3 * B0 + 3 * (1 - t) ** 2 * t * B1 + 3 * (1 - t) * t * t * B2 + t ** 3 * B3


# ----------------------------------------------------------------------------
# zapis
# ----------------------------------------------------------------------------
def encode_heights(h):
    h = np.asarray(h, np.float64).ravel()
    lo = math.floor(h.min() / HEIGHT_STEP)
    hi = math.ceil(h.max() / HEIGHT_STEP)
    if hi - lo > 0xFFFF:
        return struct.pack("<B", 1) + h.astype("<f4").tobytes()
    steps = np.clip(np.rint(h / HEIGHT_STEP) - lo, 0, 0xFFFF).astype(np.int64)
    diff = (np.diff(np.concatenate([[0], steps])) & 0xFFFF).astype("<u2")
    return struct.pack("<Bi", 0, int(lo)) + diff.tobytes()


def write_pack(path, key, chunks, zstd_level=19):
    import zstandard
    cctx = zstandard.ZstdCompressor(level=zstd_level)
    tables = {}
    payload = bytearray()
    HEADER = 64

    def add(table, tag, data):
        data = bytes(data)
        crc = zlib.crc32(data) & 0xFFFFFFFF
        codec, stored = 0, data
        if len(data) > 64:
            packed = cctx.compress(data)
            if len(packed) < len(data):
                codec, stored = 1, packed
        table.append((TAG[tag], codec, HEADER + len(payload), len(stored), len(data), crc))
        payload.extend(stored)

    for (cx, cz), ch in chunks.items():
        t = []
        layers = [0] + [NO_LAYER] * 7
        add(t, "CHNK", struct.pack("<HH8H", ch["cells"], 0, *layers))
        add(t, "HGHT", encode_heights(ch["heights"]))
        if ch.get("base") is not None:
            add(t, "HBAS", encode_heights(ch["base"]))
        if ch.get("grass") is not None:
            samples, kinds, dens = ch["grass"]
            kk = list(kinds) + [NO_LAYER] * (4 - len(kinds))
            add(t, "GRAS", struct.pack("<H4H", samples, *kk) + dens.astype(np.uint8).tobytes())
        tables[(cz - key[1] * PACK_SIDE) * PACK_SIDE + (cx - key[0] * PACK_SIDE)] = t

    f = bytearray(struct.pack("<IIiiIf", MAGIC, 1, key[0], key[1], PACK_SIDE, CHUNK))
    index_field = len(f)
    f += b"\0" * 8
    f += b"\0" * (HEADER - len(f))
    f += payload
    offsets = [0] * (PACK_SIDE * PACK_SIDE)
    for i in range(PACK_SIDE * PACK_SIDE):
        if i not in tables:
            continue
        offsets[i] = len(f)
        for rec in tables[i]:
            f += struct.pack("<IIQIIII", rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], 0)
    struct.pack_into("<Q", f, index_field, len(f))
    for i in range(PACK_SIDE * PACK_SIDE):
        f += struct.pack("<QII", offsets[i], len(tables.get(i, [])), 0)
    with open(path, "wb") as fh:
        fh.write(f)


def write_manifest(path, spacing, modifiers, grass_kinds, origin=None, natural=None):
    out = ["# MaSzyna heightmap terrain; the chunks are in the .tch files next to this one",
           "# teren z NMT GUGiK (pas wokol torow IRT), nasypy i przekopy wg torowiska - irt2maszyna/teren.py",
           "version: 1", "compress: true", f"spacing: {spacing:g}", "water_material: water",
           "layers:", "  - grass"]
    if origin is not None:
        # gdzie lezy punkt 0,0,0 scenerii (EPSG:2180): edytor bierze z terenu srodek dla ortofotomapy, NMT i map
        out += ["origin:", "  system: PUWG1992", f"  east: {origin[0]:.3f}", f"  north: {origin[1]:.3f}"]
    if grass_kinds:
        out.append("grass:")
        for g in grass_kinds:
            out.append(f"  - name: {g['name']}")
            for k in ("base", "tip", "dry"):
                out.append(f"    {k}: [{', '.join(f'{v:g}' for v in g[k])}]")
            for k in ("height", "variation", "width", "density", "bend", "seasonal"):
                out.append(f"    {k}: {g[k]:g}")
    if modifiers:
        out.append("modifiers:")
        for i, m in enumerate(modifiers):
            out.append(f"  - name: tor_{i:05d}")
            out.append("    slope: 1.5")
            out.append("    reach: 60")
            out.append("    rounding: 1.5")
            if natural:
                out.append("    natural: {" + ", ".join(f"{k}: {natural[k]:g}" for k in ("wander", "scale", "gentler", "soften", "roughness")) + "}")
            if m.get("open_start"):
                out.append("    open_start: true")
            if m.get("open_end"):
                out.append("    open_end: true")
            out.append("    points:")
            for x, z, f, hw in m["pts"]:
                out.append(f"      - [{x:.3f}, {z:.3f}, {f:.3f}, {hw:.4f}]")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(out) + "\n")


GRASS = [
    # jak grass_presets() w galezi Hirka: meadow i weeds
    dict(name="meadow", base=(0.22, 0.33, 0.09), tip=(0.58, 0.68, 0.27), height=0.25, variation=0.45, width=0.02,
         density=90, bend=0.3, dry=(0.74, 0.66, 0.40), seasonal=1),
    dict(name="weeds", base=(0.16, 0.25, 0.08), tip=(0.45, 0.55, 0.22), height=0.16, variation=0.6, width=0.03,
         density=30, bend=0.25, dry=(0.52, 0.44, 0.28), seasonal=1),
]


def build(grid, runs, origin, out_dir, name="irt", corridor=230.0, fine_dist=40.0, log=print, with_base=True,
          with_grass=True, natural=None):
    """runs: listy kawalkow torow (z y0/y1/env) - tory 1, 2, laczniki; rozjazdy jako proste kawalki."""
    os.makedirs(out_dir, exist_ok=True)
    form = Formation(runs, origin, natural=natural)
    # osie do odleglosci (trawa, wybor rozdzielczosci)
    axis = []
    for run in runs:
        for pc in run:
            n = max(1, int(math.ceil(pc["L"] / 4.0)))
            for i in range(n + 1):
                P = pc["p0"] + (pc["p1"] - pc["p0"]) * (i / n) if pc["kind"] == "P" else bezier(pc, i / n)
                axis.append((-(P[0] - origin[0]), P[1] - origin[1]))
    axis = np.array(axis)
    atree = cKDTree(axis)
    # chunki w pasie: kazdy chunk, w ktorym jest teren NMT
    keys = set()
    for kx, ky in grid.tiles:
        for e in np.arange(kx * 500, kx * 500 + 500 + 1, 125):
            for n in np.arange(ky * 500, ky * 500 + 500 + 1, 125):
                x, z = -(e - origin[0]), n - origin[1]
                keys.add((int(math.floor(x / CHUNK)), int(math.floor(z / CHUNK))))
    chunks_by_pack = {}
    stats = dict(chunks=0, fine=0, skipped=0)
    for ck in sorted(keys):
        x0, z0 = ck[0] * CHUNK, ck[1] * CHUNK
        Emax, Emin = origin[0] - x0, origin[0] - x0 - CHUNK
        Nmin, Nmax = origin[1] + z0, origin[1] + z0 + CHUNK
        M, r0, c0 = grid.window(Emin, Emax, Nmin, Nmax)
        valid = np.isfinite(M)
        # udzial terenu w samym chunku
        inner = valid[int((Nmin - 5) / 10) - r0:int((Nmax - 5) / 10) - r0 + 1, int((Emin - 5) / 10) - c0:int((Emax - 5) / 10) - c0 + 1]
        if inner.size == 0 or inner.mean() < 0.15:
            stats["skipped"] += 1
            continue
        # chunk przy torze: 2 m, dalej 5 m
        d_c, _ = atree.query((x0 + CHUNK / 2, z0 + CHUNK / 2))
        spacing = 2.0 if d_c < CHUNK * 0.7071 + fine_dist else 5.0
        cells = int(round(CHUNK / spacing))
        # braki (poza pasem) wypelnione najblizszym terenem, lagodne wygladzenie
        if not valid.all():
            _, (ri, ci) = ndimage.distance_transform_edt(~valid, return_indices=True)
            M = M[ri, ci]
        M = ndimage.gaussian_filter(M, 0.8, mode="nearest")
        ix = np.arange(cells + 1)
        X, Z = np.meshgrid(x0 + ix * spacing, z0 + ix * spacing)     # wiersze rosnacego z
        E, N = origin[0] - X, origin[1] + Z
        rr = (N - 5.0) / 10.0 - r0
        cc = (E - 5.0) / 10.0 - c0
        G = ndimage.map_coordinates(M, [rr.ravel(), cc.ravel()], order=3, mode="nearest").reshape(X.shape)
        H, Dax = form.apply(X, Z, G)
        ch = dict(cells=cells, heights=H.astype(np.float32), base=G.astype(np.float32) if with_base else None)
        if with_grass:
            gs = 250
            g5 = np.arange(0, 251, 5)
            Xg, Zg = np.meshgrid(x0 + g5, z0 + g5)
            d5, _ = atree.query(np.column_stack([Xg.ravel(), Zg.ravel()]), distance_upper_bound=60.0)
            d5 = d5.reshape(Xg.shape)
            d = ndimage.zoom(np.minimum(d5, 60.0), 251 / 51, order=1)[:251, :251]
            meadow = np.clip((d - 5.0) / 6.0, 0, 1) * 255
            weeds = np.clip((d - 3.2) / 1.5, 0, 1) * 255 - meadow
            weeds = np.clip(weeds, 0, 255)
            dens = np.concatenate([np.rint(meadow).astype(np.uint8).ravel(), np.rint(weeds).astype(np.uint8).ravel()])
            ch["grass"] = (gs, [0, 1], dens)
        pk = (ck[0] // PACK_SIDE, ck[1] // PACK_SIDE)
        chunks_by_pack.setdefault(pk, {})[ck] = ch
        stats["chunks"] += 1
        stats["fine"] += spacing == 2.0
        if stats["chunks"] % 1000 == 0:
            log(f"teren: {stats['chunks']} chunkow")
    for pk, chunks in chunks_by_pack.items():
        write_pack(os.path.join(out_dir, f"{pk[0]}_{pk[1]}.tch"), pk, chunks)
    write_manifest(os.path.join(out_dir, "terrain.yaml"), 5.0, form.mods, GRASS if with_grass else None, origin=origin, natural=natural)
    stats["packs"] = len(chunks_by_pack)
    stats["modifiers"] = len(form.mods)
    return stats
