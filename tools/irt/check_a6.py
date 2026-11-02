#!/usr/bin/env python3
"""Niezalezna kontrola wyjsciowych torow wzgledem ST-T1-A6 (tylko z tego, co jest w .inc)."""
import sys, math, glob
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import check_inc as C
viol = {"dD/ds>2.5": 0, "D>150": 0, "I>130": 0, "R<180": 0}
worst = {"dD/ds": 0.0, "I": -1e9}
n = 0
for f in sys.argv[1:]:
    for t in C.parse(f):
        n += 1
        D1 = 1500 * math.sin(math.radians(abs(t["r1"])))
        D2 = 1500 * math.sin(math.radians(abs(t["r2"])))
        if math.copysign(1, t["r1"]) != math.copysign(1, t["r2"]) and D1 > 0 and D2 > 0:
            dD = D1 + D2
        else:
            dD = abs(D2 - D1)
        sl = dD / max(t["L"], 1e-6)
        worst["dD/ds"] = max(worst["dD/ds"], sl)
        if sl > 2.5 + 0.05:
            viol["dD/ds>2.5"] += 1
        if max(D1, D2) > 150.5:
            viol["D>150"] += 1
        if t["R"] > 0:
            V = float(t.get("velocity", 0))
            I = 11.8 * V * V / t["R"] - min(D1, D2)
            worst["I"] = max(worst["I"], I)
            if I > 130.5:
                viol["I>130"] += 1
            if t["R"] < 179.5 and abs(t["r1"]) > 0:
                viol["R<180"] += 1
print(f"torow {n}; naruszenia: {viol}; najgorsze: dD/ds={worst['dD/ds']:.2f} mm/m, I={worst['I']:.0f} mm")
