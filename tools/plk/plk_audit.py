#!/usr/bin/env python3
"""
plk_audit.py - kontrola i poprawa torow scenerii MaSzyny wedlug norm PKP PLK.

Czyta tory (track normal / switch) z plikow .scn i .inc, laczy je w siec po wspolnych koncach i sprawdza je wedlug
ST-T1-A6 "Uklady geometryczne torow" (PLK, obowiazuje od 17.04.2025) i rozporzadzenia MI z 17.09.2026 w sprawie
warunkow technicznych budowli kolejowych (Dz.U. 2026 poz. 1225). Szczegoly i zrodla: docs/plk-normy.md.

Sprawdza:
  R      promien luku (tab. 9.2), wg rodzaju toru
  D      przechylka (tab. 9.6, wzor 9.8 dla R < 300 m)
  I      niedomiar przechylki I = 11,8 V^2/R - D (tab. 9.7), takze w rozjazdach
  dD/ds  pochylenie rampy przechylkowej (tab. 9.12)
  dD/dt  zmiana przechylki w czasie (tab. 9.13)
  dI/dt  zmiana niedomiaru przechylki w czasie (tab. 9.14)
  dI     nagla zmiana niedomiaru na stykach torow (tab. 9.15)
  L      najkrotsze proste i luki w torach glownych (tab. 9.1)
  i      pochylenie podluzne torow glownych (tab. 10.1)
  Rv     zaokraglenie zalomow niwelety (tab. 10.2)
  rozjazd  skosy zakazane w torach glownych (rozp. 2026 par. 23), rozstaw torow przy polaczeniu (tab. 11.2),
           odcinki posrednie miedzy rozjazdami (pkt 11.3)
  styk   szczeliny, zalamania i skoki przechylki na stykach

Progi: P0 zalecane, P1 normalne, P2 graniczne (wymagaja zgody ZLK). Wynik ponad P1 to "uwaga", ponad P2 "blad".

Poprawia (--napraw PLIK_WYJSCIOWY, zapisuje kopie pierwszego pliku wejsciowego ze zmianami):
  - przechylke ponad dopuszczalna: luk z rampami (od prostej do prostej) skalowany do dopuszczalnej wartosci,
  - predkosc: tam, gdzie I, dD/dt, dI/dt albo dI przekraczaja prog --prog-naprawy, velocity torow obnizone do najwiekszej
    wielokrotnosci 10 km/h, przy ktorej warunki sa spelnione (tak jak ogranicza sie predkosc na sieci).
Geometrii w planie ani niwelety nie zmienia: te miejsca sa w raporcie do poprawienia w edytorze.

Tylko biblioteka standardowa Pythona 3.8+.

Przyklady:
  python plk_audit.py scenery/irt_tory.inc
  python plk_audit.py scenery/moja.scn --raport raport.csv --typ-linii P
  python plk_audit.py scenery/irt_tory.inc --napraw scenery/irt_tory_plk.inc
"""
import argparse
import bisect
import csv
import math
import os
import re
import sys
from collections import defaultdict

LEVELS = ("P0", "P1", "P2")
GAUGE = 1500.0  # mm, rozstaw osi szyn do przeliczenia przechylki z kata


# ---------------------------------------------------------------------------------------------------------------------
# normy (ST-T1-A6, rozp. 2026); kazda funkcja zwraca krotke (P0, P1, P2)

def r_min(kind):
    # tab. 9.2: szlakowe i glowne zasadnicze / glowne dodatkowe i boczne (polaczenia jak boczne)
    return (600.0, 300.0, 180.0) if kind == "main" else (450.0, 180.0, 180.0)


def d_max(kind, radius):
    # tab. 9.6 (nawierzchnia podsypkowa) i wzor 9.8
    if kind == "main":
        limits = [150.0, 150.0, 150.0]
    elif kind == "connection":
        limits = [0.0, 100.0, 110.0]
    else:
        limits = [0.0, 60.0, 100.0]
    if radius <= 200.0:
        limits = [min(a, b) for a, b in zip(limits, (0.0, 60.0, 100.0))]
    elif radius <= 300.0:
        limits = [min(a, b) for a, b in zip(limits, (0.0, 100.0, 150.0))]
    if radius < 300.0:
        limits = [min(v, max(0.0, (radius - 50.0) / 1.5)) for v in limits]
    if kind == "main":
        # przechylka zalecana w torze glownym nie jest zerowa: P0 = P1
        limits[0] = limits[1]
    return tuple(limits)


def i_max(kind, radius):
    # tab. 9.7
    limits = (110.0, 130.0, 150.0) if kind == "main" else (80.0, 100.0, 100.0)
    if radius <= 200.0:
        limits = tuple(min(a, b) for a, b in zip(limits, (50.0, 70.0, 70.0)))
    elif radius <= 250.0:
        limits = tuple(min(a, b) for a, b in zip(limits, (80.0, 100.0, 100.0)))
    return limits


def i_max_switch(speed):
    # tab. 9.7, rozjazdy ze stala krzyzownica
    return (90.0, 110.0, 110.0) if speed <= 160.0 else (60.0, 90.0, 90.0)


def ramp_max():
    return (1.6, 2.0, 2.5)  # tab. 9.12, mm/m


def dd_dt_max():
    return (35.0, 50.0, 60.0)  # tab. 9.13, mm/s


def di_dt_max(speed):
    return (55.0, 70.0, 80.0) if speed <= 200.0 else (55.0, 55.0, 70.0)  # tab. 9.14


def sudden_i_max(kind, speed):
    # tab. 9.15
    if kind == "connection":
        if speed <= 60.0:
            return (130.0, 130.0, 130.0)
        if speed <= 100.0:
            return (100.0, 100.0, 125.0)
        if speed <= 160.0:
            return (80.0, 160.0 - 0.35 * speed, 160.0 - 0.35 * speed)
        return (60.0, 160.0 - 0.35 * speed, 160.0 - 0.35 * speed)
    if kind == "side":
        return (50.0, 100.0, 100.0)
    if speed <= 70.0:
        return (50.0, 50.0, 60.0)
    if speed <= 160.0:
        return (0.0, 64.0 - 0.2 * speed, 50.0 if 64.0 - 0.2 * speed < 50.0 else 64.0 - 0.2 * speed)
    if speed <= 200.0:
        return (0.0, 30.0, 50.0)
    return (0.0, 25.0, 25.0)


