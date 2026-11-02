"""
uklad.py - uklad torow po korekcji: tor 2, rozjazdy i dowiazania miedzy liniami.

Rozjazdy wzorowane na edytorze MaSzyny (MaSzyna-EU07/maszyna, galezie maj00r / Hirek193):
  * katalog Rz jak w editor/editorSwitchDesign.cpp (R, 1:n, a, b),
  * tor zwrotny = luk R od poczatku rozjazdu + prosta do konca (b od punktu
    matematycznego), zapisany jedna krzywa Beziera z uchwytami 2/3 odleglosci do
    przeciecia stycznych - tak jak "whole" w edytorze,
  * wpis "track switch" jak editor_track::create_switch, predkosc ujemna
    (ogranicza tylko jazde na bok, world/Track.cpp),
  * rekord "//$r switch ..." w formacie editor/editorSwitchStore.cpp, zeby edytor
    rozpoznal rozjazd jako katalogowy.
Uklad wspolrzednych planu: (E, N) EPSG:2180; heading = atan2(dN, dE).
"""
import math
from collections import defaultdict
from dataclasses import dataclass, field

import numpy as np

# nazwa, szyna, R, n, a, b  (editorSwitchDesign.cpp)
CATALOGUE = {
    "Rz 60E1-300-1:9": ("60E1", 300.0, 9.0, 16.615, 16.615),
    "Rz 60E1-500-1:12": ("60E1", 500.0, 12.0, 20.797, 20.797),
    "Rz 60E1-1200-1:18,5": ("60E1", 1200.0, 18.5, 32.409, 32.409),
    "Rz 49E1-300-1:9": ("49E1", 300.0, 9.0, 16.615, 16.615),
    "Rz 49E1-500-1:12": ("49E1", 500.0, 12.0, 20.797, 20.797),
    "Rz 49E1-190-1:9": ("49E1", 190.0, 9.0, 10.523, 16.615),
}
# predkosc na bok wg promienia (PLK): 190/300 -> 40, 500 -> 60, 1200 -> 100
DIVERGING_SPEED = {190.0: 40, 300.0: 40, 500.0: 60, 760.0: 80, 1200.0: 100}


def pick_switch(v_line, v_branch=None):
    """Typ rozjazdu: szyna z predkosci linii, skos z predkosci jazdy na bok."""
    rail = "60E1" if v_line >= 120 else "49E1"
    vb = v_branch if v_branch is not None else v_line
    if v_branch is None:   # koniec dwutorowego: na bok jak najszybciej, ale rozsadnie
        vb = 100 if v_line >= 140 else (60 if v_line >= 80 else 40)
    if vb >= 100 and rail == "60E1":
        return "Rz 60E1-1200-1:18,5"
    if vb >= 60:
        return f"Rz {rail}-500-1:12"
    return f"Rz {rail}-300-1:9"


def wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def hvec(h):
    return np.array([math.cos(h), math.sin(h)])


def integrate_at(p0, h0, L, k0, k1, t):
    """Punkt i kierunek na kawalku o krzywiznie liniowej, w odleglosci t od poczatku."""
    n = max(4, int(t / 0.5) + 2)
    s = np.linspace(0.0, t, n)
    h = h0 + k0 * s + (k1 - k0) * s * s / (2.0 * L if L > 0 else 1.0)
    if t <= 0:
        return np.array(p0, float), h0
    c, sn = np.cos(h), np.sin(h)
    ds = s[1] - s[0]
    x = p0[0] + np.sum(0.5 * (c[1:] + c[:-1])) * ds
    y = p0[1] + np.sum(0.5 * (sn[1:] + sn[:-1])) * ds
    return np.array([x, y]), float(h[-1])


# ----------------------------------------------------------------------------
# ciagi torow
# ----------------------------------------------------------------------------
@dataclass
class Run:
    name: str
    kind: str                 # t1, t2, conn, join
    chain: str
    pieces: list
    file: str
    removed: list = field(default_factory=list)   # przedzialy s zastapione rozjazdem

    def s(self):
        return np.concatenate([[0.0], np.cumsum([p["L"] for p in self.pieces])])

    def length(self):
        return float(sum(p["L"] for p in self.pieces))


def split_piece(pc, t):
    """Dzieli kawalek w odleglosci t: dwa kawalki o tych samych wlasnosciach."""
    L = pc["L"]
    if pc["kind"] == "P":
        pm = pc["p0"] + (pc["p1"] - pc["p0"]) * (t / L)
        hm = pc["h0"]
    else:
        # punkt na tej samej krzywej Beziera, ktora zapisujemy (uchwyty jak w write_track)
        dh = wrap(pc["h1"] - pc["h0"])
        hl = L / 3.0 if abs(dh) < 1e-9 else L * (4.0 / 3.0) * math.tan(abs(dh) / 4.0) / abs(dh)
        B0, B3 = np.array(pc["p0"], float), np.array(pc["p1"], float)
        B1, B2 = B0 + hvec(pc["h0"]) * hl, B3 - hvec(pc["h1"]) * hl
        u = np.linspace(0, 1, 401)[:, None]
        pts = (1 - u) ** 3 * B0 + 3 * (1 - u) ** 2 * u * B1 + 3 * (1 - u) * u ** 2 * B2 + u ** 3 * B3
        sl = np.concatenate([[0], np.cumsum(np.hypot(*np.diff(pts, axis=0).T))])
        uu = float(np.interp(t / L * sl[-1], sl, u[:, 0]))
        pm = (1 - uu) ** 3 * B0 + 3 * (1 - uu) ** 2 * uu * B1 + 3 * (1 - uu) * uu ** 2 * B2 + uu ** 3 * B3
        d = 3 * (1 - uu) ** 2 * (B1 - B0) + 6 * (1 - uu) * uu * (B2 - B1) + 3 * uu ** 2 * (B3 - B2)
        hm = pc["h0"] + wrap(math.atan2(d[1], d[0]) - pc["h0"])
    f = t / L
    km = pc["k0"] + (pc["k1"] - pc["k0"]) * f
    Dm = pc["D0"] + (pc["D1"] - pc["D0"]) * f
    a = dict(pc, L=t, p1=pm, h1=hm, k1=km, D1=Dm)
    b = dict(pc, L=L - t, p0=pm, h0=hm, k0=km, D0=Dm)
    return a, b


def split_at(run, s_cut, tol=0.05):
    """Zapewnia granice kawalkow w s_cut; zwraca indeks kawalka zaczynajacego sie w s_cut."""
    s = run.s()
    i = int(np.searchsorted(s, s_cut))
    if i < len(s) and abs(s[i] - s_cut) <= tol:
        return i
    if i > 0 and abs(s[i - 1] - s_cut) <= tol:
        return i - 1
    j = i - 1
    pc = run.pieces[j]
    a, b = split_piece(pc, s_cut - s[j])
    run.pieces[j:j + 1] = [a, b]
    return j + 1


def split_many(run, cuts, tol=1.0):
    """Dzieli tor w wielu miejscach naraz (jedno przejscie)."""
    Lr = run.length()
    cuts = sorted(c for c in cuts if tol < c < Lr - tol)
    if not cuts:
        return
    out = []
    s0 = 0.0
    k = 0
    for pc in run.pieces:
        s1 = s0 + pc["L"]
        cur = pc
        base = s0
        while k < len(cuts) and cuts[k] < s1 - tol:
            c = cuts[k]
            k += 1
            if c - base <= tol:
                continue
            a, b = split_piece(cur, c - base)
            out.append(a)
            cur, base = b, c
        while k < len(cuts) and cuts[k] < s1 + tol:
            k += 1
        out.append(cur)
        s0 = s1
    run.pieces = out


