#!/usr/bin/env python3
"""
kawalki_terenu.py - teren wzdluz torow pociety na osobne kawalki (powierzchnie edytora) do osobnego teksturowania.

Z terenu heightmapowego MaSzyny (terrain.yaml + paczki .tch, np. z irt2maszyna/teren.py) bierze modyfikatory torowiska
(osie torow z wysokoscia formacji i polszerokoscia) i robi z nich powierzchnie wyciete z terenu (patches edytora gse,
"follow": wysokosci bierze z terenu pod nimi):

  torowisko  - pas formacji wzdluz osi, polszerokosc z modyfikatora (3,52 m z teren.py: stopa podsypki 2,32 m od osi
               + lawa >= 0,55 m wg Sysaka rozdz. 11 + zapas na siatke 2 m); przy torach obok siebie dzielony w polowie
               miedzy osiami, wiec tory wielotorowe maja torowisko kazdego toru osobno, bez nakladek,
  nasyp      - skarpa od krawedzi torowiska do miejsca, gdzie teren z modyfikatorem (HGHT) schodzi sie z NMT (HBAS),
  przekop      po kazdej stronie osobno, rodzaj z tego, czy teren jest nadsypany, czy wykopany.

Kazdy kawalek ma najwyzej --dlugosc metrow (domyslnie 100) wzdluz osi; skarpy dziela sie tez tam, gdzie nasyp przechodzi
w przekop albo skarpa znika. Pod kawalkami teren dostaje dziury (HOLE w paczkach), zwezone o --zapas od brzegu, zeby
kawalki zachodzily na teren i nie bylo szczelin. Wysokosci terenu i modyfikatory zostaja bez zmian - nasypy i przekopy
robi dalej modyfikator torowiska edytora (ten sam, ktory je przelicza po zmianie torow).

Wynik: kopia folderu terenu z paczkami z dziurami, terrain.yaml z "patches_file: patches.txt" i patches.txt (linia na
kawalek: nazwa, material, powtarzanie, obrot, uniesienie, siatka, obrys).

  python kawalki_terenu.py terrain/irt terrain/irt_kawalki --tory scenery/irt_tory.scm
"""
import argparse
import math
import os
import re
import shutil
import struct
import zlib
from collections import defaultdict

import numpy as np
from scipy.spatial import cKDTree

CHUNK = 250.0
PACK_SIDE = 16
HEIGHT_STEP = 0.005
HOLE_SAMPLES = 250  # probka co metr


def fourcc(s):
    return s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24)


TAG_HGHT, TAG_HBAS, TAG_HOLE, TAG_CHNK = (fourcc(t) for t in (b"HGHT", b"HBAS", b"HOLE", b"CHNK"))


# ----------------------------------------------------------------------------------------------------------------------
# paczki .tch