def element_min_length(speed):
    # tab. 9.1 (P0 = P1, tabela podaje dwa progi)
    if speed <= 120.0:
        p1, p2 = max(0.35 * speed, 30.0), max(0.25 * speed, 20.0)
    elif speed <= 200.0:
        p1, p2 = 0.40 * speed, 0.30 * speed
    else:
        p1, p2 = 0.65 * speed, 0.55 * speed
    return (p1, p1, p2)


def grade_max(line_type):
    # tab. 10.1, mm/m, tory szlakowe i glowne zasadnicze
    return (6.0, 17.5, 35.0) if line_type == "P" else (6.0, 10.0, 20.0)


def vertical_radius_min(speed):
    # tab. 10.2, linie typu P, M i T
    return (max(0.77 * speed * speed, 5000.0), max(0.35 * speed * speed, 5000.0), max(0.25 * speed * speed, 2000.0))


# tab. 11.2: (promien, skos) -> (najmniejszy rozstaw torow polaczonych [m], predkosc na bok [km/h])
CROSSOVER = {(190.0, 9.0): (3.70, 40), (300.0, 9.403): (4.00, 50), (300.0, 9.0): (4.35, 50), (500.0, 12.0): (4.00, 60),
             (760.0, 14.0): (4.75, 80), (1200.0, 18.5): (4.35, 100), (2500.0, 26.5): (4.80, 130)}
# rozp. 2026 par. 23 ust. 2 pkt 1: skosy zakazane w torach glownych zasadniczych
FORBIDDEN_MAIN = (7.5, 7.0, 6.6, 4.8)


def level_of(value, limits, higher_is_worse=True):
    """None gdy w normie P1; "P1" gdy ponad P1 (uwaga); "P2" gdy ponad P2 (blad)."""
    p0, p1, p2 = limits
    # odczyt z pliku (kat przechylki, zaokraglenia) nie powinien robic przekroczen z wartosci rownych granicy
    eps = max(1e-6, 0.003 * max(abs(p1), abs(p2)))
    if higher_is_worse:
        if value > p2 + eps:
            return "P2"
        if value > p1 + eps:
            return "P1"
    else:
        if value < p2 - eps:
            return "P2"
        if value < p1 - eps:
            return "P1"
    return None


# ---------------------------------------------------------------------------------------------------------------------
# odczyt

TOKEN = re.compile(r"\S+")


class Source:
    """Tekst pliku z tokenami i ich polozeniem, do odczytu i do zapisu zmian w miejscu."""

    def __init__(self, path):
        self.path = path
        with open(path, encoding="utf-8", errors="replace", newline="") as handle:
            self.text = handle.read()
        self.tokens = []  # (start, end)
        position = 0
        text = self.text
        length = len(text)
        while position < length:
            line_end = text.find("\n", position)
            if line_end < 0:
                line_end = length
            comment = text.find("//", position, line_end)
            stop = line_end if comment < 0 else comment
            for match in TOKEN.finditer(text, position, stop):
                self.tokens.append((match.start(), match.end()))
            position = line_end + 1
        self.edits = {}  # token index -> new text
        self.inserts = {}  # token index -> text put before the token

    def token(self, index):
        start, end = self.tokens[index]
        return self.text[start:end]

    def comments(self):
        for line in self.text.splitlines():
            at = line.find("//")
            if at >= 0:
                yield line[at:]

    def save(self, path):
        pieces = []
        last = 0
        for index in sorted(set(self.edits) | set(self.inserts)):
            start, end = self.tokens[index]
            pieces.append(self.text[last:start])
            if index in self.inserts:
                pieces.append(self.inserts[index])
            pieces.append(self.edits.get(index, self.text[start:end]))
            last = end
        pieces.append(self.text[last:])
        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write("".join(pieces))


class Path:
    """Sciezka toru: krzywa Beziera w XZ (Y w gore), z przechylka (roll w stopniach) na koncach."""

    __slots__ = ("track", "index", "p0", "p1", "p2", "p3", "roll", "roll_tokens", "length", "straight", "k", "grade")

    def __init__(self, track, index, numbers, tokens):
        self.track = track
        self.index = index
        start, roll0 = numbers[0:3], numbers[3]
        control1, control2 = numbers[4:7], numbers[7:10]
        end, roll1 = numbers[10:13], numbers[13]
        self.straight = not any(control1) and not any(control2)
        self.p0 = start
        self.p3 = end
        if self.straight:
            self.p1 = [a + (b - a) / 3.0 for a, b in zip(start, end)]
            self.p2 = [a + (b - a) * 2.0 / 3.0 for a, b in zip(start, end)]
        else:
            self.p1 = [a + b for a, b in zip(start, control1)]
            self.p2 = [a + b for a, b in zip(end, control2)]
        self.roll = [roll0, roll1]
        self.roll_tokens = tokens
        self.length = self.measure()
        self.k = [self.curvature(0.0), self.curvature(1.0)]
        self.grade = (end[1] - start[1]) / self.length if self.length > 1e-6 else 0.0

    def point(self, t):
        u = 1.0 - t
        return [u * u * u * a + 3 * u * u * t * b + 3 * u * t * t * c + t * t * t * d for a, b, c, d in zip(self.p0, self.p1, self.p2, self.p3)]

    def measure(self, steps=16):
        total = 0.0
        last = self.p0
        for i in range(1, steps + 1):
            here = self.point(i / steps)
            total += math.hypot(here[0] - last[0], here[2] - last[2])
            last = here
        return total

    def derivatives(self, t):
        u = 1.0 - t
        d1 = [3 * u * u * (b - a) + 6 * u * t * (c - b) + 3 * t * t * (d - c) for a, b, c, d in zip(self.p0, self.p1, self.p2, self.p3)]
        d2 = [6 * u * (c - 2 * b + a) + 6 * t * (d - 2 * c + b) for a, b, c, d in zip(self.p0, self.p1, self.p2, self.p3)]
        return d1, d2

    def curvature(self, t):
        if self.straight:
            return 0.0
        d1, d2 = self.derivatives(t)
        speed = math.hypot(d1[0], d1[2])
        if speed < 1e-9:
            return 0.0
        return (d1[0] * d2[2] - d1[2] * d2[0]) / speed ** 3

    def heading(self, t):
        d1, _ = self.derivatives(t)
        if math.hypot(d1[0], d1[2]) < 1e-12:
            d1 = [b - a for a, b in zip(self.p0, self.p3)]
        return math.atan2(d1[2], d1[0])


