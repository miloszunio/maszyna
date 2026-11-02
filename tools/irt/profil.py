"""
profil.py - profil podluzny (niweleta) torow z NMT GUGiK, wg ST-T1-A6 rozdz. 10.

Zrodlo wysokosci jak w edytorze MaSzyny (editor/editorNmt.cpp): usluga WCS
  https://mapy.geoportal.gov.pl/wss/service/PZGIK/NMT/GRID1/WCS/DigitalTerrainModelFormatTIFF
kwadraty 500 m siatki EPSG:2180, tu z rozdzielczoscia --nmt-res (domyslnie 2 m), trzymane na dysku.
Zamiast uslugi mozna podac katalog z arkuszami NMT (.asc ESRI ASCII albo .tif z GeoTIFF) w EPSG:2180.

Niweleta (uproszczona wersja profile::fit_ground z edytora):
  * teren probkowany wzdluz osi toru 1 co 10 m,
  * odrzucane probki odstajace od linii o wiecej niz --bridge-tol (mosty, wiadukty, tunele, sciany wykopow),
  * zalamania niwelety (PVI) z upraszczania Douglasa-Peuckera z tolerancja --grade-tol,
  * pochylenia <= tab. 10.1 (P1: 17,5 promila; wiecej -> ostrzezenie), najmniejszy odcinek staly 0,4*Vmax, min 20 m (tab. 10.1),
  * luki pionowe tam, gdzie zmiana pochylenia > 2 promile (10.2 pkt 2), R >= max(0,35 V^2; 5000 m) (tab. 10.2 P1),
    a gdy sie nie mieszcza - do P2 max(0,25 V^2; 2000 m),
  * konce linii laczonych w wezlach dostaja wysokosc linii, do ktorej sa dowiazane.
"""
import io
import math
import os
import threading
import urllib.request
from concurrent.futures import ThreadPoolExecutor

import numpy as np

SERVICE = "https://mapy.geoportal.gov.pl/wss/service/PZGIK/NMT/GRID1/WCS/DigitalTerrainModelFormatTIFF"
COVERAGE = "DTM_PL-KRON86-NH_TIFF"
SQUARE = 500.0


