#!/usr/bin/env python3
"""
irt2maszyna.py - surowe osie linii kolejowych (GeoJSON z warstwy IRT Geoportalu
Dolnego Slaska, EPSG:2180) -> tory MaSzyny (.inc), z korekcja geometrii wg
PKP PLK ST-T1-A6 "Uklady geometryczne torow" (obowiazuje od 17.04.2025).

Co robi korekcja (rozdz. 9 A6):
  * lamana z danych -> wieloboki (punkty zalaman, PI) -> proste + krzywe
    przejsciowe (klotoida ~ parabola 3 st., rampa prostoliniowa) + luki kolowe,
  * promien dopasowany do surowych punktow, wybierany jak najwiekszy (9.2 pkt 1),
    nie mniejszy niz Rmin P2 = 180 m (tab. 9.2),
  * przechylka D z przedzialu <Dreg; Deq> (9.3 pkt 12b), Dmax wg tab. 9.6 i (9.8),
    min. 20 mm, niedomiar I <= Idop P1 (tab. 9.7),
  * gdy luk nie pozwala na predkosc linii: lokalne ograniczenie predkosci
    V = sqrt((D + Idop) * R / 11,8) (wzor 9.1), zaokraglone w dol do 10 km/h,
  * dlugosc krzywej przejsciowej z tab. 9.10 (dD/ds tab. 9.12, dD/dt tab. 9.13,
    dI/dt tab. 9.14, 30 m, sqrt(qR*R)), najpierw P0, potem P1, P2,
  * minimalne proste miedzy lukami z tab. 9.1 (P1, w razie braku miejsca P2),
  * luki zgodnie skierowane, ktore sie nie mieszcza, sa scalane w jeden,
  * drugi tor w odstepie e (wielokrotnosc 0,05 m, 9.13) - po lewej stronie
    w kierunku kilometracji; tor 1 lezy na osi z danych.

Uklad MaSzyny (sprawdzony w zrodlach eu07/maszyna): X rosnie na ZACHOD,
Z na polnoc, Y w gore. roll w stopniach, dodatni podnosi lewy tok.
"""
import argparse
import csv
import json
import math
import os
from collections import defaultdict

import numpy as np
from scipy.spatial import cKDTree

# ----------------------------------------------------------------------------
# ST-T1-A6 - wartosci dopuszczalne
# ----------------------------------------------------------------------------
CLS = ("P0", "P1", "P2")
A6 = {
    "Rmin": {"P0": 600, "P1": 300, "P2": 180},          # tab. 9.2 szlak/gl. zasadnicze
    "Rmax": 50000,                                        # tab. 9.2 P1
    "Idop": {"P0": 110, "P1": 130, "P2": 150},           # tab. 9.7 [mm]
    "dDds": {"P0": 1.6, "P1": 2.0, "P2": 2.5},           # tab. 9.12 [mm/m]
    "dDdt": {"P0": 35.0, "P1": 50.0, "P2": 60.0},        # tab. 9.13 rampy prostoliniowe [mm/s]
    "dIdt_le200": {"P0": 55.0, "P1": 70.0, "P2": 80.0},  # tab. 9.14 V<=200
    "dIdt_gt200": {"P0": 55.0, "P1": 55.0, "P2": 70.0},  # tab. 9.14 200<V<=250
    "qN": 1.0, "qR": 0.48,                                # tab. 9.10, parabola 3 st./klotoida
    "Dmin_nonzero": 20,                                   # tab. 9.6
}


def d_max(R):
    """Max przechylka [mm], tab. 9.6 (P1, nawierzchnia podsypkowa) + wzor 9.8."""
    if R <= 200:
        d = 60.0
    elif R <= 300:
        d = 100.0
    else:
        d = 150.0
    if R < 300:
        d = min(d, (R - 50.0) / 1.5)
    return max(0.0, d)


def l_min_straight(V, cls):
    """Min. dlugosc prostej / luku, tab. 9.1 [m]."""
    if V <= 120:
        return max(0.35 * V, 30.0) if cls != "P2" else max(0.25 * V, 20.0)
    if V <= 200:
        return 0.40 * V if cls != "P2" else 0.30 * V
    return 0.65 * V if cls != "P2" else 0.55 * V


def ceil5(x):
    return 5.0 * math.ceil(x / 5.0 - 1e-9)


def floor5(x):
    return 5.0 * math.floor(x / 5.0 + 1e-9)


def design_cant(R, V):
    """Przechylka wg 9.3 pkt 12b: D w <Dreg; Deq>, I <= Idop P1. Zwraca (D, I, Deq)."""
    Deq = 11.8 * V * V / R
    Dreg = 6.5 * V * V / R
    Dlim = floor5(d_max(R))
    D = max(Dreg, Deq - A6["Idop"]["P1"])
    D = min(ceil5(D), Dlim)
    if D < A6["Dmin_nonzero"]:
        D = 0.0 if Deq <= A6["Idop"]["P0"] else min(float(A6["Dmin_nonzero"]), Dlim)
    return D, Deq - D, Deq


def v_allowed(R, D):
    """Wzor 9.1 przeksztalcony: V = sqrt((D + Idop) R / 11,8), Idop P1."""
    return math.sqrt((D + A6["Idop"]["P1"]) * R / 11.8)


def curve_speed_and_cant(R, V_line):
    """Lokalna predkosc w luku i przechylka. Predkosc tniemy do pelnych 10 km/h."""
    V = V_line
    D, I, Deq = design_cant(R, V)
    if I > A6["Idop"]["P1"] + 1e-6:
        Dlim = floor5(d_max(R))
        V = max(10.0, 10.0 * math.floor(v_allowed(R, Dlim) / 10.0))
        D, I, Deq = design_cant(R, V)
    return V, D, I, Deq


def transition_length(R, D, I, V, cls):
    """Min. dlugosc krzywej przejsciowej prosta-luk, tab. 9.10 + wzor 9.14."""
    dIdt = A6["dIdt_le200" if V <= 200 else "dIdt_gt200"][cls]
    v = V / 3.6
    terms = [
        A6["qN"] * D / A6["dDds"][cls],
        A6["qN"] * v * D / A6["dDdt"][cls],
        A6["qN"] * v * abs(I) / dIdt,
    ]
    if cls != "P2":
        terms += [30.0, math.sqrt(A6["qR"] * R)]
    return math.ceil(max(terms))


# ----------------------------------------------------------------------------
# geometria krzywej: klotoida - luk - klotoida (symetrycznie)
# ----------------------------------------------------------------------------
def clothoid_xy(L, R):
    """Koniec klotoidy o dlugosci L dochodzacej do promienia R (szeregi)."""
    if L <= 0:
        return 0.0, 0.0
    A2 = R * L
    x = L - L ** 5 / (40 * A2 ** 2) + L ** 9 / (3456 * A2 ** 4)
    y = L ** 3 / (6 * A2) - L ** 7 / (336 * A2 ** 3)
    return x, y


def tangent_length(R, L, gamma):
    """Dlugosc stycznej T od PI do poczatku krzywej przejsciowej."""
    if L <= 0:
        return R * math.tan(gamma / 2.0)
    tau = L / (2.0 * R)
    x, y = clothoid_xy(L, R)
    n = y - R * (1 - math.cos(tau))       # przesuniecie luku
    m = x - R * math.sin(tau)
    return (R + n) * math.tan(gamma / 2.0) + m


class Curve:
    __slots__ = ("R", "L", "cls", "V", "D", "I", "Deq", "R_fit", "flags", "rms", "maxdev")

    def __init__(self):
        self.flags = []


def curve_params(R, gamma, V_line, cls_list=CLS):
    """Dobiera V, D, L dla promienia R. Zwraca liste wariantow (cls, L) od najlepszego."""
    V, D, I, Deq = curve_speed_and_cant(R, V_line)
    out = []
    for cls in cls_list:
        L = transition_length(R, D, I, V, cls)
        out.append((cls, L))
    return V, D, I, Deq, out


# ----------------------------------------------------------------------------
# elementy osi i ich probkowanie
# ----------------------------------------------------------------------------
# element: dict(kind, L, k0, k1, D0, D1, V, src) ; krzywizna liniowa po dlugosci,
# przechylka liniowa (rampa prostoliniowa).