class Track:
    __slots__ = ("name", "kind_node", "source", "paths", "velocity", "velocity_token", "end_token", "file", "kind", "template")

    def __init__(self, name, kind_node, source):
        self.name = name
        self.kind_node = kind_node  # normal / switch / ...
        self.source = source
        self.paths = []
        self.velocity = None
        self.velocity_token = None
        self.end_token = None
        self.kind = "main"
        self.template = None


def read_tracks(source, tracks, includes):
    count = len(source.tokens)
    i = 0
    while i < count:
        word = source.token(i)
        if word == "include" and i + 1 < count:
            includes.append(source.token(i + 1))
            i += 2
            continue
        if word != "node" or i + 5 >= count:
            i += 1
            continue
        name = source.token(i + 3)
        if source.token(i + 4) != "track":
            i += 5
            continue
        sub = source.token(i + 5)
        if sub not in ("normal", "switch"):
            i += 6
            continue
        track = Track(name, sub, source)
        try:
            visible = source.token(i + 13)
            j = i + 14 + (6 if visible == "vis" else 0)
            for path_index in range(2 if sub == "switch" else 1):
                numbers = [float(source.token(j + n)) for n in range(15)]
                track.paths.append(Path(track, path_index, numbers, (j + 3, j + 13)))
                j += 15
        except (ValueError, IndexError):
            i += 6
            continue
        while j < count and source.token(j) != "endtrack":
            if source.token(j) == "velocity" and j + 1 < count:
                try:
                    track.velocity = float(source.token(j + 1))
                    track.velocity_token = j + 1
                except ValueError:
                    pass
                j += 2
                continue
            j += 1
        track.end_token = j
        tracks.append(track)
        i = j + 1


def read_templates(source):
    """Rekordy //$r switch edytora: nazwa -> (promien, skos)."""
    found = {}
    for line in source.comments():
        words = line.split()
        if len(words) >= 7 and words[0] == "//$r" and words[1] == "switch":
            try:
                found[words[2]] = (float(words[5]), float(words[6]))
            except ValueError:
                pass
    return found


# ---------------------------------------------------------------------------------------------------------------------
# siec

class End:
    __slots__ = ("path", "at", "point", "neighbours")

    def __init__(self, path, at):
        self.path = path
        self.at = at  # 0 poczatek, 1 koniec
        self.point = path.p0 if at == 0 else path.p3
        self.neighbours = []


def connect(paths, tolerance=0.02):
    ends = []
    grid = defaultdict(list)
    cell = 1.0
    for path in paths:
        for at in (0, 1):
            end = End(path, at)
            ends.append(end)
            grid[(int(math.floor(end.point[0] / cell)), int(math.floor(end.point[2] / cell)))].append(end)
    for end in ends:
        gx, gz = int(math.floor(end.point[0] / cell)), int(math.floor(end.point[2] / cell))
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                for other in grid.get((gx + dx, gz + dz), ()):
                    if other is end or other.path is end.path:
                        continue
                    if math.hypot(other.point[0] - end.point[0], other.point[2] - end.point[2]) <= tolerance and abs(other.point[1] - end.point[1]) < 0.5:
                        end.neighbours.append(other)
    return ends


class Run:
    """Ciag sciezek torow zwyklych polaczonych koniec w koniec, z kierunkiem przejscia kazdej sciezki."""

    def __init__(self):
        self.items = []  # (path, forward)
        self.ends = [None, None]  # co jest za koncami: "switch", "free" albo "track"


def oriented(path, forward):
    """Krzywizna, przechylka i wysokosc na poczatku i koncu sciezki w kierunku przejscia."""
    if forward:
        return path.k[0], path.k[1], path.roll[0], path.roll[1], path.grade
    return -path.k[1], -path.k[0], -path.roll[1], -path.roll[0], -path.grade


def build_runs(tracks, end_of):
    used = set()
    runs = []
    for track in tracks:
        if track.kind_node != "normal":
            continue
        path = track.paths[0]
        if id(path) in used:
            continue
        # cofnij do poczatku ciagu
        current, forward = path, True
        seen = {id(current)}
        while True:
            back = end_of[(id(current), 0 if forward else 1)]
            nexts = [n for n in back.neighbours]
            if len(nexts) != 1 or nexts[0].path.track.kind_node != "normal" or id(nexts[0].path) in seen:
                break
            neighbour = nexts[0]
            # jesli sasiad ma wiecej niz jednego sasiada na tym styku, to rozgalezienie
            if len(neighbour.neighbours) != 1:
                break
            current = neighbour.path
            forward = neighbour.at == 1  # wchodzimy w jego koniec, wiec idzie do przodu, jesli to jego koniec 1
            seen.add(id(current))
        run = Run()
        while True:
            used.add(id(current))
            run.items.append((current, forward))
            ahead = end_of[(id(current), 1 if forward else 0)]
            nexts = ahead.neighbours
            if len(nexts) != 1 or nexts[0].path.track.kind_node != "normal" or id(nexts[0].path) in used or len(nexts[0].neighbours) != 1:
                run.ends[1] = "free" if not nexts else ("switch" if any(n.path.track.kind_node == "switch" for n in nexts) else "track")
                break
            neighbour = nexts[0]
            current, forward = neighbour.path, neighbour.at == 0
        first, first_forward = run.items[0]
        behind = end_of[(id(first), 0 if first_forward else 1)].neighbours
        run.ends[0] = "free" if not behind else ("switch" if any(n.path.track.kind_node == "switch" for n in behind) else "track")
        runs.append(run)
    return runs


# ---------------------------------------------------------------------------------------------------------------------
# sprawdzanie