def point_at(run, s_q):
    s = run.s()
    j = int(np.clip(np.searchsorted(s, s_q) - 1, 0, len(run.pieces) - 1))
    pc = run.pieces[j]
    t = s_q - s[j]
    if pc["kind"] == "P":
        return pc["p0"] + (pc["p1"] - pc["p0"]) * (t / pc["L"]), pc["h0"]
    return integrate_at(pc["p0"], pc["h0"], pc["L"], pc["k0"], pc["k1"], t)


def straight_between(run, s0, s1):
    """Czy tor jest prosty (bez przechylki) na calym [s0, s1]."""
    s = run.s()
    if s0 < 0 or s1 > s[-1]:
        return False
    for j, pc in enumerate(run.pieces):
        if s[j + 1] <= s0 or s[j] >= s1:
            continue
        if pc["kind"] != "P" or abs(pc["k0"]) > 1e-7 or abs(pc["k1"]) > 1e-7 or pc["D0"] or pc["D1"]:
            return False
        if pc.get("switch"):
            return False
    return True


def offset_piece(pc, off):
    """Kawalek przesuniety rownolegle o off (w lewo +)."""
    n0 = np.array([-math.sin(pc["h0"]), math.cos(pc["h0"])])
    n1 = np.array([-math.sin(pc["h1"]), math.cos(pc["h1"])])
    dh = wrap(pc["h1"] - pc["h0"])
    L = max(0.01, pc["L"] - off * dh)
    q = dict(pc, p0=pc["p0"] + off * n0, p1=pc["p1"] + off * n1, L=L)
    for k in ("k0", "k1"):
        kk = pc[k]
        q[k] = kk / (1.0 - off * kk) if abs(1.0 - off * kk) > 1e-6 else kk
    return q


def make_piece(kind, p0, h0, L, k0, k1, V, D=0.0, ksign=0.0, tag=""):
    p1, h1 = integrate_at(p0, h0, L, k0, k1, L) if (k0 or k1) else (np.array(p0) + hvec(h0) * L, h0)
    return dict(kind=kind, L=L, p0=np.array(p0, float), h0=h0, p1=np.array(p1, float), h1=h1,
                k0=k0, k1=k1, D0=D, D1=D, V=V, tag=tag, ksign=ksign)


def arc_pieces(p0, h0, R_signed, length, V, max_len=50.0):
    out = []
    n = max(1, int(math.ceil(length / max_len)))
    k = 1.0 / R_signed
    p, h = np.array(p0, float), h0
    for _ in range(n):
        pc = make_piece("LUK", p, h, length / n, k, k, V)
        out.append(pc)
        p, h = pc["p1"], pc["h1"]
    return out


# ----------------------------------------------------------------------------
# geometria rozjazdu
# ----------------------------------------------------------------------------
def switch_geometry(P0, h, hand, typ):
    """Rozjazd od poczatku P0 w kierunku h; hand +1 lewy, -1 prawy (patrzac od poczatku)."""
    rail, R, n, a, b = CATALOGUE[typ]
    alpha = math.atan(1.0 / n)
    f = hvec(h)
    I = P0 + f * a                                    # punkt matematyczny
    hd = h + hand * alpha
    E = I + hvec(hd) * b                              # koniec toru zwrotnego
    main_end = P0 + f * (a + b)
    # uchwyty jak "whole" w edytorze: 2/3 odleglosci do przeciecia stycznych
    return dict(type=typ, rail=rail, R=R, n=n, a=a, b=b, alpha=alpha, hand=hand,
                P0=np.array(P0, float), h=h, main_end=main_end, E=E, hd=hd,
                h0=a * 2.0 / 3.0, h1=b * 2.0 / 3.0, length=a + b,
                v_div=DIVERGING_SPEED[R])


def crossover_link(sw, e, V):
    """Od konca toru zwrotnego do toru rownoleglego w odstepie e: prosta + luk odwrotny."""
    alpha, b = sw["alpha"], sw["b"]
    y_rest = e - b * math.sin(alpha)
    Rc = min(sw["R"], y_rest / (1.0 - math.cos(alpha)))
    sd = (y_rest - Rc * (1.0 - math.cos(alpha))) / math.sin(alpha)
    pcs = []
    p, h = sw["E"], sw["hd"]
    if sd > 0.05:
        pc = make_piece("P", p, h, sd, 0.0, 0.0, V)
        pcs.append(pc)
        p, h = pc["p1"], pc["h1"]
    pcs += arc_pieces(p, h, -sw["hand"] * Rc, Rc * alpha, V)
    end = pcs[-1]["p1"]
    along = float(np.dot(end - sw["P0"], hvec(sw["h"])))
    return pcs, along, Rc


# ----------------------------------------------------------------------------
# biluk (dowiazania): dwa luki styczne od (P0, t0) do (P1, t1)
# ----------------------------------------------------------------------------
def cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def arc_through(P, t, Q):
    c = Q - P
    cc = float(c @ c)
    if cc < 1e-12:
        return 0.0, 0.0
    k = 2.0 * cross(t, c) / cc
    ang = 2.0 * math.atan2(cross(t, c), float(t @ c))
    if abs(k) < 1e-9:
        return 0.0, math.sqrt(cc)
    return k, ang / k


def biarc(P0, h0, P1, h1):
    """Zwraca liste (k, L) dwoch lukow albo None."""
    t0, t1 = hvec(h0), hvec(h1)
    v = P1 - P0
    t = t0 + t1
    denom = 2.0 * (1.0 - float(t0 @ t1))
    vt = float(v @ t)
    vv = float(v @ v)
    if abs(denom) < 1e-9:
        if abs(vt) < 1e-9:
            return None
        d = vv / (2.0 * vt)
    else:
        disc = vt * vt + denom * vv
        if disc < 0:
            return None
        d = (-vt + math.sqrt(disc)) / denom
    if d <= 0:
        return None
    Pm = 0.5 * (P0 + d * t0 + P1 - d * t1)
    k1, L1 = arc_through(P0, t0, Pm)
    tm = Pm - P0
    # kierunek w Pm z pierwszego luku
    hm = h0 + k1 * L1
    k2, L2 = arc_through(Pm, hvec(hm), P1)
    if L1 <= 0 or L2 <= 0:
        return None
    if abs(wrap(hm + k2 * L2 - h1)) > 1e-3:
        return None
    return [(k1, L1), (k2, L2)]


def biarc_pieces(P0, h0, parts, V, max_len=50.0):
    out = []
    p, h = np.array(P0, float), h0
    for k, L in parts:
        if L < 1e-3:
            continue
        n = max(1, int(math.ceil(L / max_len)))
        for _ in range(n):
            kind = "P" if abs(k) < 1e-7 else "LUK"
            pc = make_piece(kind, p, h, L / n, k, k, V)
            out.append(pc)
            p, h = pc["p1"], pc["h1"]
    return out