class NMT:
    def __init__(self, cache_dir, res=2.0, folder=None):
        self.cache = cache_dir
        self.res = res
        os.makedirs(cache_dir, exist_ok=True)
        self.tiles = {}
        self.lock = threading.Lock()
        self.sheets = []          # lokalne arkusze: (xmin, ymin, cell, array)
        self.failed = 0
        if folder:
            self.load_folder(folder)

    # --- lokalne arkusze
    def load_folder(self, folder):
        for root, _, files in os.walk(folder):
            for f in files:
                p = os.path.join(root, f)
                if f.lower().endswith(".asc"):
                    self.sheets.append(read_asc(p))
                elif f.lower().endswith((".tif", ".tiff")):
                    try:
                        self.sheets.append(read_geotiff_file(p))
                    except Exception as ex:  # noqa: BLE001
                        print("pominiety", p, ex)

    # --- kwadraty z uslugi WCS (jak editor/editorNmt.cpp: nazwa pokrycia z GetCapabilities)
    _cov = None

    def coverage(self):
        if NMT._cov is None:
            NMT._cov = COVERAGE
            try:
                import re
                with urllib.request.urlopen(f"{SERVICE}?SERVICE=WCS&VERSION=1.0.0&REQUEST=GetCapabilities", timeout=60) as r:
                    txt = r.read().decode("utf-8", "replace")
                brief = re.search(r"CoverageOfferingBrief>(.*?)</(?:\w+:)?CoverageOfferingBrief", txt, re.S | re.I)
                m = re.search(r"<(?:\w+:)?name>([^<]+)</(?:\w+:)?name>", brief.group(1), re.I) if brief else None
                if m:
                    NMT._cov = m.group(1).strip()
            except Exception:  # noqa: BLE001
                pass
        return NMT._cov

    def key(self, E, N):
        return int(math.floor(E / SQUARE)), int(math.floor(N / SQUARE))

    def fetch(self, k):
        path = os.path.join(self.cache, f"{k[0]}_{k[1]}_{self.res:g}.npy")
        if os.path.exists(path):
            return np.load(path)
        e0, n0 = k[0] * SQUARE, k[1] * SQUARE
        n = int(round(SQUARE / self.res))
        # siatka komorek: srodek pierwszej komorki w e0 + res/2
        url = (f"{SERVICE}?SERVICE=WCS&VERSION=1.0.0&REQUEST=GetCoverage&COVERAGE={self.coverage()}"
               f"&CRS=EPSG:2180&RESPONSE_CRS=EPSG:2180&BBOX={e0},{n0},{e0 + SQUARE},{n0 + SQUARE}"
               f"&WIDTH={n}&HEIGHT={n}&FORMAT=GEOTIFF")
        for attempt in range(3):
            try:
                with urllib.request.urlopen(url, timeout=60) as r:
                    data = r.read()
                import tifffile
                arr = tifffile.imread(io.BytesIO(data)).astype(np.float32)
                arr[arr < -1000] = np.nan
                np.save(path, arr)
                return arr
            except Exception:  # noqa: BLE001
                continue
        self.failed += 1
        return None

    def prefetch(self, points, workers=8):
        keys = sorted({self.key(E, N) for E, N in points} - set(self.tiles))
        with ThreadPoolExecutor(workers) as ex:
            for k, arr in zip(keys, ex.map(self.fetch, keys)):
                self.tiles[k] = arr
        return len(keys)

    def height(self, E, N):
        for xmin, ymax, cell, arr in self.sheets:
            c = (E - xmin) / cell - 0.5
            r = (ymax - N) / cell - 0.5
            if 0 <= c < arr.shape[1] - 1 and 0 <= r < arr.shape[0] - 1:
                return bilinear(arr, r, c)
        k = self.key(E, N)
        if k not in self.tiles:
            self.tiles[k] = self.fetch(k)
        arr = self.tiles[k]
        if arr is None:
            return float("nan")
        cell = SQUARE / arr.shape[1]
        c = (E - k[0] * SQUARE) / cell - 0.5
        r = (k[1] * SQUARE + SQUARE - N) / cell - 0.5
        c = min(max(c, 0), arr.shape[1] - 1.001)
        r = min(max(r, 0), arr.shape[0] - 1.001)
        return bilinear(arr, r, c)


def bilinear(a, r, c):
    r0, c0 = int(r), int(c)
    fr, fc = r - r0, c - c0
    v = (a[r0, c0] * (1 - fr) * (1 - fc) + a[r0, c0 + 1] * (1 - fr) * fc +
         a[r0 + 1, c0] * fr * (1 - fc) + a[r0 + 1, c0 + 1] * fr * fc)
    return float(v)


def read_asc(path):
    hdr = {}
    with open(path) as f:
        for _ in range(6):
            k, v = f.readline().split()
            hdr[k.lower()] = float(v)
        arr = np.loadtxt(f, dtype=np.float32)
    nd = hdr.get("nodata_value", -9999)
    arr[arr == nd] = np.nan
    cell = hdr["cellsize"]
    x0 = hdr.get("xllcorner", hdr.get("xllcenter", 0) - cell / 2)
    y0 = hdr.get("yllcorner", hdr.get("yllcenter", 0) - cell / 2)
    # uwaga: w arkuszach GUGiK x/y bywaja zamienione (x = northing); wykrywamy po zakresie
    if x0 > 1_000_000 or (y0 < 1_000_000 and x0 > y0 and x0 > 600_000):
        x0, y0 = y0, x0
    return (x0, y0 + cell * arr.shape[0], cell, arr)


