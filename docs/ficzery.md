Gałąź `ficzery` buduje się od nowa na świeżym kodzie chłopaków. Dokłada do niego tylko nasze sprawdzone zmiany i nowy dział **Ficzery** w edytorze.

## Skąd jest kod

- **Hirek:** `feat/gl46-migration` z 1.11 (OpenGL 4.6, raporty awarii, `eu07_starter`) oraz `feat/vhg` (pojazdy z grafów).
- **maj00r:**
  - `feat/switch-blades` (iglice rozjazdów);
  - `fix/imgui-panels-no-auto-detach`;
  - z `feat/track-uneven` opis i narzędzie do oceny nierówności. Jego porządki w `Segment`/`TrackUneven` zostały pominięte, bo gl46 ma te pliki już dalej przerobione.
- **Nasze:**
  - poprawka kursora: punkt ląduje tam, gdzie się kliknie, także gdy kamera się rusza;
  - lista wypełnionych obszarów (Enter / Esc, zapis `//$w`);
  - Magic w rozjazdach (z normami PLK, `editor/editorNorms.hpp`);
  - wycinanie powierzchni terenu.

## Dział Ficzery (F7)

Jest na górnej belce obok Otoczenia, Torów, Dróg i Terenu. Ma dwa narzędzia.

**Wytnij powierzchnię.**
- Obrys rysuje się jak w Wypełnieniu: LPM dodaje punkt, Shift+LPM cofa ostatni.
- Przycisk „Wytnij jako osobną powierzchnię” wyjmuje obszar z terenu i wstawia go z materiałem wybranym w palecie.
- Powierzchnia ma wysokości terenu i nadąża za nimi, gdy teren się zmieni. Własną ma tylko teksturę.

**Torowisko i skarpy.**
- Wzdłuż modyfikatorów torowiska (tych, którymi tory kształtują teren) wycina osobne kawałki, każdy nie dłuższy niż 100 m.
- Rodzaje kawałków:
  - **torowisko:** pas do krawędzi formacji (3,52 m od osi). Przy kilku torach granica wypada w połowie odstępu między nimi.
  - **nasypy i przekopy:** od krawędzi torowiska do miejsca, gdzie skarpa 1:1,5 spotyka teren. Osobno lewa i prawa strona.
- Nazwy wyglądają tak: `L132 t1 torowisko km 012.300-012.400`, `L132 t1 nasyp L km …`.
- Zakres to tory wokół kamery (promień do ustawienia) albo cała sceneria.
- Ponowne uruchomienie zastępuje kawałki wycięte wcześniej w tym samym miejscu.
- Nasypy dalej usypują modyfikatory, kawałki służą tylko do teksturowania.

**Lista „Powierzchnie wycięte z terenu”.**
- Filtr po słowach w nazwie, np. `nasyp` albo `L274 torowisko`.
- Przycisk „Materiał z palety dla wszystkich pokazanych”.
- Przycisk „Idź do”.
- Ctrl+Z cofa każdą zmianę.

Dużo powierzchni nie spowalnia edytora. Budują się tylko te w promieniu 1,5 km od kamery. Przy ponad 200 powierzchniach zapisują się do osobnego pliku `patches.txt` obok `terrain.yaml`.

## Pozostałe

- **Punkt odniesienia scenerii.** Edytor czyta linię `//$g` także z pliku dołączonego przez `include`. Uwaga na narzędzie „Prawdziwy teren z map (NMT)” w dziale Teren: przycisk „Sceneria jest tutaj” przestawia ten punkt, a razem z nim ortofotomapę.
- **Dokumentacja.** Jak powstaje sceneria IRT jest w `docs/irt-konwersja.md`, normy PLK w `docs/plk-normy.md`, literatura w `docs/literatura-kolejowa.md`.
- **Narzędzia.** Konwerter jest w `tools/irt/`, audyt PLK w `tools/plk/`.

## Numer wersji

Każdy build ma kolejny numer, np. `ficzery nr 18`. Numer jest widoczny:
- w tytule okna gry i edytora,
- w logu,
- w pliku `WERSJA.txt` w paczce,
- w tytule wydania na GitHubie.

Numer rośnie o jeden z każdym buildem. Exe zbudowane w domu ma numer z dopiskiem `-dom`.