def free_len(run, at_end):
    """Dlugosc od konca toru do najblizszego kawalka zajetego przez rozjazd (tyle wolno skrocic)."""
    s = run.s()
    L = float(s[-1])
    best = L
    for j, p in enumerate(run.pieces):
        if p.get("switch"):
            d = (L - float(s[j + 1])) if at_end else float(s[j])
            best = min(best, d)
    return max(0.0, best - 1.0)


# ----------------------------------------------------------------------------
# glowna procedura
# ----------------------------------------------------------------------------
class Layout:
    def __init__(self, e, log):
        self.e = e
        self.runs = {}          # nazwa -> Run
        self.switches = []
        self.log = log          # lista slownikow do raportu
        self.n_sw = 0
        self.forks = set()

    def add(self, run):
        self.runs[run.name] = run
        return run

    # --- tor 2 i rozjazdy na koncach odcinkow dwutorowych
    def double_track(self, t1, e):
        nt = [p.get("NT", 1) >= 2 for p in t1.pieces]
        s = t1.s()
        runs, i = [], 0
        while i < len(nt):
            if nt[i]:
                j = i
                while j + 1 < len(nt) and nt[j + 1]:
                    j += 1
                runs.append([float(s[i]), float(s[j + 1])])
                i = j + 1
            else:
                i += 1
        total = float(s[-1])
        plans = []   # (s_from, s_to, switch_start, switch_end)
        for a, b in runs:
            if b - a < 200.0:
                self.log.append(dict(typ="tor2_pominiety", gdzie=t1.chain, s=round(a), uwagi=f"odcinek dwutorowy {b - a:.0f} m"))
                continue
            sw_a = sw_b = None
            if a > 1.0:
                sw_a, a = self.plan_double_end(t1, a, +1, a, b)
            if b < total - 1.0:
                sw_b, b = self.plan_double_end(t1, b, -1, a, b)
            plans.append((a, b, sw_a, sw_b))
        # tor 2 z kawalkow toru 1 (przed wstawieniem rozjazdow w tor 1)
        for k, (a, b, sw_a, sw_b) in enumerate(plans):
            i0 = split_at(t1, a)
            i1 = split_at(t1, b)
            pcs = [offset_piece(p, e) for p in t1.pieces[i0:i1]]
            name = f"{t1.chain}_t2" + (f"{chr(97 + k)}" if len(plans) > 1 else "")
            self.add(Run(name, "t2", t1.chain, pcs, t1.file))
        for a, b, sw_a, sw_b in plans:
            for sw in (sw_a, sw_b):
                if sw:
                    self.place(t1, sw)

    def plan_double_end(self, t1, s_b, direction, a, b):
        """Rozjazd przy poczatku (direction +1) albo koncu (-1) toru 2. Zwraca (plan, nowe s konca toru 2)."""
        v = t1.pieces[min(len(t1.pieces) - 1, int(np.searchsorted(t1.s(), s_b)) - 1)]["V"] or 60
        typ = pick_switch(v)
        rail, R, n, A, B = CATALOGUE[typ]
        probe = switch_geometry(np.zeros(2), 0.0, 1, typ)
        _, along, _ = crossover_link(probe, self.e, -probe["v_div"])
        span = max(along, A + B)
        best = None
        for shift in np.arange(0.0, 1000.0, 5.0):
            for sgn in (1, -1):
                sj = s_b + sgn * shift              # tu ma sie zaczac/konczyc tor 2
                s_sw = sj - direction * along       # poczatek rozjazdu
                lo, hi = sorted((s_sw, s_sw + direction * span))
                if straight_between(t1, lo - 1.0, hi + 1.0) and not self.occupied(t1, lo, hi):
                    best = (s_sw, sj)
                    break
            if best:
                break
        if not best:
            self.log.append(dict(typ="rozjazd_pominiety", gdzie=t1.chain, s=round(s_b),
                                 uwagi="brak prostej na rozjazd i laczenie w promieniu 1 km"))
            return None, s_b
        s_sw, sj = best
        t1.removed.append((min(s_sw, s_sw + direction * (A + B)), max(s_sw, s_sw + direction * (A + B))))
        return dict(kind="double", s=s_sw, dir=direction, typ=typ, s_join=sj, shift=sj - s_b), sj

    def occupied(self, run, lo, hi):
        return any(not (hi < x0 - 5 or lo > x1 + 5) for x0, x1 in run.removed)

    def place(self, t1, plan):
        """Wstawia rozjazd w tor 1 i laczenie do toru 2."""
        s_sw, d, typ = plan["s"], plan["dir"], plan["typ"]
        P0, h = point_at(t1, s_sw)
        h = h if d > 0 else h + math.pi
        # tor 2 lezy po lewej w kierunku kilometracji
        hand = +1 if d > 0 else -1
        sw = switch_geometry(P0, h, hand, typ)
        V = t1.pieces[0]["V"]
        link, along, Rc = crossover_link(sw, self.e, sw["v_div"])
        self.cut_for_switch(t1, s_sw, d, sw)
        self.n_sw += 1
        sw["name"] = f"zw{self.n_sw:04d}"
        sw["chain"] = t1.chain
        sw["v_main"] = V
        self.switches.append(sw)
        self.add(Run(f"{sw['name']}_lacz", "conn", t1.chain, link, "rozjazdy"))
        self.log.append(dict(typ="rozjazd_koniec_dwutorowego", gdzie=t1.chain, s=round(s_sw), nazwa=sw["name"],
                             rozjazd=typ + (" L" if hand > 0 else " P"), uwagi=f"tor 2 przesuniety o {plan['shift']:+.0f} m, luk laczacy R {Rc:.0f}"))

    def cut_for_switch(self, run, s_sw, d, sw):
        lo, hi = sorted((s_sw, s_sw + d * sw["length"]))
        i0 = split_at(run, lo)
        i1 = split_at(run, hi)
        # ten fragment toru zastepuje rozjazd - kawalki oznaczone, nie zapisywane
        for p in run.pieces[i0:i1]:
            p["switch"] = sw
        sw["run"] = run.name

    # --- dowiazanie konca linii do innej linii
    def junction(self, branch, at_end, main_runs, max_dist=30.0):
        """Koniec toru 1 'branch' przy srodku innej linii: rozjazd na linii glownej + biluk."""
        s_all = branch.s()
        total = float(s_all[-1])
        if at_end:
            Q0, hq = branch.pieces[-1]["p1"], branch.pieces[-1]["h1"]
        else:
            Q0, hq = branch.pieces[0]["p0"], branch.pieces[0]["h0"] + math.pi
        # najblizszy tor glowny
        best = None
        for run in main_runs:
            if run.chain == branch.chain:
                continue
            for j, pc in enumerate(run.pieces):
                for p in (pc["p0"], pc["p1"]):
                    d = float(np.hypot(*(p - Q0)))
                    if d < max_dist and (best is None or d < best[0]):
                        best = (d, run)
        if best is None:
            return False
        run = best[1]
        # tor po stronie odgalezienia: gdy linia glowna ma tor 2 (po lewej), a odgalezienie
        # odchodzi w lewo, rozjazd idzie w tor 2 - inaczej laczenie przecinaloby tor 2
        same = [r for r in main_runs if r.chain == run.chain and r.kind in ("t1", "t2")]
        t1m = [r for r in same if r.kind == "t1"]
        if t1m:
            base = t1m[0]
            back = min(150.0, total * 0.5)
            Qb, _ = point_at(branch, total - back if at_end else back)
            sb = base.s()
            ii = int(np.argmin([np.hypot(*(p["p0"] - Q0)) for p in base.pieces]))
            ss0 = np.linspace(max(0, sb[ii] - 1200), min(sb[-1], sb[ii] + 1200), 1200)
            pp = np.array([point_at(base, x)[0] for x in ss0])
            jj = int(np.argmin(np.hypot(*(pp - Qb).T)))
            P_b, h_b = point_at(base, float(ss0[jj]))
            left = cross(hvec(h_b), Qb - P_b) > 0
            run = base
            if left:
                for r2 in same:
                    if r2.kind == "t2":
                        d2 = min(float(np.hypot(*(p["p0"] - Q0))) for p in r2.pieces)
                        if d2 < max_dist + 10:
                            run = r2
                            break
        # rzut na tor glowny
        sr = run.s()
        ss = np.linspace(0, sr[-1], max(2, int(sr[-1] / 2.0)))
        pts = np.array([point_at(run, x)[0] for x in ss]) if sr[-1] < 4000 else None
        if pts is None:
            mids = np.array([pc["p0"] for pc in run.pieces])
            j = int(np.argmin(np.hypot(*(mids - Q0).T)))
            ss = np.linspace(max(0, sr[j] - 600), min(sr[-1], sr[j] + 600), 600)
            pts = np.array([point_at(run, x)[0] for x in ss])
        j = int(np.argmin(np.hypot(*(pts - Q0).T)))
        s_m = float(ss[j])
        v_branch = (branch.pieces[-1] if at_end else branch.pieces[0])["V"] or 60
        fork = None
        if s_m < 50 or s_m > sr[-1] - 50:
            # przy koncu linii glownej: rozwidlenie (oba konce wchodza w linie w tym samym kierunku)?
            main_start = s_m < 50
            into_main = run.pieces[0]["h0"] if main_start else run.pieces[-1]["h1"] + math.pi
            into_branch = hq + math.pi
            if math.cos(wrap(into_main - into_branch)) < math.cos(math.radians(45)):
                return False     # zwykle polaczenie koniec-koniec
            v_m = (run.pieces[0] if main_start else run.pieces[-1])["V"] or 60
            if (v_branch, -branch.length()) > (v_m, -run.length()):
                return False     # odgalezieniem jest ta druga linia
            fork = +1 if main_start else -1
        _, hm = point_at(run, s_m)
        v_main = run.pieces[min(len(run.pieces) - 1, int(np.searchsorted(sr, s_m)))]["V"] or 60
        typ = pick_switch(v_main, min(v_branch, 100))
        sgn = 1.0 if math.cos(wrap(hq - hm)) > 0 else -1.0
        d_sw = -sgn                                   # rozjazd skierowany pod prad wjazdu z odgalezienia
        if fork:
            d_sw = fork
            s_m = 0.0 if fork > 0 else float(sr[-1])
        # strona odgalezienia wzgledem kierunku rozjazdu
        h_sw_base = hm if d_sw > 0 else hm + math.pi
        side = cross(hvec(h_sw_base), Q0 - point_at(run, s_m)[0])
        hand = 1 if side > 0 else -1
        # przeszukanie: polozenie rozjazdu i skrocenie odgalezienia
        rail, R, n, A, B = CATALOGUE[typ]
        cand = None
        fl = free_len(branch, at_end)
        for trim in np.arange(10.0, 260.0, 10.0):
            if trim > total - 20 or trim > fl:
                break
            sq = total - trim if at_end else trim
            Q, hqq = point_at(branch, sq)
            hq_in = hqq + math.pi if at_end else hqq      # kierunek jazdy od rozjazdu w odgalezienie
            offs = (np.arange(0.0, 300.0, 5.0) * fork) if fork else np.arange(-300.0, 300.0, 5.0)
            for off in offs:
                s_sw = s_m + off
                lo, hi = sorted((s_sw, s_sw + d_sw * (A + B)))
                if (lo < 5 or hi > sr[-1] - 5) and not fork:
                    continue
                if lo < -0.01 or hi > sr[-1] + 0.01:
                    continue
                if not straight_between(run, lo - 1, hi + 1) or self.occupied(run, lo, hi):
                    continue
                P0, h0 = point_at(run, s_sw)
                h0 = h0 if d_sw > 0 else h0 + math.pi
                sw = switch_geometry(P0, h0, hand, typ)
                parts = biarc(sw["E"], sw["hd"], Q, hq_in)
                if parts is None:
                    continue
                kmax = max(abs(k) for k, _ in parts)
                rmin = 1.0 / kmax if kmax > 0 else 1e9
                turn = sum(abs(k * L) for k, L in parts)
                if rmin < 180.0 or turn > math.radians(60):
                    continue
                score = trim + 0.5 * abs(off) + 2000.0 / max(rmin, 1.0) * 10
                if rmin < R:
                    score += 200
                if cand is None or score < cand[0]:
                    cand = (score, s_sw, sq, sw, parts, rmin)
            if cand and cand[0] < trim:    # dalsze skracanie juz nie pomoze
                break
        if cand is None:
            self.log.append(dict(typ="dowiazanie_pominiete", gdzie=branch.chain, s=round(total if at_end else 0),
                                 uwagi=f"do {run.chain}: nie da sie wpisac rozjazdu {typ} i luku R>=180 m"))
            return False
        _, s_sw, sq, sw, parts, rmin = cand
        # skroc odgalezienie
        if at_end:
            i = split_at(branch, sq)
            branch.pieces = branch.pieces[:i]
        else:
            i = split_at(branch, sq)
            branch.pieces = branch.pieces[i:]
        self.cut_for_switch(run, s_sw, d_sw, sw)
        run.removed.append(tuple(sorted((s_sw, s_sw + d_sw * sw["length"]))))
        self.n_sw += 1
        sw["name"] = f"zw{self.n_sw:04d}"
        sw["chain"] = run.chain
        sw["v_main"] = v_main
        self.switches.append(sw)
        V = min(sw["v_div"], v_branch)
        link = biarc_pieces(sw["E"], sw["hd"], parts, V)
        self.add(Run(f"{sw['name']}_lacz", "conn", branch.chain, link, "rozjazdy"))
        self.forks.add((branch.name, at_end))
        self.log.append(dict(typ="rozjazd_rozwidlenie" if fork else "rozjazd_wezel", gdzie=f"{branch.chain}->{run.chain}", s=round(s_sw), nazwa=sw["name"],
                             rozjazd=typ + (" L" if hand > 0 else " P"),
                             uwagi=f"odgalezienie skrocone o {total - sq if at_end else sq:.0f} m, luk laczacy Rmin {rmin:.0f}"))
        return True

    # --- polaczenie koniec-koniec
    def end_join(self, run_a, end_a, run_b, end_b, V):
        if end_a:
            P0, h0 = run_a.pieces[-1]["p1"], run_a.pieces[-1]["h1"]
        else:
            P0, h0 = run_a.pieces[0]["p0"], run_a.pieces[0]["h0"] + math.pi
        if end_b:
            P1, h1 = run_b.pieces[-1]["p1"], run_b.pieces[-1]["h1"] + math.pi
        else:
            P1, h1 = run_b.pieces[0]["p0"], run_b.pieces[0]["h0"]
        gap = float(np.hypot(*(P1 - P0)))
        kmax = None
        if math.cos(wrap(h1 - h0)) < 0:
            return None              # konce rozchodza sie - to rozwidlenie, nie przedluzenie
        if gap < 0.005 and abs(wrap(h1 - h0)) < math.radians(0.05):
            return "styk"
        if abs(wrap(h1 - h0)) < 1e-4 and abs(cross(hvec(h0), P1 - P0)) < 0.01:
            pcs = [make_piece("P", P0, h0, gap, 0, 0, V)]
            pcs[0]["p1"] = np.array(P1, float)
        else:
            # zalamanie albo przesuniecie: cofnij oba konce i polacz biklukiem R >= 180 m (wolimy wiekszy)
            best = None
            fa, fb = free_len(run_a, end_a), free_len(run_b, end_b)
            for trim in np.arange(0.0, 205.0, 5.0):
                for ta, tb in ((trim, trim), (trim, 0.0), (0.0, trim)):
                    if ta > run_a.length() - 20 or tb > run_b.length() - 20:
                        continue
                    if ta > fa or tb > fb:
                        continue
                    sa = run_a.length() - ta if end_a else ta
                    sb = run_b.length() - tb if end_b else tb
                    A0, ha = point_at(run_a, sa)
                    B0, hb = point_at(run_b, sb)
                    ha = ha if end_a else ha + math.pi
                    hb = hb + math.pi if end_b else hb
                    if not (ta or tb) and gap < 0.005:
                        continue
                    parts = biarc(A0, ha, B0, hb)
                    if parts is None:
                        continue
                    kmax = max(abs(k) for k, _ in parts)
                    if kmax > 1 / 180.0:
                        continue
                    score = ta + tb + (300.0 if kmax > 1 / 500.0 else 0.0)
                    if best is None or score < best[0]:
                        best = (score, ta, tb, sa, sb, A0, ha, parts, kmax)
                if best and best[0] <= 2 * trim:
                    break
            if best is None:
                return None
            _, ta, tb, sa, sb, P0, h0, parts, kmax = best
            for run, end, t, sx in ((run_a, end_a, ta, sa), (run_b, end_b, tb, sb)):
                if t > 0:
                    i = split_at(run, sx)
                    run.pieces = run.pieces[:i] if end else run.pieces[i:]
            pcs = biarc_pieces(P0, h0, parts, V)
            gap = ta + tb
        name = f"lacz_{run_a.chain}_{run_b.chain}_{'k' if end_a else 'p'}{'k' if end_b else 'p'}"
        self.add(Run(name, "join", run_a.chain, pcs, "rozjazdy"))
        return f"luk laczacy po skroceniu o {gap:.0f} m, Rmin {1 / max(kmax, 1e-9):.0f}" if kmax else f"prosta {gap:.2f} m"


    # --- porzadki po wstawieniu: konce torow na cudzych stykach
    def end_points(self):
        pts = []
        for r in self.runs.values():
            live = [p for p in r.pieces if not p.get("switch")]
            if not live:
                continue
            # konce kazdego ciaglego kawalka (rozjazd dzieli tor 1 na czesci)
            prev = None
            for p in r.pieces:
                sw = p.get("switch")
                if sw is None and (prev is None or prev.get("switch")):
                    pts.append((p["p0"], p["h0"] + math.pi, r.name))
                if sw is not None and prev is not None and prev.get("switch") is None:
                    pts.append((prev["p1"], prev["h1"], r.name))
                prev = p
            if prev is not None and prev.get("switch") is None:
                pts.append((prev["p1"], prev["h1"], r.name))
        for sw in self.switches:
            pts.append((sw["P0"], sw["h"] + math.pi, sw["name"]))
            pts.append((sw["main_end"], sw["h"], sw["name"]))
            pts.append((sw["E"], sw["hd"], sw["name"]))
        return pts

    def resolve_overlaps(self, trim=10.0):
        """Wolny koniec, ktory wypada na cudzym styku bez ciaglosci kierunku (rozwidlenie bez rozjazdu,
        koniec na wezle srodka innej linii), zostaje cofniety o 'trim' m - inaczej symulator polaczylby
        go z przypadkowym torem."""
        from scipy.spatial import cKDTree
        changed = 0
        for _ in range(3):
            pts = self.end_points()
            # wszystkie granice kawalkow (rowniez wewnetrzne)
            inner = []
            for r in self.runs.values():
                for p in r.pieces:
                    if not p.get("switch"):
                        inner.append((p["p0"], r.name))
                        inner.append((p["p1"], r.name))
            tree = cKDTree(np.array([q[0] for q in inner]))
            bad = []
            ptree = cKDTree(np.array([q[0] for q in pts]))
            seen = set()
            for i, (P, hout, name) in enumerate(pts):
                if i in seen:
                    continue
                group = [j for j in ptree.query_ball_point(P, 0.02)]
                seen.update(group)
                inner_hits = {inner[j][1] for j in tree.query_ball_point(P, 0.02)}
                names = {pts[j][2] for j in group}
                if len(group) == 1 and len(inner_hits - names) == 0:
                    continue          # wolny koniec
                # najlepsza para o ciaglym kierunku zostaje, reszta (tory 1/2) cofnieta
                keep = set()
                bestc = -2.0
                for a in group:
                    for b in group:
                        if a < b and pts[a][2] != pts[b][2]:
                            c = math.cos(wrap(pts[b][1] - pts[a][1] - math.pi))
                            if c > bestc:
                                bestc, keep = c, {a, b}
                if bestc < math.cos(math.radians(1.0)):
                    keep = set()
                # koniec lezacy na wewnetrznym styku innego toru (nie na jego koncu) tez jest zly
                interior = inner_hits - names
                for j in group:
                    nm = pts[j][2]
                    if j in keep and not interior:
                        continue
                    if nm in self.runs and self.runs[nm].kind in ("t1", "t2"):
                        bad.append((pts[j][0], nm))
            if not bad:
                break
            for P, name in bad:
                r = self.runs[name]
                L = r.length()
                if L < trim + 20:
                    continue
                at_e = float(np.hypot(*(r.pieces[-1]["p1"] - P))) < 0.05
                if free_len(r, at_e) < trim:
                    continue
                at_end = float(np.hypot(*(r.pieces[-1]["p1"] - P))) < 0.05
                at_start = float(np.hypot(*(r.pieces[0]["p0"] - P))) < 0.05
                if at_end:
                    i = split_at(r, L - trim)
                    r.pieces = r.pieces[:i]
                elif at_start:
                    i = split_at(r, trim)
                    r.pieces = r.pieces[i:]
                else:
                    continue
                changed += 1
                self.log.append(dict(typ="koniec_cofniety", gdzie=r.chain, s=0,
                                     uwagi=f"{name}: koniec na cudzym styku bez rozjazdu, cofniety o {trim:.0f} m"))
        return changed


    # --- wspolne odcinki (IRT rysuje linie biegnace razem na tej samej osi)
    @staticmethod
    def dense(run, step=5.0):
        pts, ss = [], []
        s0 = 0.0
        for p in run.pieces:
            k = max(2, int(p["L"] / step) + 1)
            t = np.linspace(0.0, p["L"], k)
            if p["kind"] == "P":
                xy = p["p0"][None, :] + (p["p1"] - p["p0"])[None, :] * (t / p["L"])[:, None]
            else:
                hh = p["h0"] + p["k0"] * t + (p["k1"] - p["k0"]) * t * t / (2 * p["L"])
                c, sn = np.cos(hh), np.sin(hh)
                dt = np.diff(t)
                x = p["p0"][0] + np.concatenate([[0], np.cumsum(0.5 * (c[1:] + c[:-1]) * dt)])
                y = p["p0"][1] + np.concatenate([[0], np.cumsum(0.5 * (sn[1:] + sn[:-1]) * dt)])
                xy = np.column_stack([x, y])
            pts.append(xy)
            ss.append(s0 + t)
            s0 += p["L"]
        return np.vstack(pts), np.concatenate(ss)

    def shared_stretches(self, runs, tol=1.5, min_len=15.0):
        from scipy.spatial import cKDTree
        D = {r.name: self.dense(r) for r in runs}
        trees = {r.name: cKDTree(D[r.name][0]) for r in runs}
        prio = lambda r, end: ((r.pieces[-1] if end else r.pieces[0])["V"] or 0, r.length())
        cuts = []
        for r in runs:
            pts, ss = D[r.name]
            for at_end in (False, True):
                order = np.argsort(-ss) if at_end else np.argsort(ss)
                for o in runs:
                    if o is r:
                        continue
                    lo_hi = np.array([pts.min(0), pts.max(0)])
                    d, _ = trees[o.name].query(pts[order[:5]], distance_upper_bound=tol)
                    if not np.isfinite(d).all():
                        continue
                    # dlugosc wspolnego odcinka od konca
                    dd, _ = trees[o.name].query(pts[order], distance_upper_bound=tol)
                    k = int(np.argmax(~np.isfinite(dd))) if (~np.isfinite(dd)).any() else len(dd)
                    L = abs(ss[order[k - 1]] - ss[order[0]])
                    if L < min_len:
                        continue
                    # kto oddaje wspolny odcinek: nizszy priorytet (predkosc, dlugosc)
                    if prio(r, at_end) > prio(o, False) and prio(r, at_end) > prio(o, True):
                        continue
                    cuts.append((r, at_end, L, o))
                    break
        for r, at_end, L, o in cuts:
            if L > r.length() - 30:
                self.log.append(dict(typ="wspolny_odcinek", gdzie=f"{r.chain}/{o.chain}", s=0,
                                     uwagi=f"{r.name} prawie w calosci na osi {o.chain} - pozostawiony"))
                continue
            if at_end:
                i = split_at(r, r.length() - L)
                r.pieces = r.pieces[:i]
            else:
                i = split_at(r, L)
                r.pieces = r.pieces[i:]
            self.log.append(dict(typ="wspolny_odcinek", gdzie=f"{r.chain}/{o.chain}", s=0,
                                 uwagi=f"{r.name}: {L:.0f} m wspolnych z {o.chain} oddane tamtej linii"))