class Finding:
    __slots__ = ("check", "level", "track", "value", "limit", "unit", "x", "z", "note")

    def __init__(self, check, level, track, value, limit, unit, point, note=""):
        self.check, self.level, self.track = check, level, track
        self.value, self.limit, self.unit = value, limit, unit
        self.x, self.z = point[0], point[2]
        self.note = note


def cant_mm(roll_deg):
    return GAUGE * math.sin(math.radians(roll_deg))


def speed_of(track, default):
    if track.velocity is None or track.velocity == 0:
        return default
    return abs(track.velocity)


class Audit:
    def __init__(self, args, tracks, templates):
        self.args = args
        self.tracks = tracks
        self.templates = templates
        self.findings = []
        self.paths = [p for t in tracks for p in t.paths]
        self.ends = connect(self.paths)
        self.end_of = {(id(e.path), e.at): e for e in self.ends}
        self.cant_sign = self.cant_convention()
        self.runs = build_runs(tracks, self.end_of)
        self.classify()

    def cant_convention(self):
        """Znak kata przechylki, przy ktorym przechylka zmniejsza niedomiar w swoim luku (z danych)."""
        total = 0.0
        for path in self.paths:
            k = 0.5 * (path.k[0] + path.k[1])
            roll = 0.5 * (path.roll[0] + path.roll[1])
            if abs(k) > 1e-5 and abs(roll) > 1e-3:
                total += math.copysign(1.0, k) * math.copysign(1.0, roll) * path.length
        return 1.0 if total >= 0.0 else -1.0

    def classify(self):
        kinds = self.args.rodzaj
        for run in self.runs:
            length = sum(p.length for p, _ in run.items)
            names = [p.track.name for p, _ in run.items]
            speed = max(speed_of(p.track, 0.0) for p, _ in run.items)
            if kinds != "auto":
                kind = kinds
            elif run.ends[0] == "switch" and run.ends[1] == "switch" and length < 150.0:
                kind = "connection"
            elif any(re.search(r"_t\d", n) for n in names) or speed >= self.args.glowny_od:
                kind = "main"
            else:
                kind = "side"
            for path, _ in run.items:
                path.track.kind = kind

    def add(self, check, level, track, value, limit, unit, point, note=""):
        if level is not None:
            self.findings.append(Finding(check, level, track.name, value, limit, unit, point, note))

    # wskaznik niedomiaru ze znakiem: a = 11,8 V^2 k - D (k w 1/m, D ze znakiem luku)
    def signed_i(self, speed, k, roll):
        return 11.8 * speed * speed * k - self.cant_sign * cant_mm(roll)

    def check_runs(self, speed_override=None):
        """Sprawdza wszystkie ciagi; zwraca dla kazdej sciezki najwieksza predkosc spelniajaca prog naprawy."""
        args = self.args
        level_index = LEVELS.index(args.prog_naprawy)
        allowed = {}

        def limit_speed(path, value):
            allowed[id(path)] = min(allowed.get(id(path), 1e9), value)

        def largest_speed(start, ok):
            # najwieksza predkosc (krok 5 km/h w dol od start), przy ktorej warunek jest spelniony
            # predkosc pomaga tylko wtedy, gdy przy 20 km/h warunek jest spelniony; inaczej to wada geometrii
            if not ok(20.0):
                return start
            v = start
            while v > 20.0 and not ok(v):
                v -= 5.0
            return max(v, 20.0)
        for run in self.runs:
            items = run.items
            values = []
            for path, forward in items:
                track = path.track
                speed = speed_override.get(id(path), speed_of(track, args.v)) if speed_override else speed_of(track, args.v)
                k0, k1, r0, r1, grade = oriented(path, forward)
                values.append((path, forward, track, speed, k0, k1, r0, r1, grade))
            for index, (path, forward, track, speed, k0, k1, r0, r1, grade) in enumerate(values):
                kind = track.kind
                mid = path.point(0.5)
                k_mid = 0.5 * (k0 + k1)
                radius = 1.0 / abs(k_mid) if abs(k_mid) > 1e-7 else float("inf")
                d0, d1 = abs(cant_mm(r0)), abs(cant_mm(r1))
                d = max(d0, d1)
                cap = speed  # najwieksza predkosc dla tej sciezki
                if radius < 1e6 and path.length > 0.5:
                    if abs(k0 - k1) <= max(1.5e-5, 0.05 * abs(k_mid)):
                        self.add("R", level_of(radius, r_min(kind), False), track, radius, r_min(kind)[1], "m", mid, kind)
                    self.add("D", level_of(d, d_max(kind, radius)), track, d, d_max(kind, radius)[1], "mm", mid, kind)
                    a = abs(self.signed_i(speed, k_mid, 0.5 * (r0 + r1)))
                    limits = i_max(kind, radius)
                    self.add("I", level_of(a, limits), track, a, limits[1], "mm", mid, f"V {speed:.0f} km/h")
                    d_mid = self.cant_sign * cant_mm(0.5 * (r0 + r1)) * math.copysign(1.0, k_mid)
                    v_i = math.sqrt(max(0.0, (limits[level_index] + d_mid)) / (11.8 * abs(k_mid)))
                    cap = min(cap, v_i)
                # rampa
                if path.length > 0.5:
                    same = r0 * r1 >= 0.0
                    change = abs(d1 - d0) if same else d0 + d1
                    slope = change / path.length
                    self.add("dD/ds", level_of(slope, ramp_max()), track, slope, ramp_max()[1], "mm/m", mid)
                    rate = slope * speed / 3.6
                    self.add("dD/dt", level_of(rate, dd_dt_max()), track, rate, dd_dt_max()[1], "mm/s", mid, f"V {speed:.0f} km/h")
                    if slope > 1e-6:
                        cap = min(cap, dd_dt_max()[level_index] * 3.6 / slope)
                if index + 1 < len(values):
                    nxt = values[index + 1]
                    distance = 0.5 * (path.length + nxt[0].length)
                    v = min(speed, nxt[3])
                    joint = path.point(1.0 if forward else 0.0)
                    # styk
                    gap = math.dist(joint, nxt[0].point(0.0 if nxt[1] else 1.0))
                    if gap > 0.002:
                        self.add("styk", "P2" if gap > 0.02 else "P1", track, gap * 1000.0, 2.0, "mm", joint, "szczelina")
                    turn = abs((path.heading(1.0 if forward else 0.0) + (0 if forward else math.pi)
                                - nxt[0].heading(0.0 if nxt[1] else 1.0) - (0 if nxt[1] else math.pi) + math.pi) % (2 * math.pi) - math.pi)
                    if math.degrees(turn) > 0.05:
                        self.add("styk", "P2" if math.degrees(turn) > 0.5 else "P1", track, math.degrees(turn), 0.05, "deg", joint, "zalamanie w planie")
                    if abs(r1 - nxt[6]) > 0.01:
                        self.add("styk", "P2", track, abs(cant_mm(r1) - cant_mm(nxt[6])), 1.0, "mm", joint, "skok przechylki")
                    # zalom niwelety
                    step = abs(nxt[8] - grade)
                    if step > 0.002 and kind == "main":
                        equivalent = distance / step
                        limits = vertical_radius_min(v)
                        self.add("Rv", level_of(equivalent, limits, False), track, equivalent, limits[1], "m", joint,
                                 f"zalom {step * 1000:.1f} mm/m")
                # pochylenie
                if kind == "main" and path.length > 1.0:
                    limits = grade_max(args.typ_linii)
                    self.add("i", level_of(abs(grade) * 1000.0, limits), track, abs(grade) * 1000.0, limits[1], "mm/m", mid)
                limit_speed(path, cap)
            self.check_elements(values)
            self.check_deficiency(values, level_index, limit_speed, largest_speed)
        return allowed

    def check_deficiency(self, values, level_index, limit_speed, largest_speed):
        """Niedomiar przechylki wzdluz ciagu, probkowany co ok. 1 m z rzeczywistej krzywizny krzywych: przyrosty szybsze niz
        3-krotnosc dopuszczalnego dI/dt to nagle zmiany (dI, sumowane na 3 m), reszta to zmiana ciagla (dI/dt, na bazie 10 m)."""
        samples = []  # (stacja, k, przechylka ze znakiem [mm], predkosc, sciezka)
        station = 0.0
        for path, forward, track, speed, k0, k1, r0, r1, grade in values:
            steps = max(1, int(math.ceil(path.length)))
            for n in range(steps + (1 if path is values[-1][0] else 0)):
                t = n / steps
                k = path.curvature(t if forward else 1.0 - t) * (1.0 if forward else -1.0)
                roll = r0 + (r1 - r0) * t
                samples.append((station + path.length * t, k, self.cant_sign * cant_mm(roll), speed, path))
            station += path.length
        if len(samples) < 3:
            return

        def deficiency(sample, v):
            return 11.8 * v * v * sample[1] - sample[2]

        def split(v_of):
            """Przyrosty niedomiaru miedzy kolejnymi probkami, rozdzielone na nagle i ciagle."""
            sudden, gradual = [], []
            for a, b in zip(samples, samples[1:]):
                v = v_of(a, b)
                step = deficiency(b, v) - deficiency(a, v)
                run = max(b[0] - a[0], 1e-3)
                if abs(step) / run * v / 3.6 > 3.0 * di_dt_max(v)[2] and run < 1.5:
                    sudden.append(step)
                    gradual.append(0.0)
                else:
                    sudden.append(0.0)
                    gradual.append(step)
            return sudden, gradual

        def v_now(a, b):
            return min(a[3], b[3])

        sudden, gradual = split(v_now)
        stations = [s[0] for s in samples]
        count = len(samples)
        # nagle zmiany: suma na oknie 3 m
        j = 0
        total = 0.0
        reported = set()
        for i in range(count - 1):
            while j < count - 1 and stations[j] < stations[i] + 3.0:
                total += sudden[j]
                j += 1
            change = abs(total)
            if change > 1.0:
                a = samples[i]
                kind = a[4].track.kind
                v = min(samples[n][3] for n in range(i, min(j + 1, count)))
                limits = sudden_i_max(kind, v)
                level = level_of(change, limits)
                key = (id(a[4]), level)
                if level is not None and key not in reported:
                    reported.add(key)
                    self.add("dI", level, a[4].track, change, limits[1], "mm", a[4].point(0.5), f"V {v:.0f} km/h")
                if change > limits[level_index] + 0.5:
                    window = [samples[n] for n in range(i, min(j + 1, count))]
                    ks = [w[1] for w in window]
                    ds = [w[2] for w in window]

                    def ok(u, ks=ks, ds=ds, kind=kind):
                        values_u = [11.8 * u * u * k - d for k, d in zip(ks, ds)]
                        return max(values_u) - min(values_u) <= sudden_i_max(kind, u)[level_index] + 0.5

                    best = largest_speed(v, ok)
                    for w in window:
                        if w[3] <= v + 0.1:
                            limit_speed(w[4], best)
            total -= sudden[i]
        # zmiana ciagla: na oknie 10 m
        j = 0
        total = 0.0
        for i in range(count - 1):
            while j < count - 1 and stations[j] < stations[i] + 10.0:
                total += gradual[j]
                j += 1
            run = stations[j] - stations[i]
            if run >= 5.0:
                a = samples[i]
                v = min(samples[n][3] for n in range(i, min(j + 1, count)))
                rate = abs(total) / run * v / 3.6
                limits = di_dt_max(v)
                level = level_of(rate, limits)
                key = (id(a[4]), "t", level)
                if level is not None and key not in reported:
                    reported.add(key)
                    self.add("dI/dt", level, a[4].track, rate, limits[1], "mm/s", a[4].point(0.5), f"V {v:.0f} km/h")
                if rate > limits[level_index] + 0.5:
                    window = [samples[n] for n in range(i, min(j + 1, count))]
                    span = run

                    def ok(u, window=window, span=span):
                        change = (11.8 * u * u * window[-1][1] - window[-1][2]) - (11.8 * u * u * window[0][1] - window[0][2])
                        return abs(change) / span * u / 3.6 <= di_dt_max(u)[level_index] + 0.5

                    best = largest_speed(v, ok)
                    for w in window:
                        if w[3] <= v + 0.1:
                            limit_speed(w[4], best)
            total -= gradual[i]

    def check_elements(self, values):
        """Najkrotsze proste i luki (tab. 9.1) w torach glownych. Elementem jest ciag sciezek o tej samej krzywiznie;
        krzywe przejsciowe zapisane jako ciag krotkich lukow (krzywizna rosnie albo maleje) elementami nie sa, liczy sie
        prosta (krzywizna 0) i luk (krzywizna wieksza niz u obu sasiadow)."""
        elements = []  # [krzywizna, dlugosc, pierwszy, ostatni]
        for item in values:
            k = 0.5 * (item[4] + item[5])
            tolerance = max(1.5e-5, 0.05 * abs(k))
            if elements and abs(k - elements[-1][0]) <= tolerance and abs(item[4] - item[5]) < tolerance:
                elements[-1][1] += item[0].length
                elements[-1][3] = item
            else:
                elements.append([k, item[0].length, item, item])
        for index in range(1, len(elements) - 1):
            k, length, first, last = elements[index]
            track = first[2]
            if track.kind != "main":
                continue
            before, after = elements[index - 1][0], elements[index + 1][0]
            straight = abs(k) < 1e-6 and abs(first[4]) < 1e-6
            arc = abs(k) > abs(before) + 1e-7 and abs(k) > abs(after) + 1e-7 and before * k >= 0 and after * k >= 0
            if not (straight or arc):
                continue
            if straight and (abs(before) < 1e-6 or abs(after) < 1e-6):
                continue
            speed = max(first[3], last[3])
            limits = element_min_length(speed)
            self.add("L", level_of(length, limits, False), track, length, limits[1], "m", first[0].point(0.5), "prosta" if straight else "luk")

    def check_switches(self):
        args = self.args
        switches = [t for t in self.tracks if t.kind_node == "switch" and len(t.paths) == 2]
        for track in switches:
            template = self.templates.get(track.name)
            main, branch = track.paths
            radius = max(abs(1.0 / branch.k[0]) if abs(branch.k[0]) > 1e-7 else 0.0, abs(1.0 / branch.k[1]) if abs(branch.k[1]) > 1e-7 else 0.0)
            if template:
                radius = template[0]
            speed = speed_of(track, args.v)
            if radius > 0:
                i_value = 11.8 * speed * speed / radius
                limits = i_max_switch(speed)
                self.add("I", level_of(i_value, limits), track, i_value, limits[1], "mm", main.p0, f"rozjazd, V {speed:.0f} km/h na bok")
            # rodzaj toru, w ktorym lezy rozjazd: wg torow przy jego koncach drogi zasadniczej
            kinds = set()
            for at in (0, 1):
                for neighbour in self.end_of[(id(main), at)].neighbours:
                    kinds.add(neighbour.path.track.kind)
            if template and "main" in kinds:
                r, ratio = template
                if any(abs(ratio - f) < 0.05 for f in FORBIDDEN_MAIN) or (abs(r - 190.0) < 1 and abs(ratio - 9.0) < 0.05):
                    self.add("rozjazd", "P2", track, ratio, 9.0, "1:n", main.p0, f"Rz {r:.0f}-1:{ratio:g} w torze glownym zasadniczym")
        # polaczenia torow rownoleglych (tab. 11.2) i odcinki posrednie (pkt 11.3)
        by_branch_end = {}
        for track in switches:
            branch = track.paths[1]
            by_branch_end[id(branch)] = track
        for run in self.runs:
            if run.ends != ["switch", "switch"]:
                continue
            length = sum(p.length for p, _ in run.items)
            first, first_forward = run.items[0]
            last, last_forward = run.items[-1]
            a = [n.path for n in self.end_of[(id(first), 0 if first_forward else 1)].neighbours if n.path.track.kind_node == "switch"]
            b = [n.path for n in self.end_of[(id(last), 1 if last_forward else 0)].neighbours if n.path.track.kind_node == "switch"]
            if 0.01 < length < 6.0 - 1e-6:
                self.add("rozjazd", "P1", first.track, length, 6.0, "m", first.point(0.0), "odcinek posredni miedzy rozjazdami")
            if not a or not b:
                continue
            sa, sb = a[0].track, b[0].track
            if sa is sb or a[0].index != 1 or b[0].index != 1:
                continue  # polaczenie przez kierunki zwrotne obu rozjazdow
            template = self.templates.get(sa.name)
            if not template:
                continue
            key = min(CROSSOVER, key=lambda k: abs(k[0] - template[0]) + 100 * abs(k[1] - template[1]))
            if abs(key[0] - template[0]) > 1 or abs(key[1] - template[1]) > 0.05:
                continue
            ma, mb = sa.paths[0], sb.paths[0]
            ha, hb = ma.heading(0.0), mb.heading(0.0)
            if abs(math.sin(ha - hb)) > 0.02:
                continue
            dx, dz = mb.p0[0] - ma.p0[0], mb.p0[2] - ma.p0[2]
            spacing = abs(-math.sin(ha) * dx + math.cos(ha) * dz)
            minimum = CROSSOVER[key][0]
            if spacing < minimum - 0.01:
                self.add("rozjazd", "P2", sa, spacing, minimum, "m", ma.p0, f"rozstaw przy polaczeniu Rz {key[0]:.0f}-1:{key[1]:g} z {sb.name}")


