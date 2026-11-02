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

Jest na górnej belce obok Otoczenia, Torów, Dróg i Terenu. Ma trzy narzędzia.

**Wytnij powierzchnię.**
- Obrys rysuje się jak w Wypełnieniu: LPM dodaje punkt, Shift+LPM cofa ostatni.
- Duży przycisk „Wytnij jako osobną powierzchnię” maluje obszar materiałem z palety prosto w teren. Nie ma dziury ani osobnej siatki, więc nie ma szwu. Wysokości zostają terenu.
- Ustawia się:
  - **szerokość przejścia** w materiał dookoła;
  - **szum krawędzi**, czyli na ile krawędź faluje w obie strony, żeby nie była od linijki;
  - **fale szumu**: małe dają krawędź poszarpaną, duże łagodnie falistą.
- Przejście jest co najmniej 1 m. Po zagęszczeniu chunku do 0,5 m może być węższe, bo malowanie terenu jest zapisane w punktach siatki.

**Torowisko i skarpy.**
- Wzdłuż modyfikatorów torowiska (tych, którymi tory kształtują teren) wycina kawałki nie dłuższe niż 100 m:
  - **torowisko:** pas do krawędzi formacji (3,52 m od osi). Przy kilku torach granica wypada w połowie odstępu między nimi.
  - **nasypy i przekopy:** od krawędzi torowiska do miejsca, gdzie skarpa 1:1,5 spotyka teren. Osobno lewa i prawa strona.
- Kawałki są malowane w teren, tak samo jak wycinane powierzchnie.
- Przejścia ustawia się osobno: torowisko–skarpa (domyślnie 0,5 m) i skarpa–teren (1,5 m). Do tego szum krawędzi.
- Nazwy wyglądają tak: `L132 t1 torowisko km 012.300-012.400`, `L132 t1 nasyp L km …`.
- Zakres to tory wokół kamery (promień do ustawienia) albo cała sceneria.
- Ponowne uruchomienie zastępuje kawałki wycięte wcześniej w tym samym miejscu.
- Nasypy dalej usypują modyfikatory, a kawałki służą tylko do teksturowania.

**Idź do.**
- Mapa jak w narzędziu NMT maj00ra, z kamerą zaznaczoną strzałką w kierunku patrzenia. Może podążać za kamerą. Klik na mapie i „Idź do klikniętego punktu” przenosi tam kamerę.
- Wyszukiwarka miast i wsi z obszaru sieci IRT. Polskie znaki są obojętne. Przycisk „Idź do wyszukanego” albo dwuklik na liście przenosi tam kamerę.
- Zapisane kamery:
  - „Dodaj kamerę” zapamiętuje, gdzie kamera stoi i gdzie patrzy;
  - „Skocz” wraca do zapisanego miejsca;
  - lista zapisuje się obok scenerii w pliku `<sceneria>.kamery`.

**Lista „Powierzchnie wycięte z terenu”.**
- Filtr po słowach w nazwie, np. `nasyp` albo `L274 torowisko`.
- Przycisk „Materiał z palety dla wszystkich pokazanych”.
- Przycisk „Idź do”.
- Dla każdej powierzchni można zmienić przejście i szum.
- Kolejność malowania:
  - po zmianie materiału przejścia do sąsiadów przeliczają się same;
  - torowisko leży na skarpach, a powierzchnie rysowane ręcznie na wierzchu.
- „Oddaj terenowi” maluje w tym miejscu z powrotem pierwszy materiał palety.
- Ctrl+Z cofa każdą zmianę.
- Powierzchnie wycięte wcześniej z dziurą i własną siatką, np. z paczki IRT v5, zamienia w malowane przycisk „Przemaluj je w teren”. Działa wokół kamery albo w całej scenerii.

Ograniczenie: jeden chunk 250×250 m mieści najwyżej 8 materiałów, razem z tymi w przejściach.

## Pozostałe

- **Punkt odniesienia scenerii.** Edytor czyta linię `//$g` także z pliku dołączonego przez `include`. W narzędziu „Prawdziwy teren z map (NMT)” w dziale Teren, w sekcji Położenie, są dwa nowe przyciski:
  - **„Przypisz z kamery”:** punkt kliknięty na mapie to miejsce, gdzie stoi kamera, a z tego wylicza się środek scenerii. Cofa przestawienie przyciskiem „Sceneria jest tutaj”. Wystarczy podlecieć kamerą nad znaną stację, kliknąć ją na mapie i przypisać.
  - **„Środek zapisany w pliku scenerii”:** bierze środek z linii `//$g`.
- **Dokumentacja.** Jak powstaje sceneria IRT jest w `docs/irt-konwersja.md`, normy PLK w `docs/plk-normy.md`, literatura w `docs/literatura-kolejowa.md`.
- **Narzędzia.** Konwerter jest w `tools/irt/`, audyt PLK w `tools/plk/`.

## Numer wersji

Każdy build ma kolejny numer, np. `ficzery nr 18`. Numer jest widoczny:
- w tytule okna gry i edytora,
- w logu,
- w pliku `WERSJA.txt` w paczce,
- w tytule wydania na GitHubie.

Numer rośnie o jeden z każdym buildem. Exe zbudowane w domu ma numer z dopiskiem `-dom`.