def integrate(p0, h0, L, k0, k1, n=None):
    """Calkowanie kierunku dla krzywizny liniowej. Zwraca s, x, y, h."""
    if n is None:
        n = max(2, int(math.ceil(L / 0.5)) + 1)
    s = np.linspace(0.0, L, n)
    h = h0 + k0 * s + (k1 - k0) * s * s / (2.0 * L if L > 0 else 1.0)
    if k0 == 0 and k1 == 0:
        x = p0[0] + s * math.cos(h0)
        y = p0[1] + s * math.sin(h0)
        return s, x, y, h
    c, sn = np.cos(h), np.sin(h)
    ds = np.diff(s)
    x = p0[0] + np.concatenate([[0.0], np.cumsum(0.5 * (c[1:] + c[:-1]) * ds)])
    y = p0[1] + np.concatenate([[0.0], np.cumsum(0.5 * (sn[1:] + sn[:-1]) * ds)])
    return s, x, y, h


def curve_elements(R, L, gamma_signed, D, V, tag, csign=None):
    k = math.copysign(1.0 / R, gamma_signed)
    cs = math.copysign(1.0, gamma_signed) if csign is None else csign
    g = abs(gamma_signed)
    La = max(0.0, R * g - L)
    els = []
    if L > 0:
        els.append(dict(kind="KP", L=L, k0=0.0, k1=k, D0=0.0, D1=D, V=V, tag=tag, csign=cs))
    if La > 1e-6:
        els.append(dict(kind="LUK", L=La, k0=k, k1=k, D0=D, D1=D, V=V, tag=tag, csign=cs))
    if L > 0:
        els.append(dict(kind="KP", L=L, k0=k, k1=0.0, D0=D, D1=0.0, V=V, tag=tag, csign=cs))
    return els


def sample_polyline(p0, h0, els, step=0.5):
    """Gesta polilinia z listy elementow (do pomiaru odchylek)."""
    pts = [np.array([[p0[0], p0[1]]])]
    p, h = p0, h0
    for e in els:
        if e["L"] <= 0:
            continue
        s, x, y, hh = integrate(p, h, e["L"], e["k0"], e["k1"], max(2, int(e["L"] / step) + 1))
        pts.append(np.column_stack([x[1:], y[1:]]))
        p, h = (x[-1], y[-1]), hh[-1]
    return np.vstack(pts), p, h


# ----------------------------------------------------------------------------
# dane wejsciowe
# ----------------------------------------------------------------------------
def load_chains(path, lines_filter=None, join_tol=2.0):
    data = json.load(open(path, encoding="utf-8"))
    by_ln = defaultdict(list)
    stations = []
    for f in data["features"]:
        p = f["properties"]
        if p.get("kind") == "line":
            if lines_filter and str(p["LN"]) not in lines_filter:
                continue
            paths = [np.array(c, float) for c in f["geometry"]["coordinates"] if len(c) >= 2]
            if not paths:
                continue
            # sklej czesci MultiLineString w jedna polilinie
            poly = paths[0]
            for q in paths[1:]:
                cand = [(np.linalg.norm(poly[-1] - q[0]), poly, q),
                        (np.linalg.norm(poly[-1] - q[-1]), poly, q[::-1]),
                        (np.linalg.norm(poly[0] - q[-1]), q, poly),
                        (np.linalg.norm(poly[0] - q[0]), q[::-1], poly)]
                _, a, b = min(cand, key=lambda t: t[0])
                poly = np.vstack([a, b[1:] if np.linalg.norm(a[-1] - b[0]) < join_tol else b])
            by_ln[str(p["LN"])].append((p, poly))
        elif p.get("kind", "").startswith("station"):
            stations.append((p, f["geometry"]["coordinates"]))

    chains = []
    for ln, feats in by_ln.items():
        feats.sort(key=lambda t: (t[0]["km_s"], t[0]["km_e"]))
        cur = None
        for p, poly in feats:
            if cur is None:
                cur = dict(LN=ln, pts=[poly], attrs=[p])
                continue
            last = cur["pts"][-1]
            # pierwszy odcinek lancucha moze byc odwrocony - dopasuj oba konce
            opts = [(np.linalg.norm(last[-1] - poly[0]), False, False),
                    (np.linalg.norm(last[-1] - poly[-1]), False, True)]
            if len(cur["pts"]) == 1:
                opts += [(np.linalg.norm(last[0] - poly[0]), True, False),
                         (np.linalg.norm(last[0] - poly[-1]), True, True)]
            d, rev_last, rev_poly = min(opts, key=lambda t: t[0])
            if d <= join_tol:
                if rev_last:
                    cur["pts"][-1] = last[::-1]
                cur["pts"].append(poly[::-1] if rev_poly else poly)
                cur["attrs"].append(p)
            else:
                chains.append(cur)
                cur = dict(LN=ln, pts=[poly], attrs=[p])
        if cur:
            chains.append(cur)

    out = []
    for i, c in enumerate(chains):
        verts, attr_idx = [], []
        for j, poly in enumerate(c["pts"]):
            start = 1 if verts else 0
            for v in poly[start:]:
                verts.append(v)
                attr_idx.append(j)
        verts = np.array(verts)
        # usun duplikaty
        keep = np.concatenate([[True], np.linalg.norm(np.diff(verts, axis=0), axis=1) > 0.05])
        verts, attr_idx = verts[keep], np.array(attr_idx)[keep]
        verts, attr_idx, nsp = remove_spikes(verts, attr_idx)
        if len(verts) < 2:
            continue
        seg = np.linalg.norm(np.diff(verts, axis=0), axis=1)
        sacc = np.concatenate([[0.0], np.cumsum(seg)])
        out.append(dict(LN=c["LN"], verts=verts, attr_idx=attr_idx, attrs=c["attrs"], s=sacc, spikes=nsp))
    # numer lancucha w obrebie linii
    cnt = defaultdict(int)
    for c in out:
        cnt[c["LN"]] += 1
        c["name"] = f"L{c['LN']}" + (f"_{cnt[c['LN']]}" if cnt[c["LN"]] > 1 else "")
    return out, stations


def remove_spikes(v, ai, max_angle=30.0, short=30.0):
    """Usuwa artefakty digitalizacji: ostre zalamania (> max_angle) przy krotkim
    sasiednim odcinku (< short m) - typowo 'wasy' na stykach odcinkow IRT."""
    v, ai = v.copy(), np.array(ai)
    removed = 0
    while len(v) > 2:
        d = np.diff(v, axis=0)
        ln = np.hypot(d[:, 0], d[:, 1])
        h = np.arctan2(d[:, 1], d[:, 0])
        g = np.degrees(np.abs((np.diff(h) + np.pi) % (2 * np.pi) - np.pi))
        bad = ((g > max_angle) & ((ln[:-1] < short) | (ln[1:] < short))) | (g > 150.0)
        if not bad.any():
            break
        k = int(np.argmax(np.where(bad, g, -1))) + 1
        v = np.delete(v, k, axis=0)
        ai = np.delete(ai, k)
        removed += 1
    return v, ai, removed


def douglas_peucker(P, eps):
    keep = np.zeros(len(P), bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(P) - 1)]
    while stack:
        a, b = stack.pop()
        if b <= a + 1:
            continue
        A, B = P[a], P[b]
        d = B - A
        nrm = np.hypot(*d)
        seg = P[a + 1:b] - A
        if nrm < 1e-9:
            dist = np.hypot(seg[:, 0], seg[:, 1])
        else:
            dist = np.abs(seg[:, 0] * d[1] - seg[:, 1] * d[0]) / nrm
        i = int(np.argmax(dist))
        if dist[i] > eps:
            m = a + 1 + i
            keep[m] = True
            stack += [(a, m), (m, b)]
    return np.where(keep)[0]


# ----------------------------------------------------------------------------
# projekt osi dla jednego lancucha
# ----------------------------------------------------------------------------
def heading(a, b):
    return math.atan2(b[1] - a[1], b[0] - a[0])


def wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