# ---------------------------------------------------------------------------------------------------------------------
# naprawa

def curve_groups(run):
    """Luki z rampami w ciagu: kolejne sciezki od prostej bez przechylki do prostej bez przechylki."""
    groups, current = [], []
    for path, forward in run.items:
        flat = abs(path.k[0]) < 1e-7 and abs(path.k[1]) < 1e-7 and abs(path.roll[0]) < 1e-6 and abs(path.roll[1]) < 1e-6
        if flat:
            if current:
                groups.append(current)
                current = []
        else:
            current.append((path, forward))
    if current:
        groups.append(current)
    return groups


def fix_cant(audit):
    """Przechylka lukow dobrana od nowa w granicach norm (prog naprawy), gdy luk ich nie spelnia, a da sie to zrobic bez
    zmiany predkosci: luk z dwiema krzywymi przejsciowymi (lub bez nich, gdy wystarczy zmniejszyc przechylke), przechylka D
    z przedzialu
        D >= 11,8 V^2/R - Idop                       (tab. 9.7)
        D >= 11,8 V^2/R - dI/dt_dop * 3,6 * Lkp / V   (tab. 9.14)
        D <= Ddop(R)                                 (tab. 9.6, wzor 9.8)
        D <= Lkp * min(dD/ds_dop, dD/dt_dop * 3,6 / V)   (tab. 9.12, 9.13)
    najblizsza obecnej, z rampami prostoliniowymi na krzywych przejsciowych. Zwraca (poprawione luki, luki do naprawy predkoscia)."""
    level = LEVELS.index(audit.args.prog_naprawy)
    fixed = 0
    left = 0
    for run in audit.runs:
        for group in curve_groups(run):
            # krzywizna i przechylka w kierunku przejscia
            data = []
            station = 0.0
            for path, forward in group:
                k0, k1, r0, r1, _ = oriented(path, forward)
                data.append((path, forward, station, path.length, 0.5 * (k0 + k1), r0, r1))
                station += path.length
            total = station
            # luk musi zaczynac sie i konczyc bez przechylki, inaczej zmiana dalaby skok na styku z sasiadem
            if abs(data[0][5]) > 1e-6 or abs(data[-1][6]) > 1e-6:
                continue
            ks = [abs(d[4]) for d in data]
            peak = max(ks)
            if peak < 1e-6:
                continue
            arc = [i for i, k in enumerate(ks) if k >= 0.97 * peak]
            first, last = arc[0], arc[-1]
            # prosty luk: krzywizna jednego znaku, rosnaca do luku i malejaca za nim
            signs = {math.copysign(1.0, d[4]) for d in data if abs(d[4]) > 1e-6}
            if len(signs) != 1:
                continue
            if any(ks[i] < 0.9 * peak for i in range(first, last + 1)):
                continue
            # krzywizna rosnie do luku i maleje za nim (inaczej to luk koszowy albo dwa luki: bez zmian)
            slack = 0.05 * peak
            if any(ks[i + 1] < ks[i] - slack for i in range(0, first)) or any(ks[i + 1] > ks[i] + slack for i in range(last, len(ks) - 1)):
                continue
            radius = 1.0 / peak
            l_in = data[first][2]
            l_out = total - (data[last][2] + data[last][3])
            # predkosc grupy: najwieksza z jej torow
            speed = max(speed_of(d[0].track, audit.args.v) for d in data)
            kind = data[first][0].track.kind
            current = max(abs(cant_mm(d[5])) for d in data[first:last + 1])
            deq = 11.8 * speed * speed / radius
            lower = deq - i_max(kind, radius)[level]
            upper = d_max(kind, radius)[level]
            ramps = [x for x in (l_in, l_out) if x > 0.5]
            if len(ramps) == 2:
                shortest = min(ramps)
                lower = max(lower, deq - di_dt_max(speed)[level] * 3.6 * shortest / speed)
                upper = min(upper, shortest * min(ramp_max()[level], dd_dt_max()[level] * 3.6 / speed))
            else:
                continue  # bez dwoch krzywych przejsciowych przechylki nie ma gdzie rozlozyc: zostaje predkosc
            lower = max(lower, 0.0)
            # stan obecny w granicach?
            inside = lower - 0.5 <= current <= upper + 0.5
            # sprawdz tez, czy rampa obecna pasuje (np. dluzsza od krzywej przejsciowej)
            if inside:
                continue
            if lower > upper + 1e-6:
                left += 1
                continue
            target = min(max(current, lower), upper)
            if target > 0.0:
                # przechylka minimalna 20 mm, gdy w ogole jest (tab. 9.6)
                target = max(target, min(20.0, upper))
            sign = audit.cant_sign * math.copysign(1.0, data[first][4])
            start_arc = data[first][2]
            end_arc = data[last][2] + data[last][3]

            def cant_at(s):
                if ramps and s < start_arc:
                    return target * (s / start_arc if start_arc > 1e-6 else 1.0)
                if ramps and s > end_arc:
                    return target * ((total - s) / (total - end_arc) if total - end_arc > 1e-6 else 1.0)
                return target

            for path, forward, station, length, _, _, _ in data:
                rolls = []
                for s in (station, station + length):
                    d = min(cant_at(s), GAUGE)
                    rolls.append(sign * math.degrees(math.asin(d / GAUGE)))
                if forward:
                    path.roll = [rolls[0], rolls[1]]
                else:
                    path.roll = [-rolls[1], -rolls[0]]
                for e in (0, 1):
                    value = path.roll[e]
                    path.track.source.edits[path.roll_tokens[e]] = f"{value:.6f}".rstrip("0").rstrip(".") if abs(value) > 1e-9 else "0"
            fixed += 1
    return fixed, left