# ----------------------------------------------------------------------------
# profil podluzny
# ----------------------------------------------------------------------------
def mark_env(lengths, cover, tunnel_depth=8.0, tunnel_min=30.0, bridge_h=5.0, bridge_max=500.0):
    """'tunnel' / 'bridge' / 'flat' dla kolejnych kawalkow (cover = teren - niweleta)."""
    n = len(cover)
    env = ["flat"] * n

    def runs(mask):
        i = 0
        while i < n:
            if mask[i]:
                j = i
                while j + 1 < n and mask[j + 1]:
                    j += 1
                yield i, j
                i = j + 1
            else:
                i += 1
    for i, j in runs(cover > tunnel_depth):
        if sum(lengths[i:j + 1]) >= tunnel_min:
            for k in range(i, j + 1):
                env[k] = "tunnel"
    for i, j in runs(cover < -bridge_h):
        if sum(lengths[i:j + 1]) <= bridge_max:
            for k in range(i, j + 1):
                env[k] = "bridge"
    return env


def apply_profile(lay, t1_runs, nmt, step=10.0, grade_tol=0.5, bridge_tol=2.5, log=None,
                  line_types=None, stations=(), rows=None):
    import profil
    from scipy.spatial import cKDTree
    prof_pts, prof_z = [], []          # probki niwelety juz policzonych linii (do dowiazan)
    order = sorted(t1_runs, key=lambda r: (-(max(p["V"] or 0 for p in r.pieces)), -r.length()))
    results = {}
    dens = {r.name: Layout.dense(r, step) for r in order}
    pts_all = np.vstack([d[0] for d in dens.values()])
    if hasattr(nmt, "prefetch"):
        nmt.prefetch([tuple(p) for p in pts_all[::5]])
    st_xy = np.array([xy for _, xy in stations]) if len(stations) else np.zeros((0, 2))
    # dowiazania przez laczniki: koniec toru 1 -> punkt na innej linii (drugi koniec lacznika)
    ties = {}
    sw_by = {sw["name"]: sw for sw in lay.switches}
    for lk in lay.runs.values():
        if lk.kind not in ("conn", "join") or not lk.pieces:
            continue
        A, B = lk.pieces[0]["p0"], lk.pieces[-1]["p1"]
        if lk.kind == "conn":
            sw = sw_by.get(lk.name.split("_lacz")[0])
            other_chain = sw["chain"] if sw else None
        else:
            other_chain = None
        for r in t1_runs:
            for end_i, P in ((0, r.pieces[0]["p0"]), (-1, r.pieces[-1]["p1"])):
                for X, Y in ((A, B), (B, A)):
                    if float(np.hypot(*(P - X))) < 0.05:
                        oc = other_chain
                        if oc is None:
                            # drugi koniec lacznika: ktora linia tam sie konczy
                            for r2 in t1_runs:
                                if r2 is not r and min(float(np.hypot(*(r2.pieces[0]["p0"] - Y))), float(np.hypot(*(r2.pieces[-1]["p1"] - Y)))) < 0.05:
                                    oc = r2.chain
                        if oc and oc != r.chain:
                            ties[(r.name, end_i)] = (oc, Y, lk.length())
    chain_tree = {}
    for r in order:
        pts, ss = dens[r.name]
        sel = np.unique(np.concatenate([np.searchsorted(ss, np.arange(0, ss[-1], step)), [len(ss) - 1]]))
        pts, ss = pts[sel], ss[sel]
        g = np.array([nmt.height(E, N) for E, N in pts])
        V = np.interp(ss, r.s()[:-1], [p["V"] or 60 for p in r.pieces])
        fixed = {}
        for end_i, i in ((0, 0), (-1, len(ss) - 1)):
            t = ties.get((r.name, end_i))
            if t and t[0] in chain_tree:
                trc, zc = chain_tree[t[0]]
                d, j = trc.query(t[1])
                if d < 10.0:
                    fixed[i] = float(zc[j])
        if prof_pts:
            tree = cKDTree(np.vstack(prof_pts))
            zz = np.concatenate(prof_z)
            for i in (0, len(ss) - 1):
                if i in fixed:
                    continue
                d, j = tree.query(pts[i])
                if d < 30.0:
                    fixed[i] = float(zz[j])
        # strefy stacji (tory stacyjne) i przystankow (perony) przy tej osi
        zones = []
        if len(st_xy):
            tr = cKDTree(pts)
            for (p, xy) in stations:
                d, j = tr.query(xy)
                if d > 40.0:
                    continue
                kind = "ST" if p.get("TYPE") == "ST" else ("PO" if p.get("TYPE") in ("PO", "MPO", "LPO", "POGP") else None)
                if kind is None:
                    continue
                half = 300.0 if kind == "ST" else 100.0
                zones.append((max(0.0, ss[j] - half), min(ss[-1], ss[j] + half), kind, p.get("SN")))
        lt = (line_types or {}).get(r.chain, "M")
        res = profil.fit_grade_line(ss, g, V, fixed, grade_tol, bridge_tol, line_type=lt, zones=zones)
        relaxed = None
        if res is not None:
            # istniejaca linia gorska: przy limicie P2 typu linii niweleta odchodzi od terenu o wiele metrow
            # na dlugich odcinkach - wtedy limit jak dla krotkich odcinkow (35 promili, tab. 10.1 przypis 2)
            ps_, pz_, radii_, _, _ = res
            dev_ = np.abs(np.nan_to_num(g - profil.elevation(ps_, pz_, radii_, ss)))
            far = dev_ > 6.0
            run_len = 0.0
            longest = 0.0
            for f, dlt in zip(far, np.diff(np.concatenate([ss, [ss[-1]]]))):
                run_len = run_len + dlt if f else 0.0
                longest = max(longest, run_len)
            if longest > 300.0 and profil.GRADE[lt]["P2"] < 35.0:
                res2 = profil.fit_grade_line(ss, g, V, fixed, grade_tol, bridge_tol, grade_max=35.0, line_type=lt, zones=zones)
                if res2 is not None:
                    res, relaxed = res2, longest
        if res is None:
            if log is not None:
                log.append(dict(typ="profil_brak_nmt", gdzie=r.chain, s=0, uwagi="brak danych NMT - wysokosc 0"))
            continue
        ps, pz, radii, warn, stats = res
        gr = np.diff(pz) / np.diff(ps)
        split_many(r, [float(b) for b in profil.vertical_breaks(ps, radii, gr)])
        s = r.s()
        y = profil.elevation(ps, pz, radii, s)
        for i, p in enumerate(r.pieces):
            p["y0"], p["y1"] = float(y[i]), float(y[i + 1])
        results[r.name] = (ps, pz, radii)
        # srodowisko toru: tunel (teren > 8 m nad niweleta na >= 30 m), most/wiadukt (niweleta > 5 m nad terenem, <= 500 m)
        gl = np.interp(s, ss, np.nan_to_num(g, nan=np.nanmedian(g)))
        mid_cover = 0.5 * (gl[1:] + gl[:-1]) - 0.5 * (y[1:] + y[:-1])
        env = mark_env(np.diff(s), mid_cover)
        for p_, e_ in zip(r.pieces, env):
            p_["env"] = e_
        tun = sum(pc["L"] for pc in r.pieces if pc.get("env") == "tunnel")
        brg = sum(pc["L"] for pc in r.pieces if pc.get("env") == "bridge")
        dense_y = profil.elevation(ps, pz, radii, ss)
        prof_pts.append(pts)
        prof_z.append(dense_y)
        pts1, ss1 = Layout.dense(r, 1.0)
        chain_tree[r.chain] = (cKDTree(pts1), profil.elevation(ps, pz, radii, ss1))
        # 10.2 pkt 9: konce lukow pionowych >= 6 m od koncow ramp przechylkowych (KP)
        ramp_ends = [s[i] for i in range(1, len(r.pieces)) if (r.pieces[i]["kind"] == "KP") != (r.pieces[i - 1]["kind"] == "KP")]
        vc_ends = []
        for i in range(1, len(ps) - 1):
            if radii[i] > 0:
                T = radii[i] * abs(gr[i] - gr[i - 1]) / 2.0
                vc_ends += [ps[i] - T, ps[i] + T]
            else:
                vc_ends.append(ps[i])
        near6 = 0
        if ramp_ends and vc_ends:
            re_ = np.array(ramp_ends)
            near6 = int(sum(np.min(np.abs(re_ - x)) < 6.0 for x in vc_ends))
        diff = g - dense_y
        ok = np.isfinite(diff)
        row = dict(lancuch=r.chain, typ_linii=lt, PVI=len(ps), max_pochylenie=round(float(np.max(np.abs(gr)) * 1000), 1),
                   pochylenia=stats["pochylenia"], luki_pionowe=stats["luki"],
                   strefy_stacji_i_peronow=stats["strefy_stacyjne"], strefy_ponad_P1=stats["strefy_ponad_P1"],
                   konce_lukow_pionowych_blizej_6m_od_rampy=near6,
                   teren_minus_niweleta_mediana=round(float(np.median(diff[ok])), 2) if ok.any() else None,
                   nasyp_max=round(float(-np.min(diff[ok])), 1) if ok.any() else None,
                   wykop_max=round(float(np.max(diff[ok])), 1) if ok.any() else None,
                   odch_abs_p95=round(float(np.percentile(np.abs(diff[ok]), 95)), 2) if ok.any() else None,
                   brak_nmt_probek=int((~ok).sum()), dowiazane_konce=len(fixed),
                   limit_pochylenia=stats.get("limit_P2"), tunele_m=round(tun), mosty_wiadukty_m=round(brg),
                   limit_podniesiony_bo_teren=(f"odchylka >6 m na {relaxed:.0f} m przy limicie typu linii" if relaxed else None),
                   ostrzezenia=warn)
        if rows is not None:
            rows.append(row)
        if log is not None:
            log.append(dict(typ="profil", gdzie=r.chain, s=0,
                            uwagi=f"PVI {len(ps)}, max {row['max_pochylenie']} promila, luki {stats['luki']}, "
                                  f"teren-niweleta p95 {row['odch_abs_p95']} m, ostrzezen {len(warn)}"))
    if not prof_pts:
        return results
    tree = cKDTree(np.vstack(prof_pts))
    zz = np.concatenate(prof_z)

    env_pts, env_val = [], []
    for rn in results:
        for pc in lay.runs[rn].pieces:
            if pc.get("env", "flat") != "flat":
                env_pts.append(0.5 * (pc["p0"] + pc["p1"]))
                env_val.append(pc["env"])
    env_tree = cKDTree(np.array(env_pts)) if env_pts else None

    def env_at(P):
        if env_tree is None:
            return None
        d, j = env_tree.query(P)
        return env_val[j] if d < 15.0 else None

    # wysokosci per linia - zeby na skrzyzowaniach dwupoziomowych nie brac cudzej niwelety
    def h_chain(chain, P):
        if chain not in chain_tree:
            return h_at(P)
        tr, zz_ = chain_tree[chain]
        d, j = tr.query(P)
        pts_ = tr.data
        best = float(zz_[j])
        for a, b in ((j - 1, j), (j, j + 1)):
            if a < 0 or b >= len(zz_):
                continue
            v = pts_[b] - pts_[a]
            vv = float(v @ v)
            if vv < 1e-9:
                continue
            t = float((np.asarray(P) - pts_[a]) @ v) / vv
            if 0.0 <= t <= 1.0:
                return float(zz_[a] + (zz_[b] - zz_[a]) * t)
        return best

    def h_at(P):
        d, j = tree.query(P, k=2)
        w = 1.0 / np.maximum(d, 1e-3)
        return float(np.sum(zz[j] * w) / np.sum(w))

    sw_by_name = {sw["name"]: sw for sw in lay.switches}
    for sw in lay.switches:
        sw["y0"], sw["yM"], sw["yE"] = (h_chain(sw["chain"], sw["P0"]), h_chain(sw["chain"], sw["main_end"]),
                                        h_chain(sw["chain"], sw["E"]))
    for r in lay.runs.values():
        if r.name in results:
            continue
        if r.kind == "t2":
            base = [x for x in lay.runs.values() if x.kind == "t1" and x.chain == r.chain and x.name in results]
            if base:
                b = base[0]
                bs = b.s()
                pts2, s2 = Layout.dense(r, 2.0)
                t2tree = cKDTree(pts2)
                ps, pz, radii = results[b.name]
                gr = np.diff(pz) / np.diff(ps)
                pb, sbs = Layout.dense(b, 2.0)
                cuts = []
                for sb in profil.vertical_breaks(ps, radii, gr) + list(ps[1:-1]):
                    if 0 < sb < bs[-1]:
                        P = pb[min(len(pb) - 1, int(np.searchsorted(sbs, sb)))]
                        d, j = t2tree.query(P)
                        if d < 10:
                            cuts.append(float(s2[j]))
                split_many(r, cuts)
            for p in r.pieces:
                p["y0"], p["y1"] = h_chain(r.chain, p["p0"]), h_chain(r.chain, p["p1"])
        else:
            # lacznik: wysokosci koncow z linii, do ktorych dochodzi, miedzy nimi liniowo
            if r.kind == "conn":
                sw = sw_by_name.get(r.name.split("_lacz")[0])
                ya = sw["yE"] if sw else h_at(r.pieces[0]["p0"])
                yb = h_chain(r.chain, r.pieces[-1]["p1"])
            else:
                parts = r.name.split("_")
                ca, cb = r.chain, None
                for x in lay.runs.values():
                    if x.kind == "t1" and r.name.startswith("lacz_" + x.chain + "_"):
                        rest = r.name[len("lacz_" + x.chain + "_"):]
                        for y in lay.runs.values():
                            if y.kind == "t1" and rest.startswith(y.chain + "_"):
                                ca, cb = x.chain, y.chain
                ya = h_chain(ca, r.pieces[0]["p0"])
                yb = h_chain(cb or ca, r.pieces[-1]["p1"])
            s_ = r.s()
            tot = s_[-1] if s_[-1] > 0 else 1.0
            for i, p in enumerate(r.pieces):
                p["y0"] = ya + (yb - ya) * s_[i] / tot
                p["y1"] = ya + (yb - ya) * s_[i + 1] / tot
        for p in r.pieces:
            e_ = env_at(0.5 * (p["p0"] + p["p1"]))
            if e_:
                p["env"] = e_
    return results