def read_geotiff_file(path):
    import tifffile
    with tifffile.TiffFile(path) as t:
        page = t.pages[0]
        arr = page.asarray().astype(np.float32)
        tags = page.tags
        scale = tags["ModelPixelScaleTag"].value
        tie = tags["ModelTiepointTag"].value
    arr[arr < -1000] = np.nan
    return (tie[3], tie[4], scale[0], arr)


# ----------------------------------------------------------------------------
# niweleta
# ----------------------------------------------------------------------------
def dp_profile(s, z, tol, keep_fixed=()):
    keep = np.zeros(len(s), bool)
    keep[0] = keep[-1] = True
    for i in keep_fixed:
        keep[i] = True
    stack = [(0, len(s) - 1)]
    while stack:
        a, b = stack.pop()
        if b <= a + 1:
            continue
        zz = z[a] + (z[b] - z[a]) * (s[a + 1:b] - s[a]) / (s[b] - s[a])
        d = np.abs(z[a + 1:b] - zz)
        i = int(np.argmax(d))
        if d[i] > tol:
            m = a + 1 + i
            keep[m] = True
            stack += [(a, m), (m, b)]
    return np.where(keep)[0]


def fill_nan(s, g):
    m = np.isfinite(g)
    if m.sum() < 2:
        return None
    return np.interp(s, s[m], g[m])


# ST-T1-A6 tab. 10.1 (pochylenia, promile) i 10.2 (luki pionowe)
GRADE = {"P": {"P0": 6.0, "P1": 17.5, "P2": 35.0}, "M": {"P0": 6.0, "P1": 10.0, "P2": 20.0}}
GRADE_SHORT = {"M": {"P1": 17.5, "P2": 35.0}}      # odcinki krotsze niz 500 m (przypis 2)
STATION = {"P0": 2.0, "P1": 2.0, "P2": 2.5}         # tory na stacjach (dlugosc uzyteczna)
PLATFORM = {"P0": 2.5, "P1": 6.0, "P2": 10.0}       # tory przy peronach
AVG10 = {"P": {"P0": 12.5, "P1": 25.0}, "M": {"P0": 6.25, "P1": 12.5}}
VC = {"P0": (0.77, 5000.0), "P1": (0.35, 5000.0), "P2": (0.25, 2000.0)}   # R >= max(k V^2, Rmin)


def slope_limit(s, z, g_step, pinned=None):
    """Krzywa o pochyleniu na kazdym kroku nie wiekszym niz g_step [promile]: srednia przejsc w przod i w tyl."""
    g = np.asarray(g_step, float) / 1000.0 * np.diff(s)
    pin = pinned or {}

    def fwd(v):
        v = v.copy()
        for i in range(1, len(v)):
            if i in pin:
                v[i] = pin[i]
                continue
            v[i] = min(max(v[i], v[i - 1] - g[i - 1]), v[i - 1] + g[i - 1])
        return v

    def bwd(v):
        v = v.copy()
        for i in range(len(v) - 2, -1, -1):
            if i in pin:
                v[i] = pin[i]
                continue
            v[i] = min(max(v[i], v[i + 1] - g[i]), v[i + 1] + g[i])
        return v

    z = z.copy()
    for i, h in pin.items():
        z[i] = h
    m = 0.5 * (fwd(z) + bwd(z))
    return bwd(fwd(m))


