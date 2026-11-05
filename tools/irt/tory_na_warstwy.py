#!/usr/bin/env python3
"""Rozbija irt_tory.inc na pliki warstw .scm edytora: jeden na linie kolejowa, plus jeden na rozjazdy i laczniki.

Plik .inc silnik czyta jako szablon (klocek wstawiany do scenerii): tory z niego nie daja sie w edytorze chwycic,
przesunac ani usunac, a edytor nic do niego nie zapisuje. Tory w plikach .scm dolaczonych ze scenerii sa zwyklymi torami
warstw, jak te polozone w edytorze. Linie sa w osobnych plikach, bo kazda warstwe da sie w edytorze ukryc albo zablokowac.

  python tory_na_warstwy.py irt_tory.inc KATALOG_WYJSCIA [--prefiks irt_tory] [--max-mb 25]

Wypisuje linie include ... end do wklejenia do scenerii (i zapisuje je w <prefiks>_warstwy.txt).
"""
import argparse
import os
import re
import sys

LINE = re.compile(r"^// ---- linia (\S+) \(([^)]*)\)")


def split(text):
    """Naglowek (do pierwszego '// ---- rozjazdy'), rozjazdy z lacznikami, potem sekcje linii (numer, tekst)."""
    lines = text.splitlines(keepends=True)
    head, switches, sections = [], [], []
    part = head
    for line in lines:
        m = LINE.match(line)
        if line.startswith("// ---- rozjazdy"):
            part = switches
        elif m:
            sections.append((m.group(1), [line]))
            part = sections[-1][1]
            continue
        part.append(line)
    return head, switches, sections


def chunks(lines, limit):
    """Dzieli na czesci nie wieksze niz limit bajtow, nie rozcinajac wpisu toru (od 'node' do 'endtrack')."""
    out, cur, size, entry, inside = [], [], 0, [], False
    for line in lines:
        entry.append(line)
        if line.startswith("node "):
            inside = True
        if inside and line.strip() != "endtrack":
            continue
        inside = False
        n = sum(len(x.encode("utf-8")) for x in entry)
        if cur and size + n > limit:
            out.append(cur)
            cur, size = [], 0
        cur.extend(entry)
        size += n
        entry = []
    cur.extend(entry)
    if cur:
        out.append(cur)
    return out


def write_layers(head, switches, sections, outdir, prefix="irt_tory", limit_mb=25.0):
    """Zapisuje warstwy, zwraca nazwy plikow w kolejnosci dolaczania."""
    os.makedirs(outdir, exist_ok=True)
    limit = int(limit_mb * 1024 * 1024)
    names = []
    header = [l for l in head if not l.startswith("//$r")]
    records = [l for l in head if l.startswith("//$r")]

    def dump(name, body, with_records=False):
        path = os.path.join(outdir, name)
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.writelines(header)
            if with_records:
                fh.writelines(records)
            fh.writelines(body)
        names.append(name)

    # rozjazdy razem z ich rekordami //$r: edytor czyta rekordy z warstwy, w ktorej sa ich tory
    dump(f"{prefix}_rozjazdy.scm", switches, with_records=True)
    by_line = {}
    order = []
    for number, body in sections:
        if number not in by_line:
            by_line[number] = []
            order.append(number)
        by_line[number].extend(body)
    for number in order:
        safe = re.sub(r"[^0-9A-Za-z_-]", "_", number)
        parts = chunks(by_line[number], limit)
        for k, part in enumerate(parts):
            suffix = "" if len(parts) == 1 else f"_{k + 1}"
            dump(f"{prefix}_l{safe}{suffix}.scm", part)
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inc")
    ap.add_argument("outdir")
    ap.add_argument("--prefiks", default="irt_tory")
    ap.add_argument("--max-mb", type=float, default=25.0, help="najwiekszy plik warstwy (MB), wieksze linie dzielone na czesci")
    args = ap.parse_args()
    text = open(args.inc, encoding="utf-8").read()
    head, switches, sections = split(text)
    names = write_layers(head, switches, sections, args.outdir, args.prefiks, args.max_mb)
    includes = "".join(f"include {n} end\n" for n in names)
    with open(os.path.join(args.outdir, f"{args.prefiks}_warstwy.txt"), "w", encoding="utf-8") as fh:
        fh.write(includes)
    sys.stdout.write(includes)
    total = sum(os.path.getsize(os.path.join(args.outdir, n)) for n in names)
    print(f"// {len(names)} warstw, {total / 1e6:.1f} MB (bylo {os.path.getsize(args.inc) / 1e6:.1f} MB)", file=sys.stderr)


if __name__ == "__main__":
    main()