def mark_crossings(lay, dy_min=2.5, half=3.52, slope=1.5, margin=4.0, log=None):
    """Skrzyzowania bezkolizyjne: tor gorny nad dolnym dostaje wiadukt (env bridge) na dlugosci, na ktorej
    jego nasyp (polszerokosc torowiska + skarpa 1:slope) wchodzilby na tor dolny. Bez tego teren z nasypu
    gornej linii zasypuje dolna."""
    from scipy.spatial import cKDTree
    runs = [r for r in lay.runs.values() if r.pieces and all("y0" in p for p in r.pieces if not p.get("switch"))]
    P, Y, S, R, E = [], [], [], [], []
    for ri, r in enumerate(runs):
        s0 = 0.0
        for p in r.pieces:
            n = max(1, int(math.ceil(p["L"] / 2.0)))
            for i in range(n + 1):
                t = p["L"] * i / n
                if p["kind"] == "P":
                    xy = p["p0"] + (p["p1"] - p["p0"]) * (i / n)
                else:
                    xy = split_piece(p, t)[0]["p1"] if 0 < i < n else (p["p0"] if i == 0 else p["p1"])
                P.append(xy)
                Y.append(p["y0"] + (p["y1"] - p["y0"]) * (i / n) if not p.get("switch") else np.nan)
                S.append(s0 + t)
                R.append(ri)
                E.append(p.get("env") == "tunnel")
            s0 += p["L"]
    P, Y, S, R = np.array(P, float), np.array(Y), np.array(S), np.array(R)
    tree = cKDTree(P)
    spans = defaultdict(list)          # run index -> [(s0, s1)]
    for i, j in tree.query_pairs(1.5):
        if R[i] == R[j] or runs[R[i]].chain == runs[R[j]].chain or np.isnan(Y[i]) or np.isnan(Y[j]):
            continue
        dy = Y[i] - Y[j]
        if abs(dy) < dy_min:
            continue
        up, lo = (i, j) if dy > 0 else (j, i)
        if E[lo] or E[up]:
            continue
        # kat przeciecia, zeby nasyp przy skosnym skrzyzowaniu tez zszedl z dolnego toru
        def hd(k):
            a = max(0, k - 1) if R[max(0, k - 1)] == R[k] else k
            b = k + 1 if k + 1 < len(R) and R[k + 1] == R[k] else k
            d = P[b] - P[a]
            return math.atan2(d[1], d[0])
        ang = abs(math.sin(hd(up) - hd(lo)))
        w = (half + abs(dy) * slope + margin + half) / max(ang, 0.25)
        spans[R[up]].append((S[up] - w, S[up] + w))
    n_br = 0
    for ri, sp in spans.items():
        r = runs[ri]
        sp.sort()
        merged = []
        for a, b in sp:
            if merged and a <= merged[-1][1] + 20:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b])
        Lr = r.length()
        cuts = [c for m in merged for c in m if 1 < c < Lr - 1]
        # dzielenie z interpolacja wysokosci
        out, s0, k = [], 0.0, 0
        cuts.sort()
        for pc in r.pieces:
            s1 = s0 + pc["L"]
            cur, base = pc, s0
            while k < len(cuts) and cuts[k] < s1 - 1.0:
                c = cuts[k]
                k += 1
                if c - base <= 1.0 or cur.get("switch"):
                    continue
                f = (c - base) / cur["L"]
                ym = cur["y0"] + (cur["y1"] - cur["y0"]) * f
                a, b = split_piece(cur, c - base)
                a["y1"] = ym
                b["y0"] = ym
                out.append(a)
                cur, base = b, c
            while k < len(cuts) and cuts[k] < s1 + 1.0:
                k += 1
            out.append(cur)
            s0 = s1
        r.pieces = out
        s0 = 0.0
        for pc in r.pieces:
            mid = s0 + pc["L"] / 2
            if not pc.get("switch") and any(a <= mid <= b for a, b in merged) and pc.get("env") != "tunnel":
                if pc.get("env") != "bridge":
                    n_br += 1
                pc["env"] = "bridge"
            s0 += pc["L"]
        if log is not None:
            for a, b in merged:
                log.append(dict(typ="wiadukt_skrzyzowanie", gdzie=r.name, s=round(max(a, 0)), uwagi=f"{b - a:.0f} m"))
    print(f"skrzyzowania bezkolizyjne: {sum(len(v) for v in spans.values())} par punktow, wiadukty na {n_br} kawalkach")
    return n_br