def fit_grade_line(s, ground, V, fixed=None, grade_tol=0.5, bridge_tol=2.5, grade_max=None,
                   line_type="M", zones=()):
    """Niweleta wg ST-T1-A6 rozdz. 10. zones: [(s_od, s_do, 'ST'|'PO', nazwa)].
    Zwraca (pvi_s, pvi_z, radii, ostrzezenia, statystyki)."""
    warn = []
    fixed = dict(fixed or {})
    g = fill_nan(s, np.array(ground, float))
    if g is None:
        return None
    from scipy.ndimage import median_filter
    z = median_filter(g, size=5, mode="nearest")
    lim = dict(GRADE[line_type])
    if grade_max:
        lim["P2"] = grade_max
    # dopuszczalne pochylenie na kazdym kroku: P2 linii, w strefach stacji / peronow P1 strefy
    g_step = np.full(len(s) - 1, lim["P2"])
    mids = 0.5 * (s[1:] + s[:-1])
    keep = set(fixed)
    zones = [(float(s[int(np.argmin(np.abs(s - a)))]), float(s[int(np.argmin(np.abs(s - b)))]), k, n) for a, b, k, n in zones]
    for a, b, kind, _ in zones:
        m = (mids >= a) & (mids <= b)
        g_step[m] = np.minimum(g_step[m], (STATION if kind == "ST" else PLATFORM)["P1"])
        for x in (a, b):
            i = int(np.argmin(np.abs(s - x)))
            if 0 < i < len(s) - 1:
                keep.add(i)
    mask = np.ones(len(s), bool)
    for it in range(6):
        zz = fill_nan(s, np.where(mask, z, np.nan))
        if zz is None:
            zz = z.copy()
        zz = slope_limit(s, zz, g_step, fixed)
        tol = max(4.0 * bridge_tol, 5.0) if it == 0 else grade_tol
        idx = dp_profile(s, zz, tol, tuple(sorted(keep)))
        line = np.interp(s, s[idx], zz[idx])
        new = np.abs(z - line) <= bridge_tol
        for i in fixed:
            new[i] = True
        if (new == mask).all():
            break
        mask = new
    ps, pz = s[idx].astype(float), zz[idx].astype(float)
    protected = {float(s[i]) for i in keep}

    def vat(x):
        return float(np.interp(x, s, V))

    # najkrotszy odcinek stalego pochylenia (tab. 10.1, regulacja: P0 0,4 V, P1 20 m) - za krotkie scal
    changed = True
    while changed and len(ps) > 2:
        changed = False
        seg = np.diff(ps)
        for i in np.argsort(seg):
            if seg[i] >= max(0.4 * vat(ps[i]), 20.0):
                break
            cand = [k for k in (i, i + 1) if 0 < k < len(ps) - 1 and ps[k] not in protected]
            if not cand:
                continue
            k = min(cand, key=lambda k: abs(np.interp(ps[k], np.delete(ps, k), np.delete(pz, k)) - pz[k]))
            ps, pz = np.delete(ps, k), np.delete(pz, k)
            changed = True
            break
    # luk pionowy P2 musi sie zmiescic: T_i + T_i+1 + 20 m <= dlugosc odcinka; inaczej usun PVI
    def t_req(i, ps_, pz_):
        if i <= 0 or i >= len(ps_) - 1:
            return 0.0
        g1 = (pz_[i] - pz_[i - 1]) / (ps_[i] - ps_[i - 1])
        g2 = (pz_[i + 1] - pz_[i]) / (ps_[i + 1] - ps_[i])
        dg = abs(g2 - g1)
        if dg * 1000.0 <= 2.0:
            return 0.0
        k, rmin = VC["P2"]
        v = vat(ps_[i])
        return max(k * v * v, rmin) * dg / 2.0
    for _ in range(5 * len(ps)):
        T2 = np.array([t_req(i, ps, pz) for i in range(len(ps))])
        seg = np.diff(ps)
        bad = np.where(T2[:-1] + T2[1:] + 20.0 > seg + 1e-6)[0]
        if not len(bad):
            break
        j = bad[int(np.argmax((T2[:-1] + T2[1:] + 20.0 - seg)[bad]))]
        cand = [k for k in (j, j + 1) if 0 < k < len(ps) - 1 and ps[k] not in protected]
        if not cand:
            # obie chronione - nic nie zrobimy, luk ponizej P2 trafi do raportu
            break
        k = min(cand, key=lambda k: abs(np.interp(ps[k], np.delete(ps, k), np.delete(pz, k)) - pz[k]))
        ps, pz = np.delete(ps, k), np.delete(pz, k)
    gr = np.diff(pz) / np.diff(ps) * 1000.0
    stats = dict(pochylenia=dict(P0=0, P1=0, P2=0, ponad_P2=0), luki=dict(P0=0, P1=0, P2=0, ponizej_P2=0),
                 strefy_stacyjne=0, strefy_ponad_P1=0)
    # klasy pochylen
    for i, x in enumerate(gr):
        L = ps[i + 1] - ps[i]
        a = abs(x)
        l1 = GRADE_SHORT.get(line_type, lim)["P1"] if L < 500 else lim["P1"]
        l2 = GRADE_SHORT.get(line_type, lim)["P2"] if L < 500 else lim["P2"]
        if a <= lim["P0"] + 0.05:
            stats["pochylenia"]["P0"] += 1
        elif a <= l1 + 0.05:
            stats["pochylenia"]["P1"] += 1
        elif a <= l2 + 0.05:
            stats["pochylenia"]["P2"] += 1
            warn.append(f"pochylenie {x:+.1f} promila km {ps[i] / 1000:.3f}-{ps[i + 1] / 1000:.3f} (P2, wymaga zgody ZLK)")
        else:
            stats["pochylenia"]["ponad_P2"] += 1
            warn.append(f"pochylenie {x:+.1f} promila km {ps[i] / 1000:.3f}-{ps[i + 1] / 1000:.3f} PONAD P2")
    # strefy stacji i peronow
    for a, b, kind, name in zones:
        stats["strefy_stacyjne"] += 1
        m = (ps[1:] > a + 1e-6) & (ps[:-1] < b - 1e-6)
        if m.any() and np.max(np.abs(gr[m])) > (STATION if kind == "ST" else PLATFORM)["P1"] + 1e-6:
            stats["strefy_ponad_P1"] += 1
            warn.append(f"{name}: pochylenie {np.max(np.abs(gr[m])):.1f} promila w strefie {kind}")
    # srednie pochylenie na 10 km
    if s[-1] > 10000:
        zl = np.interp(s, ps, pz)
        step = s[1] - s[0]
        n10 = int(10000 / step)
        avg = np.abs(zl[n10:] - zl[:-n10]) / 10.0
        if avg.size and avg.max() > AVG10[line_type]["P1"]:
            warn.append(f"srednie pochylenie na 10 km {avg.max():.1f} promila > P1 {AVG10[line_type]['P1']}")
    # luki pionowe: zmiana pochylenia > 2 promile (10.2 pkt 2), R wg tab. 10.2 (P0 -> P1 -> P2),
    # miedzy lukami prosty odcinek >= 20 m (tab. 10.1 P1), dlugosc luku >= max(0,4 V; 20) zalecana (10.1)
    radii = np.zeros(len(ps))
    T = np.zeros(len(ps))
    order = np.argsort(-np.abs(np.diff(gr))) + 1 if len(gr) > 1 else []
    for i in order:
        dg = abs(gr[i] - gr[i - 1]) / 1000.0
        if dg * 1000.0 <= 2.0 + 1e-9:
            continue
        v = vat(ps[i])
        left = ps[i] - ps[i - 1] - T[i - 1] - 20.0
        right = ps[i + 1] - ps[i] - T[i + 1] - 20.0
        room = max(0.0, min(left, right))
        chosen = None
        for cls in ("P0", "P1", "P2"):
            k, rmin = VC[cls]
            R = max(k * v * v, rmin)
            if R * dg / 2.0 <= room:
                chosen = (cls, R)
                break
        if chosen is None:
            R = 2.0 * room / dg if room > 2.0 else 0.0
            stats["luki"]["ponizej_P2"] += 1
            warn.append(f"luk pionowy km {ps[i] / 1000:.3f}: miejsce tylko na R {R:.0f} m (ponizej P2)")
            if R <= 0:
                continue
        else:
            stats["luki"][chosen[0]] += 1
            R = chosen[1]
        radii[i] = R
        T[i] = R * dg / 2.0
        if 2 * T[i] < max(0.4 * v, 20.0) - 1e-6:
            pass   # zalecenie, nie wymog
    stats["limit_P2"] = lim["P2"]
    return ps, pz, radii, warn, stats