R_CANDIDATES = np.unique(np.round(np.geomspace(A6["Rmin"]["P2"], 30000, 70), -1))


class Aligner:
    def __init__(self, chain, args):
        self.c = chain
        self.a = args
        self.tree = cKDTree(chain["verts"])
        self.log = []
        self.rejected = []

    def v_at(self, xy):
        """Predkosc linii w okolicy punktu (z odcinka IRT najblizszego punktu)."""
        _, i = self.tree.query(xy)
        p = self.c["attrs"][self.c["attr_idx"][i]]
        v = float(p.get("v_k") or 0)
        return v if v > 0 else float(self.a.default_speed)

    def attrs_at(self, xy):
        _, i = self.tree.query(xy)
        return self.c["attrs"][self.c["attr_idx"][i]]

    # --- dopasowanie promienia w jednym PI
    def fit_pi(self, PI, i):
        prev, cur, nxt = PI[i - 1], PI[i], PI[i + 1]
        h_in, h_out = heading(prev, cur), heading(cur, nxt)
        gs = wrap(h_out - h_in)
        g = abs(gs)
        a, b = np.hypot(*(cur - prev)), np.hypot(*(nxt - cur))
        V_line = self.v_at(cur)
        # punkty zrodlowe w oknie (do polowy sasiednich ramion)
        lo = cur + (prev - cur) * 0.5
        hi = cur + (nxt - cur) * 0.5
        # punkty zrodlowe po kolei wzdluz linii (odporne na petle/zawrotki)
        _, ia = self.tree.query(lo)
        _, ib = self.tree.query(hi)
        ia, ib = sorted((int(ia), int(ib)))
        src = self.c["verts"][ia:ib + 1]
        if len(src) == 0:
            src = cur[None, :]
        u_in = (cur - prev) / a
        u_out = (nxt - cur) / b
        res = []
        for R in R_CANDIDATES:
            V, D, I, Deq, var = curve_params(R, g, V_line, ("P1",))
            L = var[0][1]
            if R * g < L:            # luk bez czesci kolowej - skroc KP
                L = R * g
            T = tangent_length(R, L, g)
            if T > 0.98 * min(a, b) and R > R_CANDIDATES[0]:
                continue
            TS = cur - u_in * T
            els = curve_elements(R, L, gs, D, V, "")
            poly, endp, _ = sample_polyline(TS, h_in, els, step=2.0)
            k1 = max(2, int(np.hypot(*(TS - lo)) / 2.0) + 1)
            k2 = max(2, int(np.hypot(*(hi - endp)) / 2.0) + 1)
            line = np.vstack([np.linspace(lo, TS, k1), poly, np.linspace(np.array(endp), hi, k2)])
            kd = cKDTree(line)
            d, _ = kd.query(src)
            # odchylka w obrebie samej krzywej (TS..ST) - kryterium scalania i raport
            _, ca = self.tree.query(TS)
            _, cb = self.tree.query(endp)
            ca, cb = sorted((int(ca), int(cb)))
            dc, _ = kd.query(self.c["verts"][ca:cb + 1]) if cb >= ca else (d, None)
            res.append((R, float(np.sqrt(np.mean(d * d))), float(np.max(dc)) if len(dc) else float(d.max())))
        if not res:
            return float(R_CANDIDATES[0]), np.nan, np.nan, gs
        best = min(res, key=lambda r: r[1])
        # 9.2 pkt 1: najwiekszy promien, ktory wciaz dobrze opisuje dane
        ok = [r for r in res if r[1] <= best[1] + self.a.fit_slack and r[2] <= best[2] + 2 * self.a.fit_slack]
        R = max(r[0] for r in ok)
        rr = [r for r in res if r[0] == R][0]
        return R, rr[1], rr[2], gs

    def straight_dev(self, A, B):
        """Max odleglosc punktow zrodlowych miedzy A i B od odcinka AB."""
        v = self.c["verts"]
        d = B - A
        L2 = float(d @ d)
        if L2 < 1e-9:
            return 0.0
        idx = self.tree.query_ball_point((A + B) / 2, r=math.sqrt(L2) / 2 + 1.0)
        if not idx:
            return 0.0
        q = v[idx] - A
        t = (q @ d) / L2
        m = (t > 0) & (t < 1)
        if not m.any():
            return 0.0
        return float(np.max(np.abs(q[m, 0] * d[1] - q[m, 1] * d[0]) / math.sqrt(L2)))

    def gamma(self, P, i):
        return wrap(heading(P[i], P[i + 1]) - heading(P[i - 1], P[i]))

    def t_min(self, P, i, c):
        """Styczna przy najkrotszej dopuszczalnej KP (P2) - do wykrywania konfliktow."""
        g = abs(self.gamma(P, i))
        _, _, _, _, var = curve_params(c.R, g, self.v_at(P[i]))
        L = min(var[-1][1], c.R * g)
        return tangent_length(c.R, L, g)

    def choose_L(self, P, i, c, room):
        """Najlepsza klasa KP (P0 -> P1 -> P2), ktora miesci sie w 'room'."""
        g = abs(self.gamma(P, i))
        V, D, I, Deq, var = curve_params(c.R, g, self.v_at(P[i]))
        c.V, c.D, c.I, c.Deq = V, D, I, Deq
        for cls, L in var:
            Lc = min(L, c.R * g)
            T = tangent_length(c.R, Lc, g)
            if T <= room + 1e-6:
                return cls, Lc, T, Lc < L - 1e-6
        # nic sie nie miesci: skracaj KP az styczna wejdzie
        cls, L = var[-1]
        Lc = min(L, c.R * g)
        while Lc > 1.0 and tangent_length(c.R, Lc, g) > room:
            Lc *= 0.9
        return cls, Lc, tangent_length(c.R, Lc, g), True

    def new_curve(self, P, i):
        R, rms, mx, _ = self.fit_pi(P, i)
        c = Curve()
        c.R_fit, c.R, c.rms, c.maxdev = R, R, rms, mx
        return c

    def build(self):
        verts = self.c["verts"]
        keep = douglas_peucker(verts, self.a.dp_eps)
        P = verts[keep].copy()
        self.n_pi_initial = len(P)
        # PI o pomijalnym kacie zwrotu -> usun
        while len(P) > 2:
            g = np.array([abs(self.gamma(P, i)) for i in range(1, len(P) - 1)])
            k = int(np.argmin(g))
            if g[k] >= math.radians(0.02):
                break
            P = np.delete(P, k + 1, axis=0)
        n = len(P)
        cur = [None] + [self.new_curve(P, i) for i in range(1, n - 1)] + [None]
        T = np.zeros(n)
        for i in range(1, n - 1):
            T[i] = self.t_min(P, i, cur[i])

        def vmin_leg(j):
            return min(self.v_at(P[j]), self.v_at(P[j + 1]))

        # --- konflikty: suma stycznych + prosta min. (P2) > dlugosc ramienia
        def t0(k):
            """Styczna bez krzywych przejsciowych (absolutne minimum dla danego R)."""
            if not (0 < k < len(P) - 1):
                return 0.0
            return tangent_length(cur[k].R, 0.0, abs(self.gamma(P, k)))

        def refit(ks, extra_flag):
            for m in ks:
                if 0 < m < len(P) - 1:
                    fl = cur[m].flags
                    cur[m] = self.new_curve(P, m)
                    cur[m].flags = sorted(set(fl) | {extra_flag})
                    T[m] = self.t_min(P, m, cur[m])

        def delete_pi(k, flag):
            nonlocal P, T, relaxed
            P = np.delete(P, k, axis=0)
            relaxed = {x if x < k else x - 1 for x in relaxed if x not in (k - 1, k)}
            del cur[k]
            T = np.delete(T, k)
            refit((k - 1, k), flag)

        def flag(k, f):
            if 0 < k < len(P) - 1 and f not in cur[k].flags:
                cur[k].flags.append(f)

        no_merge = set()
        relaxed = set()   # ramiona, na ktorych dopuszczono brak prostej / skrocone KP
        for _ in range(50 * n + 200):
            n = len(P)
            legs = np.hypot(*(np.diff(P, axis=0).T))
            over = np.array([T[j] + T[j + 1] + (l_min_straight(vmin_leg(j), "P2") if 0 < j < n - 2 else 0.0)
                             - legs[j] for j in range(n - 1)])
            for j in relaxed:
                if j < len(over):
                    over[j] = -1.0
            j = int(np.argmax(over))
            if over[j] <= 1e-6:
                break
            gj = self.gamma(P, j) if 0 < j else 0.0
            gk = self.gamma(P, j + 1) if j + 1 < n - 1 else 0.0
            same = 0 < j and j + 1 < n - 1 and gj * gk > 0
            # 1) luki zgodnie skierowane -> jeden PI na przecieciu ramion zewnetrznych
            if same and abs(gj + gk) < math.radians(120) and (j, tuple(P[j])) not in no_merge:
                A, u = P[j - 1], P[j] - P[j - 1]
                B, w = P[j + 2], P[j + 1] - P[j + 2]
                den = u[0] * w[1] - u[1] * w[0]
                t = ((B - A)[0] * w[1] - (B - A)[1] * w[0]) / den
                P2 = np.delete(P, j + 1, axis=0)
                P2[j] = A + u * t
                trial = self.new_curve(P2, j)
                if np.isfinite(trial.maxdev) and trial.maxdev <= self.a.merge_tol:
                    fl = sorted(set(cur[j].flags) | set(cur[j + 1].flags) | {"scalony"})
                    P = P2
                    relaxed = {x if x < j else x - 1 for x in relaxed if x not in (j - 1, j, j + 1)}
                    del cur[j + 1]
                    T = np.delete(T, j + 1)
                    cur[j] = trial
                    cur[j].flags = fl
                    T[j] = self.t_min(P, j, cur[j])
                    continue
                no_merge.add((j, tuple(P[j])))
                self.rejected.append((float(np.degrees(gj + gk)), trial.R, trial.maxdev, trial.rms))
            # 2) szum (luki odwrotne o malym kacie) -> prostowanie w tolerancji
            cand = sorted((self.straight_dev(P[k - 1], P[k + 1]), k) for k in (j, j + 1) if 0 < k < n - 1)
            small = cand and abs(self.gamma(P, cand[0][1])) < math.radians(3.0)
            if (not same or small) and cand and cand[0][0] <= self.a.straighten_tol:
                delete_pi(cand[0][1], "wyprostowany")
                continue
            # 3) prawdziwa geometria: bez prostej, KP skrocone (luk koszowy / esowy)
            if t0(j) + t0(j + 1) <= legs[j]:
                relaxed.add(j)
                for k in (j, j + 1):
                    flag(k, "luk_koszowy" if same else "bez_prostej")
                continue
            # 4) zmniejsz R (nie ponizej Rmin P2)
            changed = False
            for k in (j, j + 1):
                if 0 < k < n - 1 and cur[k].R > A6["Rmin"]["P2"] + 1e-6:
                    cur[k].R = max(A6["Rmin"]["P2"], cur[k].R * 0.92)
                    flag(k, "R_zmniejszony")
                    T[k] = self.t_min(P, k, cur[k])
                    changed = True
            if changed:
                continue
            # 5) ostatecznosc: prostowanie mimo odchylki
            if cand:
                k = cand[0][1]
                self.log.append(f"PI {k}: wyprostowano z odchylka {cand[0][0]:.1f} m (brak miejsca na luki)")
                delete_pi(k, "wyprostowany_ponad_tol")
            else:
                relaxed.add(j)

        # --- przydzial miejsca na ramionach i ostateczny dobor KP
        n = len(P)
        legs = np.hypot(*(np.diff(P, axis=0).T))
        need = np.zeros(n)
        tlo = np.array([t0(i) for i in range(n)])
        for i in range(1, n - 1):
            need[i] = self.choose_L(P, i, cur[i], 1e12)[2]       # styczna dla P0
        room = np.full(n, 1e12)
        for j in range(n - 1):
            inner = 0 < j < n - 2
            lmin = 0.0
            if inner:
                for cls_s in ("P1", "P2"):
                    lm = l_min_straight(vmin_leg(j), cls_s)
                    if T[j] + T[j + 1] + lm <= legs[j]:
                        lmin = lm
                        break
                else:
                    cls_s = None
                if cls_s != "P1":
                    for k in (j, j + 1):
                        flag(k, "prosta_P2" if cls_s == "P2" else "prosta<P2")
            avail = legs[j] - lmin
            if need[j] + need[j + 1] > avail:
                extra = max(0.0, avail - tlo[j] - tlo[j + 1])
                want = np.array([need[j] - tlo[j], need[j + 1] - tlo[j + 1]])
                share = extra * want / want.sum() if want.sum() > 0 else np.zeros(2)
                room[j] = min(room[j], tlo[j] + share[0])
                room[j + 1] = min(room[j + 1], tlo[j + 1] + share[1])
        Tf = np.zeros(n)
        for i in range(1, n - 1):
            c = cur[i]
            cls, L, Ti, short = self.choose_L(P, i, c, room[i])
            if short:
                flag(i, "KP_skrocona")
            c.cls, c.L = cls, L
            Tf[i] = Ti
        # zabezpieczenie: styczne nie moga na siebie zachodzic (ujemna prosta)
        for _ in range(200):
            bad = [j for j in range(n - 1) if Tf[j] + Tf[j + 1] > legs[j] + 1e-3]
            if not bad:
                break
            for j in bad:
                for k in (j, j + 1):
                    if 0 < k < n - 1:
                        c = cur[k]
                        g = abs(self.gamma(P, k))
                        c.L = 0.0
                        lim = legs[j] * (Tf[k] / (Tf[j] + Tf[j + 1]))
                        R_fit = lim / math.tan(g / 2.0) if g > 1e-9 else c.R
                        if R_fit < c.R:
                            c.R = R_fit
                            V, D, I, Deq = curve_speed_and_cant(c.R, self.v_at(P[k]))
                            c.V, c.D, c.I, c.Deq = V, D, I, Deq
                        flag(k, "R_wymuszony_geometria")
                        flag(k, "KP_skrocona")
                        Tf[k] = tangent_length(c.R, 0.0, g)
        for i in range(1, n - 1):
            if cur[i].R < A6["Rmin"]["P2"] - 1e-6:
                flag(i, "R<Rmin_P2")
        curves_out = [(i, cur[i]) for i in range(1, n - 1)]
        straights = [legs[i - 1] - Tf[i - 1] - Tf[i] for i in range(1, n)]   # prosta przed PI i
        for i, st in enumerate(straights, start=1):
            if st < -0.05:
                self.log.append(f"ujemna prosta {st:.2f} m przed PI {i}")
        sg = [0.0] + [math.copysign(1.0, self.gamma(P, i)) for i in range(1, n - 1)] + [0.0]
        # most przechylkowy miedzy lukami zgodnymi bez pelnej prostej (luk koszowy):
        # przechylka przechodzi liniowo D_j -> D_k zamiast spadac do zera
        bridged = [False] * n          # bridged[i]: prosta przed PI i jest mostem
        for i in range(2, n - 1):
            V = min(cur[i - 1].V, cur[i].V)
            if sg[i - 1] == sg[i] and straights[i - 1] < l_min_straight(V, "P2"):
                bridged[i] = True
        # przechylka ograniczona dlugoscia rampy (= KP) na koncach bez mostu (tab. 9.12 P2)
        for i in range(1, n - 1):
            c = cur[i]
            ends = [x for x, b in ((c.L, bridged[i]), (c.L, i + 1 < n and bridged[i + 1])) if not b]
            if ends and c.D > 0:
                Dr = A6["dDds"]["P2"] * min(ends)
                if Dr < c.D:
                    D = floor5(Dr)
                    if D < A6["Dmin_nonzero"]:
                        D = 0.0
                    c.D = D
                    c.I = c.Deq - D
                    if c.I > A6["Idop"]["P1"]:
                        c.V = max(10.0, min(c.V, 10.0 * math.floor(v_allowed(c.R, D) / 10.0)))
                        c.Deq = 11.8 * c.V ** 2 / c.R
                        c.I = c.Deq - D
                    flag(i, "D_ograniczona_rampa")
        # rampa na moscie (KP wyj. + prosta + KP wej.) tez musi spelnic dD/ds <= P2
        def limit_D(c, i, D):
            D = floor5(max(0.0, D))
            if D < A6["Dmin_nonzero"]:
                D = 0.0
            if D < c.D:
                c.D = D
                c.I = c.Deq - D
                if c.I > A6["Idop"]["P1"]:
                    c.V = max(10.0, min(c.V, 10.0 * math.floor(v_allowed(c.R, D) / 10.0)))
                    c.Deq = 11.8 * c.V ** 2 / c.R
                    c.I = c.Deq - D
                flag(i, "D_ograniczona_rampa")
        for _ in range(n + 5):
            changed = False
            for i in range(2, n - 1):
                if not bridged[i]:
                    continue
                a, b = cur[i - 1], cur[i]
                Lb = a.L + max(0.0, straights[i - 1]) + b.L
                lim = A6["dDds"]["P2"] * Lb
                if abs(a.D - b.D) > lim + 1e-6:
                    if a.D > b.D:
                        limit_D(a, i - 1, b.D + lim)
                    else:
                        limit_D(b, i, a.D + lim)
                    changed = True
            if not changed:
                break
        els = []
        for i in range(1, n):
            st = max(0.0, straights[i - 1])
            if bridged[i]:
                a, b = cur[i - 1], cur[i]
                # poprzedni element to KP wyjsciowe luku i-1 (jesli jest)
                prev_kp = els[-1] if els and els[-1]["kind"] == "KP" and els[-1]["tag"] == f"pi{i-1}" else None
                Lb = (prev_kp["L"] if prev_kp else 0.0) + st + b.L
                Da, Db = a.D, b.D
                pos = 0.0
                if prev_kp:
                    prev_kp["D0"] = Da
                    prev_kp["D1"] = Da + (Db - Da) * prev_kp["L"] / Lb if Lb > 0 else Db
                    pos = prev_kp["L"]
                d0 = Da + (Db - Da) * pos / Lb if Lb > 0 else Da
                d1 = Da + (Db - Da) * (pos + st) / Lb if Lb > 0 else Db
                els.append(dict(kind="P", L=st, k0=0.0, k1=0.0, D0=d0, D1=d1, csign=sg[i],
                                V=min(a.V, b.V), tag=f"pi{i}"))
                cel = curve_elements(b.R, b.L, self.gamma(P, i), b.D, b.V, f"pi{i}", sg[i])
                if cel and cel[0]["kind"] == "KP":
                    cel[0]["D0"] = d1
                els += cel
                continue
            els.append(dict(kind="P", L=st, k0=0.0, k1=0.0, D0=0.0, D1=0.0, csign=0.0,
                            V=None, tag=f"pi{i}"))
            if i < n - 1:
                c = cur[i]
                els += curve_elements(c.R, c.L, self.gamma(P, i), c.D, c.V, f"pi{i}", sg[i])
        self.PI, self.curves, self.els = P, curves_out, els
        return self