class Pack:
    def __init__(self, path):
        import zstandard
        self.path = path
        data = open(path, "rb").read()
        magic, version, self.px, self.pz, self.side, self.chunk_size = struct.unpack_from("<IIiiIf", data, 0)
        assert magic == fourcc(b"TCH1"), path
        index = struct.unpack_from("<Q", data, 24)[0]
        self.slices = {}  # slot -> [(tag, codec, raw stored bytes, size, crc)]
        dctx = zstandard.ZstdDecompressor()
        self._dctx = dctx
        for slot in range(self.side * self.side):
            offset, count, _ = struct.unpack_from("<QII", data, index + slot * 16)
            if offset == 0:
                continue
            table = []
            for k in range(count):
                tag, codec, off, stored, size, crc, _ = struct.unpack_from("<IIQIIII", data, offset + k * 32)
                table.append([tag, codec, data[off:off + stored], size, crc])
            self.slices[slot] = table

    def keys(self):
        for slot in self.slices:
            yield (self.px * self.side + slot % self.side, self.pz * self.side + slot // self.side), slot

    def payload(self, entry):
        tag, codec, stored, size, crc = entry
        return self._dctx.decompress(stored, max_output_size=size) if codec == 1 else stored

    def chunk(self, slot):
        cells = None
        h = g = None
        for entry in self.slices[slot]:
            if entry[0] == TAG_CHNK:
                cells = struct.unpack_from("<H", self.payload(entry), 0)[0]
        for entry in self.slices[slot]:
            if entry[0] in (TAG_HGHT, TAG_HBAS):
                arr = decode_heights(self.payload(entry), (cells + 1) ** 2).reshape(cells + 1, cells + 1)
                if entry[0] == TAG_HGHT:
                    h = arr
                else:
                    g = arr
        return cells, h, g if g is not None else h

    def set_slice(self, slot, tag, data, cctx):
        data = bytes(data)
        crc = zlib.crc32(data) & 0xFFFFFFFF
        packed = cctx.compress(data)
        entry = [tag, 1, packed, len(data), crc] if len(packed) < len(data) else [tag, 0, data, len(data), crc]
        table = [e for e in self.slices[slot] if e[0] != tag]
        table.append(entry)
        self.slices[slot] = table

    def write(self, path):
        header = 64
        f = bytearray(struct.pack("<IIiiIf", fourcc(b"TCH1"), 1, self.px, self.pz, self.side, self.chunk_size))
        f += b"\0" * (header - len(f))
        records = {}
        for slot in sorted(self.slices):
            recs = []
            for tag, codec, stored, size, crc in self.slices[slot]:
                recs.append((tag, codec, len(f), len(stored), size, crc))
                f += stored
            records[slot] = recs
        offsets = {}
        for slot, recs in records.items():
            offsets[slot] = len(f)
            for rec in recs:
                f += struct.pack("<IIQIIII", *rec, 0)
        index = len(f)
        struct.pack_into("<Q", f, 24, index)
        for slot in range(self.side * self.side):
            f += struct.pack("<QII", offsets.get(slot, 0), len(records.get(slot, [])), 0)
        with open(path, "wb") as handle:
            handle.write(f)


def decode_heights(buf, count):
    if buf[0] == 1:
        return np.frombuffer(buf, "<f4", count, 1).astype(np.float64)
    lo = struct.unpack_from("<i", buf, 1)[0]
    diff = np.frombuffer(buf, "<u2", count, 5).astype(np.int64)
    steps = np.cumsum(diff) & 0xFFFF
    return (steps + lo) * HEIGHT_STEP


class Terrain:
    """Wysokosci terenu z paczek: HGHT (z modyfikatorami) i HBAS (sam NMT), jak chunk_data::height_in (przekatna x0,z1 - x1,z0)."""

    def __init__(self, folder):
        self.folder = folder
        self.packs = {}
        self.where = {}  # chunk key -> (pack name, slot)
        for name in os.listdir(folder):
            if name.endswith(".tch"):
                pack = Pack(os.path.join(folder, name))
                self.packs[name] = pack
                for key, slot in pack.keys():
                    self.where[key] = (name, slot)
        self.cache = {}

    def chunk(self, key):
        if key not in self.cache:
            if key not in self.where:
                self.cache[key] = None
            else:
                name, slot = self.where[key]
                self.cache[key] = self.packs[name].chunk(slot)
        return self.cache[key]

    def heights(self, X, Z):
        """(HGHT, HBAS) w punktach; NaN poza terenem."""
        X = np.asarray(X, float)
        Z = np.asarray(Z, float)
        H = np.full(X.shape, np.nan)
        G = np.full(X.shape, np.nan)
        kx = np.floor(X / CHUNK).astype(np.int64)
        kz = np.floor(Z / CHUNK).astype(np.int64)
        code = kx * 100003 + kz
        for c in np.unique(code):
            sel = code == c
            key = (int(kx[sel][0]), int(kz[sel][0]))
            data = self.chunk(key)
            if data is None:
                continue
            cells, h, g = data
            step = CHUNK / cells
            fx = (X[sel] - key[0] * CHUNK) / step
            fz = (Z[sel] - key[1] * CHUNK) / step
            ix = np.clip(np.floor(fx).astype(int), 0, cells - 1)
            iz = np.clip(np.floor(fz).astype(int), 0, cells - 1)
            tx = np.clip(fx - ix, 0, 1)
            tz = np.clip(fz - iz, 0, 1)
            for arr, out in ((h, H), (g, G)):
                h00, h10, h01, h11 = arr[iz, ix], arr[iz, ix + 1], arr[iz + 1, ix], arr[iz + 1, ix + 1]
                low = tx + tz <= 1.0
                value = np.where(low, h00 + tx * (h10 - h00) + tz * (h01 - h00), h11 + (1 - tx) * (h01 - h11) + (1 - tz) * (h10 - h11))
                out[sel] = value
        return H, G


# ----------------------------------------------------------------------------------------------------------------------
# modyfikatory i nazwy torow

def read_modifiers(path):
    mods = []
    current = None
    for line in open(path, encoding="utf-8"):
        if re.match(r"\s*- name: ", line) and current is not None and current.get("points") is not None:
            current = None
        m = re.match(r"\s*- name: (\S+)", line)
        if m and line.startswith("  - name: tor_"):
            current = dict(name=m.group(1), points=[], open_start=False, open_end=False)
            mods.append(current)
            continue
        if current is None:
            continue
        m2 = re.match(r"\s+(slope|rounding|reach): ([-\d.e]+)", line)
        if m2:
            current[m2.group(1)] = float(m2.group(2))
        if "open_start: true" in line:
            current["open_start"] = True
        if "open_end: true" in line:
            current["open_end"] = True
        m = re.match(r"\s+- \[([-\d.e]+), ([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\]", line)
        if m:
            current["points"].append(tuple(float(v) for v in m.groups()))
    return [m for m in mods if len(m["points"]) >= 2]


def scenery_lines(path, seen=None):
    """Linie pliku scenerii razem z plikami, ktore dolacza (include ... end), wzgledem jego katalogu."""
    seen = set() if seen is None else seen
    full = os.path.abspath(path)
    if full in seen or not os.path.exists(full):
        return
    seen.add(full)
    for line in open(full, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*include\s+(\S+)", line)
        if m:
            yield from scenery_lines(os.path.join(os.path.dirname(full), m.group(1)), seen)
            continue
        yield line


def track_names(path):
    """Srodki torow z pliku torow (.inc albo warstwy .scm z dolaczonymi) i ich nazwy bez numeru kawalka (L132_t1_00491 -> L132 t1)."""
    pts, names = [], []
    name = None
    numbers = []
    for line in scenery_lines(path):
        m = re.match(r"node \S+ \S+ (\S+) track (normal|switch)", line)
        if m:
            name = m.group(1)
            numbers = []
            continue
        if name is None:
            continue
        numbers += line.split()
        if len(numbers) >= 14:
            try:
                x, z = float(numbers[0]), float(numbers[2])
                pts.append((x, z))
                base = re.sub(r"_\d+$", "", name)
                base = re.sub(r"_(t\d\w?)$", r" \1", base)
                names.append(base.replace("_", " ") if base.startswith("lacz") else base)
            except ValueError:
                pass
            name = None
    return cKDTree(np.array(pts)), names


# ----------------------------------------------------------------------------------------------------------------------

def resample(points, step):
    """Os modyfikatora co ~step m: (x, z, formacja, polszerokosc), stacje i normalne w lewo."""
    P = np.array(points, float)
    seg = np.hypot(np.diff(P[:, 0]), np.diff(P[:, 1]))
    s = np.concatenate([[0.0], np.cumsum(seg)])
    total = s[-1]
    n = max(1, int(math.ceil(total / step)))
    st = np.linspace(0.0, total, n + 1)
    out = np.column_stack([np.interp(st, s, P[:, k]) for k in range(4)])
    d = np.gradient(out[:, :2], axis=0)
    d /= np.maximum(np.hypot(d[:, 0], d[:, 1]), 1e-9)[:, None]
    normal = np.column_stack([-d[:, 1], d[:, 0]])
    return out, st, normal


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("teren", help="folder terenu (terrain.yaml + .tch)")
    ap.add_argument("wynik", help="folder wynikowy (kopia terenu z kawalkami)")
    ap.add_argument("--tory", help="plik z torami (irt_tory.scm z warstwami albo stary .inc), do nazw kawalkow")
    ap.add_argument("--dlugosc", type=float, default=100.0, help="najwieksza dlugosc kawalka wzdluz osi [m]")
    ap.add_argument("--krok", type=float, default=5.0, help="krok punktow obrysu wzdluz osi [m]")
    ap.add_argument("--zasieg", type=float, default=60.0, help="najdalej od krawedzi torowiska do konca skarpy [m]")
    ap.add_argument("--prog", type=float, default=0.03, help="roznica terenu i NMT (gdy paczki maja HBAS), od ktorej jest skarpa [m]")
    ap.add_argument("--tolerancja", type=float, default=0.15, help="odchylka terenu od skarpy 1:slope modyfikatora, nadal skarpa [m]")
    ap.add_argument("--min-skarpa", type=float, default=0.5, help="najwezsza skarpa robiona osobno [m]")
    ap.add_argument("--zapas", type=float, default=1.0, help="o ile dziura w terenie jest wezsza od kawalkow [m]")
    ap.add_argument("--tylko", type=int, default=0, help="tylko tyle pierwszych modyfikatorow (do prob)")
    ap.add_argument("--material", default="", help="material kawalkow (domyslnie pierwszy z palety terenu)")
    ap.add_argument("--miejsca", type=int, default=2, help="miejsc po przecinku we wspolrzednych obrysow (1: plik patches.txt o ~1/4 mniejszy)")
    ap.add_argument("--bez-dziur", action="store_true",
                    help="bez dziur w terenie: kawalki do przemalowania w edytorze (Ficzery, Przemaluj je w teren) - bez szwow")
    args = ap.parse_args()

    manifest = os.path.join(args.teren, "terrain.yaml")
    mods = read_modifiers(manifest)
    if args.tylko:
        mods = mods[:args.tylko]
    print(f"modyfikatory torowiska: {len(mods)}")
    terrain = Terrain(args.teren)
    print(f"chunki: {len(terrain.where)} w {len(terrain.packs)} paczkach")
    names = track_names(args.tory) if args.tory else None

    # osie wszystkich modyfikatorow co metr, do granic miedzy torami obok siebie
    dense, owner, station = [], [], []
    samples = []
    for i, m in enumerate(mods):
        pts, st, nrm = resample(m["points"], 1.0)
        dense.append(pts[:, :2])
        owner.append(np.full(len(pts), i))
        station.append(st)
        samples.append(resample(m["points"], args.krok))
    dense = np.concatenate(dense)
    owner = np.concatenate(owner)
    station = np.concatenate(station)
    axis_tree = cKDTree(dense)

    def line_name(i):
        if names is None:
            return mods[i]["name"]
        tree, labels = names
        pts = np.array(mods[i]["points"])
        mid = pts[len(pts) // 2, :2]
        _, j = tree.query(mid)
        return labels[j]

    patches = []  # (nazwa, rodzaj, obrys)
    counts = defaultdict(int)
    for i, m in enumerate(mods):
        pts, st, nrm = samples[i]
        count = len(pts)
        slope = m.get("slope", 1.5)
        rounding = m.get("rounding", 1.5)
        hw = pts[:, 3]
        base_name = line_name(i)
        # granica z sasiednimi torami: przesuniecie wzdluz normalnej, od ktorego blizej do innej osi
        limit = {+1: np.full(count, np.inf), -1: np.full(count, np.inf)}
        reach = float(hw.max()) + args.zasieg
        near = axis_tree.query_ball_point(pts[:, :2], reach)
        for k in range(count):
            idx = np.array(near[k], dtype=np.int64)
            if idx.size == 0:
                continue
            other = (owner[idx] != i) | (np.abs(station[idx] - st[k]) > 3.0 * reach)
            idx = idx[other]
            if idx.size == 0:
                continue
            q = dense[idx] - pts[k, :2]
            for side in (+1, -1):
                n = nrm[k] * side
                along = q @ n
                ok = along > 1e-6
                if ok.any():
                    o = (q[ok] ** 2).sum(axis=1) / (2.0 * along[ok])
                    limit[side][k] = min(limit[side][k], float(o.min()))
        # krawedz torowiska i koniec skarpy po kazdej stronie
        edge, outer, kind = {}, {}, {}
        offsets = np.arange(0.0, args.zasieg + 0.25, 0.5)
        for side in (+1, -1):
            e = np.minimum(hw, limit[side])
            edge[side] = e
            o_end = np.array(e)
            sign = np.zeros(count)
            # probki na promieniach od krawedzi torowiska na zewnatrz. Paczki moga nie miec NMT (HBAS), wiec skarpe poznaje sie
            # po ksztalcie terenu: od krawedzi formacji opada (nasyp) albo rosnie (przekop) w pochyleniu modyfikatora 1:slope,
            # az do miejsca, gdzie spotyka teren naturalny
            X = pts[:, 0][:, None] + (hw[:, None] + offsets[None]) * nrm[:, 0][:, None] * side
            Z = pts[:, 1][:, None] + (hw[:, None] + offsets[None]) * nrm[:, 1][:, None] * side
            H, G = terrain.heights(X, Z)
            has_base = np.nanmax(np.abs(H - G)) > 1e-6 if np.isfinite(H).any() else False
            form = pts[:, 2][:, None]
            fill = np.abs(H - (form - offsets[None] / slope)) < args.tolerancja
            cut = np.abs(H - (form + offsets[None] / slope)) < args.tolerancja
            if has_base:
                fill = fill | ((H - G) > args.prog)
                cut = cut | ((G - H) > args.prog)
            room = (hw[:, None] + offsets[None]) <= limit[side][:, None]
            for k in range(count):
                if e[k] < hw[k] - 1e-6:
                    continue  # tor obok: miedzy torami nie ma skarpy
                # rodzaj z wysokosci 1-3 m od krawedzi
                probe = slice(2, 7)
                rise = np.nanmean(H[k, probe] - form[k, 0])
                if not np.isfinite(rise) or abs(rise) < 0.1:
                    continue  # teren na poziomie formacji: bez skarpy
                match = (fill[k] if rise < 0 else cut[k]) & room[k]
                miss = np.flatnonzero(~match[1:])
                stop = 1 + (miss[0] if miss.size else len(match) - 1)
                end = hw[k] + min(offsets[min(stop, len(offsets) - 1)] + rounding * 0.5, offsets[-1])
                if end - hw[k] >= args.min_skarpa:
                    o_end[k] = min(end, limit[side][k])
                    sign[k] = 1.0 if rise < 0 else -1.0
            # wygladzenie wzdluz osi: przerwy do 3 probek miedzy ta sama skarpa wypelnione, skarpy krotsze niz 3 probki pominiete,
            # zasieg skarpy mediana z 5 probek - pomiar ze siatki 2 m bywa poszarpany
            sign = sign.copy()
            k = 0
            while k < count:
                if sign[k] != 0:
                    k += 1
                    continue
                j = k
                while j < count and sign[j] == 0:
                    j += 1
                if 0 < k and j < count and j - k <= 3 and sign[k - 1] == sign[j] and np.all(e[k:j] >= hw[k:j] - 1e-6):
                    sign[k:j] = sign[k - 1]
                    o_end[k:j] = np.minimum(np.interp(np.arange(k, j), [k - 1, j], [o_end[k - 1], o_end[j]]), limit[side][k:j])
                k = j
            k = 0
            while k < count:
                if sign[k] == 0:
                    k += 1
                    continue
                j = k
                while j < count and sign[j] == sign[k]:
                    j += 1
                if j - k < 3:
                    sign[k:j] = 0
                    o_end[k:j] = e[k:j]
                k = j
            padded = np.pad(o_end, 2, mode="edge")
            median = np.median(np.lib.stride_tricks.sliding_window_view(padded, 5), axis=1)
            o_end = np.where(sign != 0, np.maximum(np.minimum(median, limit[side]), hw + args.min_skarpa), o_end)
            outer[side] = o_end
            kind[side] = sign

        def point(k, side, offset):
            return (pts[k, 0] + nrm[k, 0] * side * offset, pts[k, 1] + nrm[k, 1] * side * offset)

        def km(a, b):
            return f"{st[a] / 1000.0:07.3f}-{st[b] / 1000.0:07.3f}"

        # torowisko: kawalki co --dlugosc
        pieces = max(1, int(math.ceil(st[-1] / args.dlugosc - 1e-9)))
        bounds = [0]
        for p in range(1, pieces):
            bounds.append(int(np.argmin(np.abs(st - p * args.dlugosc))))
        bounds.append(count - 1)
        bounds = sorted(set(bounds))
        for a, b in zip(bounds, bounds[1:]):
            left = [point(k, +1, edge[+1][k]) for k in range(a, b + 1)]
            right = [point(k, -1, edge[-1][k]) for k in range(b, a - 1, -1)]
            patches.append((f"{base_name} torowisko km {km(a, b)}", "torowisko", left + right))
            counts["torowisko"] += 1
        # skarpy: ciagi probek z ta sama skarpa po stronie, dzielone co --dlugosc (sasiednie kawalki maja wspolny punkt)
        for side, label in ((+1, "L"), (-1, "P")):
            k = 0
            while k < count:
                if kind[side][k] == 0:
                    k += 1
                    continue
                j = k
                while j + 1 < count and kind[side][j + 1] == kind[side][k]:
                    j += 1
                rodzaj = "nasyp" if kind[side][k] > 0 else "przekop"
                start = k
                while start < j:
                    end = start
                    while end + 1 <= j and st[end + 1] - st[start] <= args.dlugosc + 1e-6:
                        end += 1
                    end = max(end, start + 1)
                    inner = [point(n, side, edge[side][n]) for n in range(start, end + 1)]
                    out = [point(n, side, outer[side][n]) for n in range(end, start - 1, -1)]
                    poly = inner + out if side > 0 else list(reversed(inner + out))
                    patches.append((f"{base_name} {rodzaj} {label} km {km(start, end)}", rodzaj, poly))
                    counts[rodzaj] += 1
                    start = end
                k = j + 1
        if (i + 1) % 50 == 0:
            print(f"  {i + 1}/{len(mods)} modyfikatorow, kawalkow {len(patches)}")
    print("kawalki: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))

    # dziury pod kawalkami: probki co metr wewnatrz kawalkow, dalej niz --zapas od ich brzegu
    from matplotlib.path import Path as MplPath
    holes = defaultdict(lambda: np.zeros((HOLE_SAMPLES + 1, HOLE_SAMPLES + 1), bool))
    for name, rodzaj, poly in ([] if args.bez_dziur else patches):
        P = np.array(poly)
        if len(P) < 3:
            continue
        path = MplPath(P)
        lo = np.floor(P.min(axis=0)).astype(int)
        hi = np.ceil(P.max(axis=0)).astype(int)
        xs = np.arange(lo[0], hi[0] + 1)
        zs = np.arange(lo[1], hi[1] + 1)
        XX, ZZ = np.meshgrid(xs, zs)
        pts2 = np.column_stack([XX.ravel(), ZZ.ravel()]).astype(float)
        inside = path.contains_points(pts2, radius=-2.0 * args.zapas) & path.contains_points(pts2, radius=2.0 * args.zapas)
        # oba znaki promienia: matplotlib zmienia jego znaczenie z kierunkiem obrysu - liczy sie wezszy wynik
        if not inside.any():
            continue
        q = pts2[inside]
        kx = np.floor(q[:, 0] / CHUNK).astype(int)
        kz = np.floor(q[:, 1] / CHUNK).astype(int)
        for key in set(zip(kx.tolist(), kz.tolist())):
            if key not in terrain.where:
                continue
            sel = (kx == key[0]) & (kz == key[1])
            ix = (q[sel, 0] - key[0] * CHUNK).astype(int)
            iz = (q[sel, 1] - key[1] * CHUNK).astype(int)
            holes[key][iz, ix] = True
            # probki na brzegu chunku sa tez ostatnimi probkami sasiada
            for dx, dz, cond in ((-1, 0, ix == 0), (0, -1, iz == 0), (-1, -1, (ix == 0) & (iz == 0))):
                if cond.any() and (key[0] + dx, key[1] + dz) in terrain.where:
                    jx = np.where(dx < 0, HOLE_SAMPLES, ix[cond])
                    jz = np.where(dz < 0, HOLE_SAMPLES, iz[cond])
                    holes[(key[0] + dx, key[1] + dz)][jz, jx] = True
    print(f"chunki z dziurami: {len(holes)}")

    # zapis
    os.makedirs(args.wynik, exist_ok=True)
    import zstandard
    cctx = zstandard.ZstdCompressor(level=19)
    touched = defaultdict(list)
    for key, mask in holes.items():
        name, slot = terrain.where[key]
        touched[name].append((slot, mask))
    for name, pack in terrain.packs.items():
        for slot, mask in touched.get(name, []):
            data = struct.pack("<H", HOLE_SAMPLES) + np.where(mask, 255, 0).astype(np.uint8).tobytes()
            pack.set_slice(slot, TAG_HOLE, data, cctx)
        pack.write(os.path.join(args.wynik, name))
    for extra in os.listdir(args.teren):
        if not extra.endswith(".tch") and extra not in ("terrain.yaml", "patches.txt"):
            src = os.path.join(args.teren, extra)
            if os.path.isfile(src):
                shutil.copy2(src, os.path.join(args.wynik, extra))
    # pierwszy material palety
    material = args.material
    text = open(manifest, encoding="utf-8").read()
    if not material:
        m = re.search(r"^layers:\s*\n\s*-\s*(?:material:\s*)?(\S+)", text, re.M)
        material = m.group(1) if m else "grass"
    lifts = {"torowisko": 0.03, "nasyp": 0.02, "przekop": 0.02}
    with open(os.path.join(args.wynik, "patches.txt"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# powierzchnie wyciete z terenu wzdluz torow (tools/irt/kawalki_terenu.py): nazwa, material, powtarzanie, obrot, "
                 "uniesienie, siatka, obrys x z ...\n")
        for name, rodzaj, poly in patches:
            pts_text = " ".join(f"{x:.{args.miejsca}f} {z:.{args.miejsca}f}" for x, z in poly)
            fh.write(f"{name}\t{material}\t0\t0\t{lifts[rodzaj]:g}\t2\t{pts_text}\n")
    text = re.sub(r"^patches_file:.*\n", "", text, flags=re.M)
    lines = text.split("\n")
    at = next((n for n, l in enumerate(lines) if l.startswith("modifiers:")), len(lines))
    lines.insert(at, "patches_file: patches.txt")
    with open(os.path.join(args.wynik, "terrain.yaml"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))
    print(f"zapisano {len(patches)} kawalkow do {args.wynik}")


if __name__ == "__main__":
    main()