def elevation(ps, pz, radii, x):
    """Wysokosc niwelety z lukami pionowymi (parabole)."""
    x = np.atleast_1d(np.asarray(x, float))
    y = np.interp(x, ps, pz)
    gr = np.diff(pz) / np.diff(ps)
    for i in range(1, len(ps) - 1):
        R = radii[i]
        if R <= 0:
            continue
        dg = gr[i] - gr[i - 1]
        T = R * abs(dg) / 2.0
        m = (x > ps[i] - T) & (x < ps[i] + T)
        u = x[m] - (ps[i] - T)
        y[m] = pz[i] - gr[i - 1] * T + gr[i - 1] * u + dg * u * u / (4.0 * T)
    return y


def vertical_breaks(ps, radii, gr):
    """Granice, w ktorych kawalki torow trzeba podzielic: PVI bez luku, poczatki/konce lukow i co 10 m w luku."""
    out = []
    for i in range(1, len(ps) - 1):
        R = radii[i]
        if R <= 0:
            out.append(ps[i])
            continue
        T = R * abs(gr[i] - gr[i - 1]) / 2.0
        if T < 1.0:
            out.append(ps[i])
            continue
        n = max(1, int(math.ceil(2 * T / 10.0)))
        out += list(np.linspace(ps[i] - T, ps[i] + T, n + 1))
    return sorted(out)