# ----------------------------------------------------------------------------
# kawalki torow + eksport
# ----------------------------------------------------------------------------
def handle_len(L, dh):
    if abs(dh) < 1e-9:
        return L / 3.0
    return L * (4.0 / 3.0) * math.tan(abs(dh) / 4.0) / abs(dh)


def pieces_from_elements(p0, h0, els, max_straight, max_arc, max_kp, min_len=1.0):
    out = []
    p, h = np.array(p0, float), h0
    pending = None      # krotki element (< min_len) doklejany do nastepnego kawalka
    for e in els:
        L = e["L"]
        if L <= 1e-3:
            continue
        if L < min_len:
            s_all, x, y, hh = integrate(p, h, L, e["k0"], e["k1"], 9)
            if pending is None:
                pending = dict(p0=p.copy(), h0=h, L=0.0, D0=e["D0"])
            pending["L"] += L
            p, h = np.array([x[-1], y[-1]]), hh[-1]
            continue
        lim = {"P": max_straight, "LUK": max_arc, "KP": max_kp}[e["kind"]]
        if e["kind"] == "LUK":
            lim = min(lim, math.radians(10) / abs(e["k0"]))
        npc = max(1, int(math.ceil(L / lim)))
        s_all, x, y, hh = integrate(p, h, L, e["k0"], e["k1"], max(npc * 20 + 1, int(L / 0.25) + 1))
        idx = np.round(np.linspace(0, len(s_all) - 1, npc + 1)).astype(int)
        for a, b in zip(idx[:-1], idx[1:]):
            sa, sb = s_all[a], s_all[b]
            f = lambda s: e["k0"] + (e["k1"] - e["k0"]) * s / L
            g = lambda s: e["D0"] + (e["D1"] - e["D0"]) * s / L
            pc = dict(
                kind=e["kind"], L=sb - sa,
                p0=np.array([x[a], y[a]]), h0=hh[a], p1=np.array([x[b], y[b]]), h1=hh[b],
                k0=f(sa), k1=f(sb), D0=g(sa), D1=g(sb), V=e["V"], tag=e["tag"],
                ksign=e.get("csign", np.sign(e["k0"] + e["k1"])))
            if pending is not None:
                pc.update(p0=pending["p0"], h0=pending["h0"], L=pc["L"] + pending["L"],
                          D0=pending["D0"], kind="LUK" if pc["kind"] == "P" else pc["kind"])
                pending = None
            out.append(pc)
        p, h = np.array([x[-1], y[-1]]), hh[-1]
    if pending is not None and out:
        last = out[-1]
        last.update(p1=p.copy(), h1=h, L=last["L"] + pending["L"],
                    kind="LUK" if last["kind"] == "P" else last["kind"])
    return out