def fix_speed(audit, allowed):
    """Predkosci torow obnizone do wielokrotnosci 10 km/h, przy ktorych spelnione sa I, dD/dt, dI/dt i dI. Liczy w petli,
    bo dI/dt i dI zaleza od predkosci obu sasiednich sciezek."""
    changed = {}
    override = {}
    for _ in range(6):
        worse = False
        # ciagi: najmniejsza dozwolona predkosc w grupie od prostej do prostej
        for run in audit.runs:
            items = run.items
            groups, current = [], []
            for p, f in items:
                curved = abs(p.k[0]) > 1e-7 or abs(p.k[1]) > 1e-7
                if curved:
                    current.append(p)
                else:
                    if current:
                        groups.append(current)
                        current = []
                    groups.append([p])  # prosta (takze z rampa) ma wlasna predkosc
            if current:
                groups.append(current)
            for group in groups:
                cap = min(allowed.get(id(p), 1e9) for p in group)
                speed = max(override.get(id(p), speed_of(p.track, audit.args.v)) for p in group)
                if cap + 1e-6 < speed:
                    new = max(20.0, math.floor(cap / 10.0) * 10.0)
                    if new < speed:
                        for p in group:
                            override[id(p)] = min(override.get(id(p), speed_of(p.track, audit.args.v)), new)
                        worse = True
        if not worse:
            break
        audit.findings = []
        allowed = audit.check_runs(override)
    # zapis do torow: velocity toru = najmniejsza z jego sciezek
    for track in audit.tracks:
        if track.kind_node != "normal":
            continue
        new = override.get(id(track.paths[0]))
        if new is None:
            continue
        old = speed_of(track, audit.args.v)
        if new >= old:
            continue
        sign = -1.0 if (track.velocity or 0) < 0 else 1.0
        text = f"{sign * new:g}"
        if track.velocity_token is not None:
            track.source.edits[track.velocity_token] = text
        else:
            track.source.inserts[track.end_token] = f"velocity {text}\n"
        track.velocity = sign * new
        changed[track.name] = (old, new)
    return changed