class PointCloud:
    """Wysokosci terenu probkowane w przegladarce wzdluz uproszczonych osi (plik NMT1 + osie JSON).
    Probki co 5 m po lamanej, w tej samej kolejnosci co w osie_uproszczone.json."""

    def __init__(self, path, axes_json=None, step=5.0):
        import json
        import struct
        from scipy.spatial import cKDTree
        axes_json = axes_json or os.path.join(os.path.dirname(os.path.abspath(path)), "osie_uproszczone.json")
        axes = json.load(open(axes_json))
        data = open(path, "rb").read()
        assert data[:4] == b"NMT1", "zly plik"
        n_ax = struct.unpack_from("<I", data, 4)[0]
        assert n_ax == len(axes), (n_ax, len(axes))
        off = 8
        pts, hs = [], []
        for a in axes:
            P = np.array(a["pts"], float)
            cum = np.concatenate([[0.0], np.cumsum(np.hypot(*np.diff(P, axis=0).T))])
            L = cum[-1]
            ss = list(np.arange(0.0, L + 1e-9, step))
            if L - ss[-1] > 0.01:
                ss.append(L)
            ss = np.array(ss)
            x = np.interp(ss, cum, P[:, 0])
            y = np.interp(ss, cum, P[:, 1])
            n = struct.unpack_from("<I", data, off)[0]
            off += 4
            h = np.frombuffer(data, "<i2", n, off).astype(float)
            off += 2 * n
            assert n == len(ss), (a["name"], n, len(ss))
            h[h == -32768] = np.nan
            pts.append(np.column_stack([x, y]))
            hs.append(h / 10.0)
        self.pts = np.vstack(pts)
        self.h = np.concatenate(hs)
        ok = np.isfinite(self.h)
        self.pts, self.h = self.pts[ok], self.h[ok]
        self.tree = cKDTree(self.pts)
        self.failed = int((~ok).sum())

    def prefetch(self, points, workers=0):
        return 0

    def height(self, E, N):
        d, j = self.tree.query((E, N), k=3)
        if d[0] > 15.0:
            return float("nan")
        w = 1.0 / np.maximum(d, 0.5) ** 2
        return float(np.sum(self.h[j] * w) / np.sum(w))