def smooth_speed(pcs, min_run):
    """Krotkie odcinki o wyzszej predkosci miedzy ograniczeniami (< min_run m)
    dostaja predkosc wyzszego z sasiadow - jak przy rzeczywistym rozkladzie
    wskaznikow W8/W9, ktorych nie stawia sie co 200 m."""
    for _ in range(100):
        runs = []
        for i, pc in enumerate(pcs):
            if runs and runs[-1][2] == pc["V"]:
                runs[-1][1] = i
                runs[-1][3] += pc["L"]
            else:
                runs.append([i, i, pc["V"], pc["L"]])
        changed = False
        for k in range(1, len(runs) - 1):
            a, b, v, L = runs[k]
            vp, vn = runs[k - 1][2], runs[k + 1][2]
            if v > vp and v > vn and L < min_run:
                for i in range(a, b + 1):
                    pcs[i]["V"] = max(vp, vn)
                changed = True
        if not changed:
            break


def roll_deg(D, ksign):
    """D [mm] na bazie 1500 mm. Luk w lewo (k>0) -> podniesiony prawy tok -> roll ujemny."""
    if D <= 0 or ksign == 0:
        return 0.0
    return -ksign * math.degrees(math.asin(D / 1500.0))


def fmtc(v):
    s = f"{v:.6f}"
    return "0" if s in ("0.000000", "-0.000000") else s.rstrip("0").rstrip(".")


def fmt(v):
    s = f"{v:.4f}"
    return "0" if s in ("0.0000", "-0.0000") else s.rstrip("0").rstrip(".")


