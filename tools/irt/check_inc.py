#!/usr/bin/env python3
"""Kontrola pliku .inc z torami MaSzyny: skladnia jak w TTrack::Load (track normal),
ciaglosc polozenia, kierunku i przechylki miedzy kolejnymi torami tego samego toru."""
import math
import re
import sys
from collections import defaultdict


def parse(path):
    toks = []
    for line in open(path, encoding="utf-8"):
        line = line.split("//")[0]
        toks += line.split()
    i, out = 0, []
    while i < len(toks):
        assert toks[i] == "node", f"oczekiwano 'node' przy tokenie {i}: {toks[i]}"
        rmax, rmin, name, typ, sub = toks[i + 1:i + 6]
        assert typ == "track", (name, typ, sub)
        if sub == "switch":              # rozjazdy sprawdza network()
            while toks[i] != "endtrack":
                i += 1
            i += 1
            continue
        L, w, fr, sd, q, dmg, env, vis = toks[i + 6:i + 14]
        assert env in ("flat", "mountains", "canyon", "tunnel", "bridge", "bank")
        j = i + 14
        if vis == "vis":
            j += 6
        nums = [float(x) for x in toks[j:j + 15]]
        p1, r1 = nums[0:3], nums[3]
        c1, c2 = nums[4:7], nums[7:10]
        p2, r2 = nums[10:13], nums[13]
        rad = nums[14]
        j += 15
        attrs = {}
        while toks[j] != "endtrack":
            attrs[toks[j]] = toks[j + 1]
            j += 2
        out.append(dict(name=name, L=float(L), p1=p1, r1=r1, c1=c1, c2=c2, p2=p2, r2=r2, R=rad, **attrs))
        i = j + 1
    return out


def main(path):
    tr = parse(path)
    groups = defaultdict(list)
    for t in tr:
        m = re.match(r"(.+_t\d)_(\d+)$", t["name"])
        groups[m.group(1)].append((int(m.group(2)), t))
    worst_gap = worst_ang = worst_roll = 0.0
    breaks = 0
    for g, lst in groups.items():
        lst.sort()
        for (ia, a), (ib, b) in zip(lst, lst[1:]):
            gap = math.dist(a["p2"], b["p1"])
            if ib != ia + 1 or gap > 0.5:
                breaks += 1              # celowa przerwa (np. koniec odcinka dwutorowego)
                continue
            worst_gap = max(worst_gap, gap)
            # kierunek na koncu a: -c2 (lub cieciwa dla prostego), na poczatku b: c1
            da = [-x for x in a["c2"]] if any(a["c2"]) else [q - p for p, q in zip(a["p1"], a["p2"])]
            db = b["c1"] if any(b["c1"]) else [q - p for p, q in zip(b["p1"], b["p2"])]
            ha = math.atan2(da[2], da[0])
            hb = math.atan2(db[2], db[0])
            ang = abs((hb - ha + math.pi) % (2 * math.pi) - math.pi)
            worst_ang = max(worst_ang, math.degrees(ang))
            worst_roll = max(worst_roll, abs(a["r2"] - b["r1"]))
    print(f"{path}: torow {len(tr)}, ciagow {len(groups)}, przerw {breaks}, "
          f"max szczelina {worst_gap * 1000:.1f} mm, max zalamanie {worst_ang:.4f} deg, "
          f"max skok roll {worst_roll:.4f} deg")
    return worst_gap, worst_ang, worst_roll


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)


def parse_any(path):
    """Tory zwykle i rozjazdy (dwie sciezki) z pliku .inc."""
    toks = []
    for line in open(path, encoding="utf-8"):
        toks += line.split("//")[0].split()
    i, out = 0, []
    while i < len(toks):
        name, typ, sub = toks[i + 3], toks[i + 4], toks[i + 5]
        vis = toks[i + 13]
        j = i + 14 + (6 if vis == "vis" else 0)
        paths = []
        for _ in range(2 if sub == "switch" else 1):
            nums = [float(x) for x in toks[j:j + 15]]
            paths.append(dict(p1=nums[0:3], c1=nums[4:7], c2=nums[7:10], p2=nums[10:13], R=nums[14]))
            j += 15
        while toks[j] != "endtrack":
            j += 1
        out.append(dict(name=name, sub=sub, paths=paths))
        i = j + 1
    return out


def ends_of(t):
    """(punkt, kierunek wychodzacy z toru w tym koncu)"""
    res = []
    for k, p in enumerate(t["paths"]):
        d1 = p["c1"] if any(p["c1"]) else [b - a for a, b in zip(p["p1"], p["p2"])]
        d2 = p["c2"] if any(p["c2"]) else [a - b for a, b in zip(p["p1"], p["p2"])]
        if k == 0 or t["sub"] != "switch":
            res.append((p["p1"], [-x for x in d1]))
        res.append((p["p2"], d2 and [-x for x in d2] if False else [-x for x in d2]))
    return res


def network(paths):
    import numpy as np
    from scipy.spatial import cKDTree
    pts, dirs, owner = [], [], []
    for f in paths:
        for t in parse_any(f):
            for p, d in ends_of(t):
                pts.append([p[0], p[2]])
                dirs.append(math.atan2(d[2], d[0]))
                owner.append(t["name"])
    pts = np.array(pts)
    tree = cKDTree(pts)
    free, kinks = [], []
    for i, p in enumerate(pts):
        nb = [j for j in tree.query_ball_point(p, 0.02) if owner[j] != owner[i]]
        if not nb:
            free.append(owner[i])
            continue
        j = nb[0]
        # wychodzacy z i oraz z j powinny byc przeciwne
        a = abs((dirs[i] - dirs[j] + math.pi) % (2 * math.pi) - math.pi)
        kinks.append((math.degrees(abs(math.pi - a)), owner[i], owner[j]))
    kinks.sort(reverse=True)
    print(f"koncow {len(pts)}, wolnych {len(free)}, max zalamanie na stykach {kinks[0][0]:.3f} deg ({kinks[0][1]} / {kinks[0][2]})")
    return free, kinks