# ---------------------------------------------------------------------------------------------------------------------

def resolve(include, roots):
    for root in roots:
        candidate = os.path.normpath(os.path.join(root, include.replace("\\", "/")))
        if os.path.isfile(candidate):
            return candidate
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pliki", nargs="+", help="pliki .scn / .inc z torami")
    parser.add_argument("--raport", help="plik CSV z wszystkimi uwagami i bledami")
    parser.add_argument("--typ-linii", choices=("P", "M", "T"), default="M", help="typ linii dla pochylen (tab. 10.1), domyslnie M")
    parser.add_argument("--rodzaj", choices=("auto", "main", "side", "connection"), default="auto",
                        help="rodzaj torow: auto (glowne: nazwy z _t1/_t2 albo V >= --glowny-od; polaczenia: krotkie miedzy rozjazdami)")
    parser.add_argument("--glowny-od", type=float, default=80.0, help="predkosc, od ktorej tor jest glowny w trybie auto [km/h]")
    parser.add_argument("--v", type=float, default=60.0, help="predkosc torow bez velocity [km/h]")
    parser.add_argument("--bez-include", action="store_true", help="nie czytaj plikow z dyrektyw include")
    parser.add_argument("--napraw", help="zapisz poprawiony pierwszy plik wejsciowy pod ta nazwa")
    parser.add_argument("--prog-naprawy", choices=LEVELS, default="P2", help="prog, ponad ktorym poprawiac (domyslnie P2)")
    args = parser.parse_args()

    sources, tracks, templates = [], [], {}
    queue = list(args.pliki)
    roots = [os.path.dirname(os.path.abspath(args.pliki[0]))]
    roots.append(os.path.dirname(roots[0]))
    seen = set()
    while queue:
        name = queue.pop(0)
        path = os.path.abspath(name)
        if path in seen:
            continue
        seen.add(path)
        source = Source(path)
        sources.append(source)
        includes = []
        read_tracks(source, tracks, includes)
        templates.update(read_templates(source))
        if not args.bez_include:
            for include in includes:
                found = resolve(include, roots + [os.path.dirname(path)])
                if found and found.lower().endswith((".inc", ".scn", ".scm")):
                    queue.append(found)
    if not tracks:
        print("Nie znaleziono torow.")
        return 1

    audit = Audit(args, tracks, templates)
    allowed = audit.check_runs()
    audit.check_switches()
    before = list(audit.findings)

    kinds = defaultdict(int)
    for track in tracks:
        kinds[track.kind if track.kind_node == "normal" else "rozjazd"] += 1
    print(f"Pliki: {len(sources)}, tory: {len(tracks)} ({', '.join(f'{k} {v}' for k, v in sorted(kinds.items()))}), "
          f"ciagi: {len(audit.runs)}, rozjazdy z rekordem //$r: {len(templates)}")

    def summary(findings, title):
        counts = defaultdict(lambda: [0, 0, None])
        for f in findings:
            entry = counts[f.check]
            entry[0 if f.level == "P1" else 1] += 1
            worst = entry[2]
            if worst is None or (f.level == "P2" and worst.level != "P2") or (f.level == worst.level and abs(f.value - f.limit) > abs(worst.value - worst.limit)):
                entry[2] = f
        print(f"\n{title}")
        if not counts:
            print("  wszystko w normie (P1)")
        for check in ("R", "D", "I", "dD/ds", "dD/dt", "dI/dt", "dI", "L", "i", "Rv", "rozjazd", "styk"):
            if check not in counts:
                continue
            p1, p2, worst = counts[check]
            print(f"  {check:8s} uwagi (ponad P1): {p1:6d}   bledy (ponad P2): {p2:6d}   najgorszy: {worst.track} "
                  f"{worst.value:.1f} {worst.unit} przy {worst.limit:.1f} {worst.note}")

    summary(before, "Wynik kontroli:")

    if args.raport:
        with open(args.raport, "w", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle, delimiter=";")
            writer.writerow(["kontrola", "poziom", "tor", "wartosc", "granica_P1", "jednostka", "X", "Z", "uwagi"])
            for f in sorted(before, key=lambda f: (f.check, f.level, f.track)):
                writer.writerow([f.check, "blad" if f.level == "P2" else "uwaga", f.track, f"{f.value:.2f}", f"{f.limit:.2f}", f.unit, f"{f.x:.1f}", f"{f.z:.1f}", f.note])
        print(f"\nRaport: {args.raport}")

    if args.napraw:
        cant, left = fix_cant(audit)
        audit.findings = []
        allowed = audit.check_runs()
        speeds = fix_speed(audit, allowed)
        audit.findings = []
        audit.check_runs()
        audit.check_switches()
        sources[0].save(args.napraw)
        lowered = defaultdict(int)
        for old, new in speeds.values():
            lowered[f"{old:g}->{new:g}"] += 1
        print(f"\nNaprawa (prog {args.prog_naprawy}): przechylka dobrana od nowa w {cant} lukach ({left} lukow bez rozwiazania przechylka), predkosc obnizona na {len(speeds)} torach"
              + (": " + ", ".join(f"{k} km/h x{v}" for k, v in sorted(lowered.items(), key=lambda kv: -kv[1])[:12]) if speeds else ""))
        if len(sources) > 1:
            print("  Uwaga: zapisany jest tylko pierwszy plik; tory z plikow dolaczanych nie sa poprawiane.")
        summary(audit.findings, "Po naprawie:")
        print(f"\nZapisano: {args.napraw}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