def write_track(fh, name, pc, off, origin, args, V):
    def to_ms(xy):  # EPSG:2180 (E, N) -> MaSzyna (X=-E, Z=N)
        return (-(xy[0] - origin[0]), args.height, xy[1] - origin[1])

    n0 = np.array([-math.sin(pc["h0"]), math.cos(pc["h0"])])
    n1 = np.array([-math.sin(pc["h1"]), math.cos(pc["h1"])])
    P0 = pc["p0"] + off * n0
    P1 = pc["p1"] + off * n1
    dh = wrap(pc["h1"] - pc["h0"])
    L = max(0.01, pc["L"] - off * dh)
    straight = pc["kind"] == "P"
    if straight:
        c1 = c2 = (0.0, 0.0, 0.0)
        R = 0.0
    else:
        hl = handle_len(L, dh)
        t0 = np.array([math.cos(pc["h0"]), math.sin(pc["h0"])]) * hl
        t1 = np.array([math.cos(pc["h1"]), math.sin(pc["h1"])]) * hl
        c1 = (-t0[0], 0.0, t0[1])
        c2 = (t1[0], 0.0, -t1[1])
        kmean = dh / L if L > 0 else 0.0
        R = 0.0 if abs(kmean) < 1e-7 else 1.0 / abs(kmean)
    a = to_ms(P0)
    b = to_ms(P1)
    y0, y1 = pc.get("y0"), pc.get("y1")
    if y0 is not None:
        a = (a[0], y0, a[2])
        b = (b[0], y1, b[2])
        if not straight:
            slope = (y1 - y0) / L if L > 0 else 0.0
            c1 = (c1[0], slope * hl, c1[2])
            c2 = (c2[0], -slope * hl, c2[2])
    r0 = roll_deg(pc["D0"], pc["ksign"])
    r1 = roll_deg(pc["D1"], pc["ksign"])
    vis = (f"vis {args.rail_tex} {fmt(args.rail_tex_len)} {args.bed_tex} "
           f"{fmt(args.bed_height)} {fmt(args.bed_width)} {fmt(args.bed_slope)}")
    fh.write(f"node -1 0 {name} track normal {fmt(L)} 1.435 0.15 25 20 0 {pc.get('env', 'flat')} {vis}\n")
    fh.write(f" {fmt(a[0])} {fmt(a[1])} {fmt(a[2])} {fmt(r0)}\n")
    fh.write(f" {fmtc(c1[0])} {fmtc(c1[1])} {fmtc(c1[2])}\n")
    fh.write(f" {fmtc(c2[0])} {fmtc(c2[1])} {fmtc(c2[2])}\n")
    fh.write(f" {fmt(b[0])} {fmt(b[1])} {fmt(b[2])} {fmt(r1)}\n")
    fh.write(f" {fmt(R)}\n")
    fh.write(f" velocity {int(v_cap(V, R, pc['D0'], pc['D1']))}\nendtrack\n")


def v_cap(V, R, D0, D1, Idop=130.0, tol=5.0):
    """Predkosc na torze nie wieksza niz z niedomiaru I <= Idop (tab. 9.7), np. na laczach i za rozjazdami
    (bez przechylki). Tolerancja tol pomija przekroczenia z zaokraglen przechylki na krancach rampy."""
    if R <= 0 or V <= 0:
        return V
    Dm = min(D0, D1)
    if 11.8 * V * V / R - Dm <= Idop + tol:
        return V
    return max(20, int(math.sqrt((Dm + Idop) * R / 11.8) // 10 * 10))


def to_ms(xy, origin, y=0.0):
    return (-(xy[0] - origin[0]), y, xy[1] - origin[1])


def write_switch(fh, sw, origin, args):
    """Wpis 'track switch' jak editor_track::create_switch (dwie sciezki, uchwyty jak 'whole')."""
    P0 = to_ms(sw["P0"], origin, sw.get("y0", args.height))
    M = to_ms(sw["main_end"], origin, sw.get("yM", args.height))
    E = to_ms(sw["E"], origin, sw.get("yE", args.height))
    sl = (E[1] - P0[1]) / sw["length"]
    f = (-math.cos(sw["h"]), 0.0, math.sin(sw["h"]))
    d = (-math.cos(sw["hd"]), 0.0, math.sin(sw["hd"]))
    c1 = (f[0] * sw["h0"], sl * sw["h0"], f[2] * sw["h0"])
    c2 = (-d[0] * sw["h1"], -sl * sw["h1"], -d[2] * sw["h1"])
    fh.write(f"node -1 0 {sw['name']} track switch {fmt(sw['length'])} 1.435 0.15 10 20 0 flat vis "
             f"{args.rail_tex} {fmt(args.rail_tex_len)} {args.rail_tex} {fmt(args.bed_height)} {fmt(args.bed_width)} {fmt(args.bed_slope)}\n")
    fh.write(f" {fmt(P0[0])} {fmt(P0[1])} {fmt(P0[2])} 0\n 0 0 0\n 0 0 0\n {fmt(M[0])} {fmt(M[1])} {fmt(M[2])} 0\n 0\n")
    fh.write(f" {fmt(P0[0])} {fmt(P0[1])} {fmt(P0[2])} 0\n {fmt(c1[0])} {fmt(c1[1])} {fmt(c1[2])}\n {fmt(c2[0])} {fmt(c2[1])} {fmt(c2[2])}\n"
             f" {fmt(E[0])} {fmt(E[1])} {fmt(E[2])} 0\n {fmt(sw['R'])}\n")
    fh.write(f" trackbed {args.bed_tex} velocity -{int(sw['v_div'])}\nendtrack\n")


def write_switch_record(fh, sw, origin):
    """Rekord //$r w formacie editor/editorSwitchStore.cpp."""
    P0 = to_ms(sw["P0"], origin)
    dx, dz = -math.cos(sw["h"]), math.sin(sw["h"])
    name = sw["type"].split()[0]
    hand = "L" if sw["hand"] > 0 else "R"
    fh.write(f"//$r switch {sw['name']} {name} {sw['rail']} {sw['R']:.3f} {sw['n']:.3f} {hand} {sw['a']:.3f} {sw['b']:.3f} 0.000 "
             f"parts 1 {sw['name']} start {P0[0]:.4f} {P0[1]:.4f} {P0[2]:.4f} dir {dx:.9f} {dz:.9f} km - number - source designed\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("geojson")
    ap.add_argument("outdir")
    ap.add_argument("--lines", help="numery linii, np. 274,137 (domyslnie wszystkie)")
    ap.add_argument("--origin", help="E,N punktu 0 scenerii w EPSG:2180 (domyslnie srodek danych, zaokr. do 1 km)")
    ap.add_argument("--dp-eps", type=float, default=5.0, help="tolerancja upraszczania lamanej [m]")
    ap.add_argument("--straighten-tol", type=float, default=6.0,
                    help="max odchylka przy usuwaniu PI zamiast zmniejszania R [m]")
    ap.add_argument("--merge-tol", type=float, default=8.0,
                    help="max odchylka luku po scaleniu dwoch PI [m]")
    ap.add_argument("--fit-slack", type=float, default=1.0, help="zapas RMS przy wyborze najwiekszego R [m]")
    ap.add_argument("--track-spacing", type=float, default=4.0, help="rozstaw osi torow e [m] (wielokr. 0,05)")
    ap.add_argument("--default-speed", type=float, default=60, help="V dla odcinkow z v_k=0 (nieczynne)")
    ap.add_argument("--min-speed-run", type=float, default=500.0,
                    help="krotsze odcinki wyzszej predkosci miedzy ograniczeniami sa wyrownywane [m]")
    ap.add_argument("--height", type=float, default=0.0)
    ap.add_argument("--cache", action="store_true", help="zapisz/uzyj _cache.pkl z geometria (szybsze kolejne przebiegi)")
    ap.add_argument("--nmt", action="store_true", help="wysokosci z NMT GUGiK (usluga WCS geoportal.gov.pl)")
    ap.add_argument("--nmt-dir", help="katalog z arkuszami NMT (.asc/.tif, EPSG:2180) zamiast/obok uslugi")
    ap.add_argument("--nmt-res", type=float, default=2.0, help="rozdzielczosc pobieranego NMT [m]")
    ap.add_argument("--grade-tol", type=float, default=0.5, help="tolerancja niwelety wzgledem terenu przy wyznaczaniu zalaman [m]")
    ap.add_argument("--bridge-tol", type=float, default=2.5, help="teren dalej od niwelety (most, tunel, wykop) nie wplywa na nia [m]")
    ap.add_argument("--export-axes", help="zapisz uproszczone osie torow 1 (JSON) do pobrania NMT w przegladarce")
    ap.add_argument("--nmt-points", help="plik wysokosci terenu wzdluz osi (z przegladarki) zamiast uslugi WCS")
    ap.add_argument("--no-switches", action="store_true", help="bez rozjazdow i dowiazan")
    ap.add_argument("--rail-tex", default="rail_screw_used1")
    ap.add_argument("--rail-tex-len", type=float, default=6.0)
    ap.add_argument("--bed-tex", default="1435mm/tpbps-new2", help="podsypka (domyslna z edytora MaSzyny)")
    ap.add_argument("--terrain-nmt", help="plik NMT pasa wokol torow (z przegladarki) -> terrain/irt/")
    ap.add_argument("--terrain-base", action="store_true", help="zapisz tez HBAS (sam NMT pod modyfikatorami; paczki ~2x wieksze)")
    ap.add_argument("--natural-slopes", action="store_true", help="skarpy nie od linijki (teren.NATURAL): stopa faluje, skarpy miejscami lagodniejsze")
    ap.add_argument("--bed-height", type=float, default=0.2)
    ap.add_argument("--bed-width", type=float, default=0.5)
    ap.add_argument("--bed-slope", type=float, default=1.1)
    ap.add_argument("--max-straight", type=float, default=1000.0)
    ap.add_argument("--max-arc", type=float, default=100.0)
    ap.add_argument("--max-kp", type=float, default=10.0)
    args = ap.parse_args()
    e = round(args.track_spacing / 0.05) * 0.05            # 9.13 wzor 9.25
    args.track_spacing = e

    lines = set(s.strip() for s in args.lines.split(",")) if args.lines else None
    chains, stations = load_chains(args.geojson, lines)
    os.makedirs(args.outdir, exist_ok=True)
    allv = np.vstack([c["verts"] for c in chains])
    if args.origin:
        origin = tuple(float(v) for v in args.origin.split(","))
    else:
        origin = tuple(np.round(allv.mean(axis=0), -3))

    rep = open(os.path.join(args.outdir, "raport_lukow.csv"), "w", newline="", encoding="utf-8")
    rw = csv.writer(rep, delimiter=";")
    rw.writerow(["lancuch", "PI", "E", "N", "kat_zwrotu_deg", "kierunek", "R_dopasowany", "R",
                 "L_KP", "klasa_KP", "D_mm", "I_mm", "V_linii", "V_luku", "rms_m", "max_odch_m",
                 "spelnia_Rmin_P0", "uwagi"])
    summary = []
    geo_out = {"type": "FeatureCollection", "name": "os_po_korekcji",
               "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::2180"}},
               "features": []}
    master = open(os.path.join(args.outdir, "irt_sceneria.inc"), "w", encoding="utf-8")
    master.write(f"// tory z IRT po korekcji ST-T1-A6; origin EPSG:2180 E={origin[0]:.0f} N={origin[1]:.0f}\n")
    master.write(f"//$g PUWG1992 {origin[0]/1000:.3f} {origin[1]/1000:.3f}\n")

    import pickle
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import uklad
    cache = os.path.join(args.outdir, "_cache.pkl") if args.cache else None
    if cache and os.path.exists(cache):
        built = pickle.load(open(cache, "rb"))
    else:
        built = []
        for c in chains:
            al = Aligner(c, args).build()
            pcs = pieces_from_elements(al.PI[0], heading(al.PI[0], al.PI[1]), al.els,
                                       args.max_straight, args.max_arc, args.max_kp)
            for pc in pcs:
                mid = 0.5 * (pc["p0"] + pc["p1"])
                at = al.attrs_at(mid)
                vline = float(at.get("v_k") or 0) or float(args.default_speed)
                pc["V"] = min(vline, pc["V"]) if pc["V"] is not None else vline
                pc["NT"] = int(at.get("NT") or 1)
            smooth_speed(pcs, args.min_speed_run)
            dense_pts = []
            for pc in pcs:
                k = max(2, int(pc["L"] / 2) + 1)
                if pc["kind"] == "P":
                    dense_pts.append(np.linspace(pc["p0"], pc["p1"], k))
                else:
                    _, x, y, _ = integrate(pc["p0"], pc["h0"], pc["L"], pc["k0"], pc["k1"], k)
                    dense_pts.append(np.column_stack([x, y]))
            dense = np.vstack(dense_pts)
            dev, _ = cKDTree(dense).query(c["verts"])
            end_gap = float(np.hypot(*(pcs[-1]["p1"] - c["verts"][-1])))
            rows = []
            for i, cv in al.curves:
                P = al.PI
                gs = wrap(heading(P[i], P[i + 1]) - heading(P[i - 1], P[i]))
                vline = al.v_at(P[i])
                rows.append([c["name"], i, f"{P[i][0]:.2f}", f"{P[i][1]:.2f}", f"{math.degrees(abs(gs)):.3f}",
                             "L" if gs > 0 else "P", f"{cv.R_fit:.0f}", f"{cv.R:.0f}", f"{cv.L:.0f}", cv.cls,
                             f"{cv.D:.0f}", f"{cv.I:.0f}", f"{vline:.0f}", f"{cv.V:.0f}",
                             f"{cv.rms:.2f}", f"{cv.maxdev:.2f}",
                             "tak" if cv.R >= A6["Rmin"]["P0"] else "nie", ",".join(cv.flags)])
            nlim = sum(1 for _, cv in al.curves if cv.V < al.v_at(al.PI[_]) - 1e-6)
            summ = dict(chain=c["name"], LN=c["LN"], km_s=c["attrs"][0]["km_s"], km_e=c["attrs"][-1]["km_e"],
                        dl_m=round(float(c["s"][-1])), usuniete_wasy=c["spikes"], PI_po_DP=al.n_pi_initial,
                        luki=len(al.curves), odch_srednia_m=round(float(dev.mean()), 2),
                        odch_max_m=round(float(dev.max()), 2), odch_p95_m=round(float(np.percentile(dev, 95)), 2),
                        koniec_odch_m=round(end_gap, 2), luki_z_ogr_V=nlim, ostrzezenia=al.log)
            built.append(dict(name=c["name"], LN=c["LN"], km=(c["attrs"][0]["km_s"], c["attrs"][-1]["km_e"]),
                              pcs=pcs, rows=rows, summ=summ, dense=np.round(dense[::5], 2)))
        if cache:
            pickle.dump(built, open(cache, "wb"))

    # --- uklad: tor 2, rozjazdy, dowiazania
    lay_log = []
    lay = uklad.Layout(e, lay_log)
    t1 = {}
    for b in built:
        t1[b["name"]] = lay.add(uklad.Run(f"{b['name']}_t1", "t1", b["name"], [dict(p) for p in b["pcs"]], b["name"]))
    if not args.no_switches:
        lay.shared_stretches(list(t1.values()))
        for run in list(t1.values()):
            lay.double_track(run, e)
        mains = [r for r in lay.runs.values() if r.kind in ("t1", "t2")]
        done_pairs = set()
        resolved = set()
        for run in list(t1.values()):
            for at_end in (False, True):
                if lay.junction(run, at_end, [r for r in mains if r.chain != run.chain]):
                    resolved.add((run.name, at_end))
        for run in list(t1.values()):
            for at_end in (False, True):
                if (run.name, at_end) in resolved or (run.name, at_end) in lay.forks:
                    continue
                P = run.pieces[-1]["p1"] if at_end else run.pieces[0]["p0"]
                best = None
                for other in t1.values():
                    if other is run:
                        continue
                    for oe in (False, True):
                        if (other.name, oe) in resolved or (other.name, oe) in lay.forks:
                            continue
                        Q = other.pieces[-1]["p1"] if oe else other.pieces[0]["p0"]
                        d = float(np.hypot(*(Q - P)))
                        if d < 30.0 and (best is None or d < best[0]):
                            best = (d, other, oe)
                if not best:
                    continue
                key = frozenset([(run.name, at_end), (best[1].name, best[2])])
                if key in done_pairs:
                    continue
                done_pairs.add(key)
                V = min(run.pieces[-1 if at_end else 0]["V"], best[1].pieces[-1 if best[2] else 0]["V"])
                res = lay.end_join(run, at_end, best[1], best[2], V)
                if res:
                    resolved.add((run.name, at_end))
                    resolved.add((best[1].name, best[2]))
                lay_log.append(dict(typ="polaczenie_koniec_koniec" if res else "polaczenie_pominiete",
                                    gdzie=f"{run.chain}->{best[1].chain}", s=0,
                                    uwagi=res or "konce rozchodza sie albo luk R>=180 m sie nie miesci"))

        # tor 2 na przedluzeniach linii (koniec-koniec): dowiaz takze tor 2
        t2runs = [r for r in lay.runs.values() if r.kind == "t2"]
        used = set()
        for r in t2runs:
            for at_end in (False, True):
                if (r.name, at_end) in used:
                    continue
                P = r.pieces[-1]["p1"] if at_end else r.pieces[0]["p0"]
                best = None
                for o in t2runs:
                    if o.chain == r.chain:
                        continue
                    for oe in (False, True):
                        if (o.name, oe) in used:
                            continue
                        Q = o.pieces[-1]["p1"] if oe else o.pieces[0]["p0"]
                        d = float(np.hypot(*(Q - P)))
                        if d < 15.0 and (best is None or d < best[0]):
                            best = (d, o, oe)
                if best:
                    V = min(r.pieces[-1 if at_end else 0]["V"], best[1].pieces[-1 if best[2] else 0]["V"])
                    res = lay.end_join(r, at_end, best[1], best[2], V)
                    if res:
                        used.add((r.name, at_end))
                        used.add((best[1].name, best[2]))
                        lay_log.append(dict(typ="polaczenie_tor2", gdzie=f"{r.name}->{best[1].name}", s=0, uwagi=res))
        n_cut = lay.resolve_overlaps()
        print(f"cofniete konce na cudzych stykach: {n_cut}")
    if args.export_axes:
        axes = []
        for r in lay.runs.values():
            if r.kind != "t1":
                continue
            pts, ss = uklad.Layout.dense(r, 1.0)
            keep = douglas_peucker(pts, 0.25)
            axes.append(dict(name=r.name, pts=np.round(pts[keep], 1).tolist()))
        json.dump(axes, open(args.export_axes, "w"))
        print("osie:", len(axes), "wierzcholkow:", sum(len(a["pts"]) for a in axes))
    if args.nmt or args.nmt_dir or args.nmt_points:
        import profil
        if args.nmt_points:
            nmt = profil.PointCloud(args.nmt_points)
        else:
            nmt = profil.NMT(os.path.join(args.outdir, "_nmt"), args.nmt_res, args.nmt_dir)
            if not args.nmt:
                nmt.fetch = lambda k: None
        lt = {}
        for c in chains:
            fr = any(a.get("FS") == "+" for a in c["attrs"])
            ps_ = any(a.get("PS") == "+" for a in c["attrs"])
            lt[c["name"]] = "P" if (ps_ and not fr) else "M"
        prof_rows = []
        uklad.apply_profile(lay, [r for r in lay.runs.values() if r.kind == "t1"], nmt,
                            grade_tol=args.grade_tol, bridge_tol=args.bridge_tol, log=lay_log,
                            line_types=lt, stations=stations, rows=prof_rows)
        json.dump(prof_rows, open(os.path.join(args.outdir, "raport_profilu.json"), "w", encoding="utf-8"),
                  ensure_ascii=False, indent=1)
        uklad.mark_crossings(lay, log=lay_log)

    # --- zapis
    def wr_runs(fh, runs):
        for r in runs:
            k = 0
            for pc in r.pieces:
                if pc.get("switch"):
                    continue
                write_track(fh, f"{r.name}_{k:05d}", pc, 0.0, origin, args, pc["V"])
                k += 1

    by_chain = defaultdict(list)
    for r in lay.runs.values():
        by_chain[r.file].append(r)
    # cale torowisko w jednym pliku: rekordy //$r, rozjazdy, potem tory linia po linii
    with open(os.path.join(args.outdir, "irt_tory.inc"), "w", encoding="utf-8") as fh:
        fh.write("// torowisko Dolnego Slaska z IRT (Geoportal Dolnego Slaska), geometria wg PKP PLK ST-T1-A6 (17.04.2025)\n")
        fh.write(f"// origin EPSG:2180 E={origin[0]:.0f} N={origin[1]:.0f}; X = -(E - E0), Z = N - N0, Y = m n.p.m.\n")
        fh.write(f"//$g PUWG1992 {origin[0]/1000:.3f} {origin[1]/1000:.3f}\n")
        fh.write(f"// podsypka: {args.bed_tex} (profil 0,2 / 0,5 / 1,1 jak w edytorze), szyny {args.rail_tex}\n")
        if lay.switches:
            fh.write("//$r # switches placed in the track editor, the simulation skips these lines\n")
        for sw in lay.switches:
            write_switch_record(fh, sw, origin)
        fh.write("// ---- rozjazdy\n")
        for sw in lay.switches:
            write_switch(fh, sw, origin, args)
        fh.write("// ---- laczniki i dowiazania\n")
        wr_runs(fh, [r for r in lay.runs.values() if r.file == "rozjazdy"])
        for b in built:
            fh.write(f"// ---- linia {b['LN']} ({b['name']}), km {b['km'][0]}-{b['km'][1]}\n")
            wr_runs(fh, by_chain[b["name"]])
    master.write("include irt_tory.inc end\n")
    if args.terrain_nmt:
        master.write("heightmap_terrain irt endheightmap_terrain\n")
    for b in built:
        for row in b["rows"]:
            rw.writerow(row)
        summ = dict(b["summ"])
        summ["tory"] = sum(1 for r in by_chain[b["name"]] for p in r.pieces if not p.get("switch"))
        summary.append(summ)
        geo_out["features"].append({"type": "Feature", "properties": {"chain": b["name"], "LN": b["LN"]},
                                    "geometry": {"type": "LineString", "coordinates": b["dense"].tolist()}})
    if args.terrain_nmt:
        import teren
        runs = []
        for r in lay.runs.values():
            seg = []
            for p in r.pieces:
                if p.get("switch"):
                    if seg:
                        runs.append(seg)
                    seg = []
                    continue
                if "y0" in p:
                    seg.append(p)
            if seg:
                runs.append(seg)
        for sw in lay.switches:
            if "y0" not in sw:
                continue
            for end, yk, hk in (("main_end", "yM", "h"), ("E", "yE", "hd")):
                P0, P1 = sw["P0"], sw[end]
                runs.append([dict(kind="P", L=float(np.hypot(*(P1 - P0))), p0=P0, p1=P1, h0=sw["h"], h1=sw[hk],
                                  y0=sw["y0"], y1=sw[yk], env="flat")])
        grid = teren.Grid(args.terrain_nmt)
        st = teren.build(grid, runs, origin, os.path.join(args.outdir, "terrain", "irt"), log=print,
                         with_base=args.terrain_base, natural=teren.NATURAL if args.natural_slopes else None)
        print("teren:", st)
    with open(os.path.join(args.outdir, "raport_ukladu.csv"), "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter=";")
        w.writerow(["typ", "gdzie", "s_m", "nazwa", "rozjazd", "uwagi"])
        for x in lay_log:
            w.writerow([x.get("typ"), x.get("gdzie"), x.get("s"), x.get("nazwa", ""), x.get("rozjazd", ""), x.get("uwagi", "")])
    print(f"rozjazdow: {len(lay.switches)}, wpisow ukladu: {len(lay_log)}")
    master.close()
    rep.close()
    json.dump(geo_out, open(os.path.join(args.outdir, "os_po_korekcji.geojson"), "w"))
    json.dump(dict(origin_EPSG2180=origin, track_spacing_m=e, chains=summary),
              open(os.path.join(args.outdir, "podsumowanie.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    with open(os.path.join(args.outdir, "stacje.txt"), "w", encoding="utf-8") as fh:
        fh.write("// nazwa; typ; X; Z (uklad MaSzyny wzgledem origin)\n")
        for p, xy in stations:
            fh.write(f"{p.get('SN')}; {p.get('TYPE')}; {-(xy[0] - origin[0]):.1f}; {xy[1] - origin[1]:.1f}\n")
    print(f"lancuchow: {len(summary)}, origin {origin}, e={e:.2f} m")


if __name__ == "__main__":
    main()
