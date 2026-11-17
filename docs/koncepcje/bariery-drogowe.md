# Koncepcja: bariery energochłonne przy drogach

Stan: **etap 1 w kodzie** (17.11.2026, `world/RoadBarrier`, sekcja „Bariery ochronne” przy zaznaczonym odcinku drogi); reszta to
koncepcja, wersja 2. Wersja 2 powstała po przejściu kodu edytora dróg pod kątem cofania, ponawiania, Esc, cięcia, sklejania
i przejść między odcinkami. Co się zmieniło względem wersji 1 i dlaczego: pkt 11. Co dokładnie weszło i czym różni się od opisu:
pkt 13.

Cel: bariery stalowe (prowadnica W na słupkach, „SP-05/06”) i betonowe przy drogach z narzędzia Drogi. Kładzione dwoma
kliknięciami wzdłuż wielu odcinków, idą za każdą zmianą drogi, nie wymagają niczego z SVN. Bez kolizji: pojazdy AI jadą po pasach.

## 0. Decyzje

1. Bariera to **opcjonalna właściwość odcinka drogi** (`barrier <opis>` w `road ... endroad`), a nie węzeł ani model.
2. Geometria proceduralna w `world/RoadBarrier.{h,cpp}` (czyste funkcje, testy bez silnika), dokładana w `road_node::create_shapes()`.
3. **Odcinek pamięta tylko własny zakres bariery. Ciągłość z sąsiadem nie jest nigdzie zapisana: wynika z geometrii przy
   budowaniu kształtów.** Jeśli koniec linii bariery jednego odcinka leży w tym samym miejscu co koniec linii bariery sąsiada
   (do 0,3 m, ten sam typ), bariera jest ciągła. Jeśli nie, dostaje zakończenie. To jedna reguła zamiast pilnowania spójności
   w każdej operacji edytora.
4. Zmiana samych barier **nie rusza pasów ruchu** (osobna, lekka ścieżka: `define` + `hide` + `show`), więc działa też z pojazdami
   na drodze i nie odpina przejazdów.

Dlaczego pkt 3 jest najważniejszy: w edytorze jest co najmniej 12 miejsc, które tworzą albo zmieniają odcinki (pkt 6). Gdyby
ciągłość była zapisana w danych (wersja 1: pola `przed` / `po`), każde z nich musiałoby ją poprawiać u sąsiadów, sąsiedzi
trafialiby do rekordu cofania i do zapisu pliku, a zablokowana warstwa sąsiada psułaby wszystko. Przy ciągłości wyliczanej
usunięcie odcinka ze środka bariery samo daje zakończenia po obu stronach dziury, a Ctrl+Z samo je zabiera.

## 1. Co już jest w kodzie

| Potrzeba | Gdzie |
|---|---|
| Rama przekroju w punkcie osi | `road_axis::at(station)`, `axis_frame` (Road.cpp) |
| Przeciąganie przekroju o zmiennych narożnikach | `sweep(...)` z `section_corner`, zamknięcie `cap(...)` |
| Krawędź jezdni z `taper`, `median`, `lanesto` | `state::breadth()`, `middle()`, `median_width()` |
| Kolor bez tekstury | materiał `"colored"` + `lighting_data.diffuse` (jak linia zatrzymania w RoadPoint.cpp) |
| Kształty odcinka osobno od pasów | `road_node::show()` / `hide()` (`owned_shapes`), `define()` nie rusza pasów |
| Cofanie zmian dróg | `editor_road::record` trzyma całe `state` sprzed zmiany; `restore_road_snapshot` |
| Przebudowa po zmianie z sąsiadami | `editor_road::refresh()` (zbiera sąsiadów i skrzyżowania przed zmianą i po niej) |
| Cięcie stanu odcinka | `part()` (odgałęzienia), `split()` (własny kod), `flipped()` (odwrócenie osi), `dissolve()` |

## 2. Przepisy (wartości domyślne, wszystkie do zmiany w UI)

| Parametr | Wartość | Skąd |
|---|---|---|
| Wysokość prowadnicy | 0,75 m | SST D-07.05.01 |
| Słupki | co 2 m lub 4 m, środnik 100–140 mm; przekładka 100–180 mm | SST D-07.05.01 |
| Odległość od krawędzi | ≥ 0,50 m od pobocza utwardzonego; bez niego ≥ 1,00 m od pasa (A, S, GP, G, Z) albo ≥ 0,75 m (L, D); 0,50 m przy krawężniku ≥ 0,12 m | WR-D-22-3 4.3.4 |
| Odcinek początkowy / końcowy | 16 / 12 m przy V ≥ 90 km/h, 12 / 8 m poniżej; najkrócej 4 m; na dwukierunkowej jednojezdniowej oba jak początkowy | WR-D-22-3 4.3.3 ust. 19–22 |
| Odgięcie końców | na zewnątrz, skos ≤ 1:20 (wyjątkowo 1:12) | ust. 13–14 |
| Zakończenie | prowadnica pochylona do ziemi, czoło nie wystaje ponad grunt | ust. 18 |
| Długość przed przeszkodą (LB2) | ≤30 km/h: 10 m, 40–60: 40, 70–80: 60, 90: 80, 100–110: 90, ≥120: 110 | tab. 4.3.3.3 |
| Kiedy bariera na nasypie | > 2,0 m (A, S) albo > 3,5 m (niższe klasy), skarpa stromsza niż 1:3 | rozp. z 1999 r. (archiwalne; obecne odsyła do strefy bez przeszkód, bez jednej liczby) |

## 3. Format w scenerii

```
barrier <strona>:<typ>:<odsunięcie>:<od>:<do>:<początek>:<dł>:<koniec>:<dł>:<materiał>
```

**Dwa tokeny** (słowo i opis sklejony dwukropkami), nie jedenaście. Powody:

- Stary parser drogi na nieznanym słowie loguje `Bad road: unknown property` i idzie dalej token po tokenie. Jedenaście luźnych
  tokenów to jedenaście linii w logu na barierę. Co gorsza, **wartość `median` jako strona bariery zostałaby przez stary parser
  wzięta za właściwość `median`** i zjadłaby cztery kolejne tokeny, robiąc z drogi dwujezdniową ze śmieciowymi szerokościami.
  Sklejony opis nie może trafić w żadne słowo kluczowe.
- Strona pasa dzielącego nazywa się `middle`, nie `median`.

Pola:

- strona: `left`, `right` (patrząc wzdłuż osi odcinka, jak `sides[]`), `middle` (oś pasa dzielącego).
- typ: `wbeam` (jednostronna), `wbeam2` (dwustronna), `concrete`.
- odsunięcie: od krawędzi pasa ruchu do lica prowadnicy, na zewnątrz; dla `middle` od osi pasa dzielącego.
- `od`, `do`: metry wzdłuż osi. **`do = -1` znaczy „do końca odcinka”** i tak zostaje przy każdej zmianie długości.
- początek / koniec: `ramp` (pochylenie do ziemi), `flare` (pochylenie + odgięcie 1:20), `cap` (krótka końcówka) i długość w metrach.
  **Używane tylko wtedy, gdy bariera naprawdę się tam kończy.** Gdy geometria mówi, że idzie dalej u sąsiada, pola śpią.
- materiał: `none` = kolor ocynku / betonu; inaczej tekstura.

Najprostszy i najczęstszy wpis: `barrier right:wbeam:1:0:-1:flare:12:flare:12:none`. Taki sam na każdym odcinku drogi daje jedną
ciągłą barierę z zakończeniami tylko na prawdziwych końcach. **Podział takiego odcinka nie wymaga żadnej arytmetyki**: obie
połówki dostają kopię i wynik jest poprawny.

Wczytywanie: opis rozbity po `:`. Zła liczba pól, nieznana strona lub typ, wartość nieskończona albo NaN: wpis pominięty z jednym
`ErrorLog`, parser zostaje zsynchronizowany (to zawsze jeden token). `export_as_text` pisze wpisy tylko gdy są, posortowane,
więc sceneria bez barier wychodzi bajt w bajt jak dziś.

Normalizacja listy wpisów odcinka (czysta funkcja, wołana w `state::normalize()`, **idempotentna**, bo cofanie i ponawianie
przepuszczają stan przez `define()` za każdym razem):

- `od ≤ 0,25` → 0; `do ≥ długość − 0,25` → −1 (przyciąganie do końców, żeby reguła ciągłości trafiała);
- zakres przycięty do długości, wpis krótszy niż 0,5 m usunięty;
- wpisy tej samej strony, typu, odsunięcia i materiału, które się stykają albo nachodzą, sklejone;
- odsunięcie ≥ 0,25 m, długości zakończeń 0–30 m, najwyżej 16 wpisów na odcinek;
- sortowanie: strona, `od`.

## 4. Geometria

1. **Linia bariery** w stacji s: krawędź jezdni (te same kotwice co `loft`, więc z `taper`, `median`, `lanesto`) + odsunięcie +
   odgięcie zakończenia.
2. **Wysokość**: górna krawędź prowadnicy zawsze 0,75 m nad **płaszczyzną jezdni** przedłużoną do linii bariery (z przechyłką).
   Nie nad chodnikiem ani skarpą. Dzięki temu wysokość jest ciągła na styku odcinków o różnych poboczach (chodnik +0,12 m obok
   pobocza) i nie zależy od `banks[]`. Słupki schodzą do lokalnego podłoża (pobocze, chodnik, skarpa) i 0,25 m głębiej.
3. **Ciągłość i odległość do końca** (jedna funkcja, `barrier_end_distance`): od końca wpisu, który sięga końca odcinka, szuka
   wśród odcinków z barierami wpisu, którego linia kończy się w tym samym punkcie (0,3 m w planie i w pionie, ten sam typ
   i materiał). Idzie dalej, sumując długości, aż znajdzie prawdziwy koniec albo przejdzie 32 m (najdłuższe zakończenie + zapas).
   Licznik kroków (64) chroni przed pętlą na rondzie. Zwraca odległość do końca oraz **styl i długość zakończenia z wpisu, na
   którym bariera się kończy** (nie z własnego), więc zakończenie przechodzące przez styk odcinków ma jeden kształt.
   Odcinki bywają krótkie (`kShortestRoad` = 2 m), więc 12–16 m zakończenia regularnie przechodzi przez kilka.
4. **Krótka bariera**: gdy cała ma mniej niż suma obu zakończeń, oba są skalowane proporcjonalnie. Wysokość to minimum z obu
   ramp, więc nigdy nie wychodzi ujemna długość części środkowej.
5. **Koniec przy skrzyżowaniu** (koniec odcinka leży na ramieniu `junction_node`): zawsze `ramp` bez odgięcia i cofnięcie o 3 m,
   niezależnie od wpisu. Odgięcie wchodziłoby w narożnik skrzyżowania. Wyliczane przy budowaniu, więc działa też dla skrzyżowań
   dorobionych później.
6. **`middle`**: rysowana tylko tam, gdzie pas dzielący ma ≥ 1,2 m. Na odcinku, gdzie jezdnie dopiero się rozchodzą, zaczyna się
   od tej stacji. Wpis zostaje w danych, więc cofnięcie zwężenia przywraca barierę.
7. **Prowadnica**: przekrój W (ok. 10 narożników, 0,31 × 0,08 m) przeciągnięty `sweep`; dwie powierzchnie odsunięte o 4 mm, żeby
   była widoczna z obu stron. Na zakończeniu schodzi liniowo do −0,05 m. Koniec zamknięty `cap`.
8. **Słupki**: zakres na odcinku dzielony na całą liczbę przęseł najbliższą 2 m; słupek na początku każdego przęsła, a na końcu
   zakresu tylko wtedy, gdy bariera tam się kończy. Przy ciągłości słupek na styku stawia więc jeden odcinek, nie dwa.
9. **Droga gruntowa (`rough`)**: bariera jest dokładana **po** pętli, która faluje wierzchołki drogi, więc prowadnica zostaje
   prosta i ciągła na styku drogi gruntowej z asfaltową. Słupki wpuszczone głębiej o wartość `rough`. Barier na gruntowej UI
   nie blokuje.
10. **`radius_()`** odcinka powiększony o odsunięcie + 1 m odgięcia, inaczej odgięty koniec bywa obcinany przy cullingu.
11. Zabezpieczenia przed zdegenerowaną geometrią (CLAUDE.md, zawieszenie GPU): odcinek o długości < 0,01 m, zerowa styczna,
    wartości nieskończone → bariera pominięta.

Trójkąty: ok. 36 na metr prowadnicy przy przekroju co 1 m + 16 na słupek, czyli do ok. 45 tys. na km strony; przy kroku osi 2 m
mniej więcej połowa.

## 5. Edytor: jak się tego używa

### 5.1 Narzędzie „Bariera” (czwarte narzędzie dróg, `tool.tool == 3`)

Osobne narzędzie w pasku, obok Zaznacz / Buduj / Postaw, a nie przycisk w panelu. Wtedy istniejący kod obsługuje je bez nowych
gałęzi: `road_choose_tool` czyści zaznaczenie, a `road_cancel` jest już wołane przy Esc, zmianie narzędzia i cofnięciu.

- Kursor nad drogą podświetla linię, na której stanie bariera: lewa krawędź, prawa krawędź albo oś pasa dzielącego, **ta
  najbliższa kursora**. Żadnego wybierania strony z listy.
- Pierwszy klik: początek. Drugi klik: koniec, także wiele odcinków dalej. Podgląd całej linii na żółto.
- Opcje narzędzia: typ, odsunięcie (domyślne z pkt 2 według pobocza), rozstaw słupków, zakończenia (domyślne z prędkości drogi;
  `velocity < 0`, czyli brak ograniczenia, liczy się jak < 90 km/h), materiał.
- **„Gumka”** jako typ: ten sam gest, ale wycina barierę z zakresu. Tak się robi przerwę na zjazd.
- Przycisk „Cała droga, lewa / prawa” dla zaznaczonego ciągu.

Reguły narzędzia:

| Sytuacja | Co się dzieje |
|---|---|
| Drugi klik na tym samym odcinku przed pierwszym | zakres zamieniony miejscami |
| Drugi klik na innym odcinku | droga między punktami szukana po stykach dokładnie dwóch odcinków, **także przez odcinki zmieniające liczbę pasów** (nie `chain()`: ono na nich staje i nie ma kolejności); limit 256 odcinków |
| Odcinki o przeciwnych osiach po drodze | strona przeliczana na każdym odcinku z kierunku osi |
| Między punktami jest skrzyżowanie albo rozwidlenie | odmowa z komunikatem; po każdej stronie skrzyżowania kładzie się osobno |
| Nowy zakres nachodzi na istniejącą barierę tej strony | nowa wygrywa: stara przycięta do reszty, identyczne sklejone (normalizacja) |
| Zakres krótszy niż 2 m | odmowa |
| Odcinek w zablokowanej warstwie albo sceneria otwarta bez `-edit` (`merged()`) | odmowa całości z nazwą odcinka, nic nie zmienione (jak `road_apply`) |
| Pojazdy na drodze | **nie przeszkadzają** (lekka ścieżka, pkt 5.4) |
| Odcinek z pierwszego kliknięcia zniknął przed drugim (cofnięcie) | kotwica i tak wyczyszczona przez `road_cancel`; dodatkowo sprawdzane `m_editorremoved` |

### 5.2 Budowanie drogi od razu z barierą

W opcjach narzędzia Buduj: „Bariera z lewej / z prawej / w pasie dzielącym” (osobne pole `tool.build_barriers`, domyślnie nic).
Nowe odcinki dostają wpis na całą długość. **Nie jest brane z `tool.settings`**: tam po zaznaczeniu drogi siedzi kopia jej stanu
i każdy nowo budowany odcinek dostałby cudze zakresy częściowe (np. „od 40 do 80 m” na każdym kawałku). Z tego samego powodu
`tool.settings.barriers` jest czyszczone w `road_select(nullptr)`, przy „Carrying on…” i w `roundabout()`.

Lewa i prawa są względem osi, jak pobocza. Przy drodze przedłużanej od początku (`tool.reversed`) oś idzie przeciwnie do
kierunku klikania; podgląd linii pokazuje, gdzie bariera stanie.

### 5.3 Panel zaznaczonego odcinka

Sekcja „Bariery” **poza `render_road_layout`**, z własnym zatwierdzaniem. Powód: `render_road_layout` zwraca „zmieniono” →
`road_apply()`, a ono przy „Zmiany na całą drogę” wpisuje stan zaznaczonego odcinka wszystkim odcinkom ciągu. Bariery byłyby
kopiowane z zakresami. Dlatego:

- `road_apply()` dla odcinków innych niż zaznaczony **zachowuje ich własne bariery** (jak dziś `changes`, `taper`, `banks`);
- lista wierszy (strona, typ, od–do, zakończenia) z przyciskiem „Usuń” w każdym wierszu. **Nie ma „zaznaczonego wiersza”**:
  Del w oknie dróg usuwa zaznaczoną drogę (`road_delete`), więc wiersz reagujący na Del kończyłby się skasowaną drogą;
- pola liczbowe zatwierdzane Enterem (jak reszta panelu, `EnterReturnsTrue`), jedna zmiana = jeden krok historii, a nie krok na
  każdą klatkę przeciągania;
- zmiana dotyczy zawsze tylko zaznaczonego odcinka; na całą drogę służy narzędzie.

### 5.4 Lekka ścieżka zmian i cofanie

- `editor_road::can_restyle(Road)`: jak `can_edit`, ale bez warunków o pojazdach (dostępność, `merged()`, warstwa).
- `editor_road::apply_look(changes)`: `define()` + `mark_dirty()` + `hide()` + `show()` zmienionych odcinków i `hide()` + `show()`
  odcinków zależnych (pkt 5.5). Pasy, połączenia i przejazdy nietknięte.
- `editor_road::record` dostaje pole `look_only`. `restore_road_snapshot` woła wtedy `apply_look` zamiast `apply`. Bez tego
  Ctrl+Z zmiany bariery szłoby pełną ścieżką i zdejmowało pasy spod pojazdów, które wjechały po edycji.
- Rekord zawiera tylko odcinki, których wpisy się zmieniły. Sąsiedzi nie są w rekordzie ani oznaczani jako zmienieni, więc
  zapis pliku przepisuje tylko odcinki z nową barierą (zapis łatami, CLAUDE.md).
- Jedno użycie narzędzia, automatu albo gumki = jeden rekord = jeden Ctrl+Z, niezależnie od liczby odcinków.

### 5.5 Odcinki zależne (jedyny nowy zaczep w istniejącym kodzie przebudowy)

Kształt bariery odcinka zależy od sąsiadów w zasięgu 32 m. `refresh()` dostaje jeden dodatek: po zmianie, dla odcinków
z barierami w zasięgu 32 m wzdłuż styków od każdego zmienianego (zebranych **przed zmianą i po niej**, jak dziś sąsiedzi),
które same nie są zmieniane: `hide()` + `show()`. Ponieważ przez `refresh()` przechodzi wszystko (`create`, `apply`, `remove`,
`revive`, czyli też cofanie i ponawianie), ten jeden zaczep obsługuje wszystkie operacje z pkt 6.

### 5.6 Esc

Kolejność w obsłudze klawisza (istniejąca: okna wyboru → … → `road_cancel()` → narzędzie ≠ 0 wraca do Zaznacz):

1. Otwarta lista propozycji automatu: Esc ją zamyka i nic więcej.
2. Narzędzie Bariera z ustawionym pierwszym punktem: Esc zdejmuje punkt, narzędzie zostaje.
3. Narzędzie Bariera bez punktu: Esc wraca do Zaznacz (istniejący kod).

Punkt 2 to jedyne odstępstwo od narzędzia Buduj, gdzie jeden Esc kończy drogę i od razu wraca do Zaznacz. Przy kładzeniu wielu
barier pod rząd dwa kroki są wygodniejsze; jeśli ma być identycznie jak Buduj, odpada punkt 2 i nie trzeba żadnego nowego kodu.
`road_cancel()` czyści kotwicę w obu wariantach (jest wołane także przy cofnięciu i zmianie narzędzia). Uwaga istniejąca:
`road_cancel()` idzie też przy Esc w polu tekstowym, więc Esc podczas wpisywania odsunięcia zdejmie pierwszy punkt.

## 6. Każda operacja edytora i co robi z barierą

„Samo” znaczy: bez kodu poza zaczepem z pkt 5.5 i normalizacją.

| Operacja (funkcja) | Co robi dziś ze stanem | Bariera | Kod do dopisania |
|---|---|---|---|
| Budowanie (`road_click`) | `state{tool.settings}` na każdy kawałek | z `build_barriers`, cała długość; poprzedni koniec traci zakończenie samo | wyczyścić wpisy z `tool.settings`, wstawić `build_barriers` |
| Przedłużanie drogi („Carrying on…”, `state::section`) | kopia stanu końca | jak wyżej; bariera nie przedłuża się sama | czyszczenie wpisów |
| Przedłużanie jednej jezdni drogi dwujezdniowej | kopia stanu, jedna strona zerowana | brak bariery na nowym; ciągłość z barierą starej jezdni zadziała geometrycznie, jeśli linie się zejdą | czyszczenie wpisów |
| Rondo (`roundabout`) | kopia `Layout` na 4–8 łuków | bez barier; położona później na zewnętrznej stronie nie ma końca, pętlę przerywa licznik | czyszczenie wpisów |
| Zmiana układu, „na całą drogę” (`road_apply`) | stan zaznaczonego do wszystkich odcinków ciągu | każdy zachowuje swoje | jedna linia: `state.barriers = road->definition().barriers` |
| Zmiana liczby pasów, szerokości, pobocza, krawężnika | nowe krawędzie | linia idzie za krawędzią samo | – |
| Dopasowanie sąsiada (`settle`): zwężenie, pas dzielący | zmienia `taper` / `median` sąsiada | samo | – |
| Dopasowanie sąsiada: odcinek zmiany pasów (`settle` → `split`) | tnie sąsiada 30 m od końca | przez `split` | – |
| Podział (K, `split`) | kopia stanu do obu połówek | wpis na całą długość: samo. Wpis częściowy: przeliczony | `barriers_part()` (T → metry przez `stations()`) |
| Odgałęzienie, wjazd w bok drogi (`branch_from` → `part`, `carry_out`) | tnie odcinki pod skrzyżowanie, część usuwa | zakresy przez `part()`; koniec przy skrzyżowaniu dostaje `ramp` z cofnięciem (pkt 4.5) | `barriers_part()` w `part()` |
| Usunięcie punktu (`dissolve`) | skleja dwa odcinki, **czasem odwraca oś zachowanego** (`flipped`) | wpisy obu połówek na jednym odcinku, stykające się sklejone | `barriers_flipped()` w `flipped()`, `barriers_joined()` w `dissolve` |
| Usunięcie punktu na końcu drogi | usuwa ostatni odcinek | sąsiad dostaje zakończenie samo | – |
| Usunięcie odcinka / skrzyżowania (Del) | `m_editorremoved` | sąsiedzi dostają zakończenia samo; Ctrl+Z je zabiera | – |
| Przesunięcie punktu, wysokości, Alt+przeciąganie, okno profilu, „Po terenie” (`road_points_apply`) | zmienia oś | idzie za osią; `do = -1` trzyma koniec; wpis częściowy liczony w metrach od początku osi | – |
| Szerokość w punkcie (`road_point_width`) | `taper` | samo | – |
| Skarpy do terenu (`road_bank_to_ground`) | `banks` | bez wpływu na prowadnicę, słupki do nowego podłoża | – |
| Postawienie przejazdu kolejowego (`roadpoint_place`) | tworzy `roadpoint_node` | **wycina barierę** nad torami + 10 m z każdej strony, w tym samym rekordzie co przejazd | wycięcie + `record.roads` |
| Legacy skrzyżowanie dwóch dróg wczytane jako droga (`as_road`) | nowy stan | bez barier | – |
| Cofnij / ponów dowolnej z powyższych | stany z rekordu przez `apply` / `apply_look` | samo; sąsiedzi przez zaczep | pole `look_only` |

Trzy funkcje arytmetyki zakresów (`barriers_part`, `barriers_flipped`, `barriers_joined`) są czyste i siedzą w `RoadBarrier`,
żeby dało się je testować bez silnika. `editorRoad.cpp` tylko je woła, w trzech miejscach.

## 7. Warunki brzegowe, sprawdzone na kodzie

**Cofanie i ponawianie**

| Przypadek | Wynik |
|---|---|
| Bariera na A → podział A na A, B → Ctrl+Z, Ctrl+Z → Ctrl+Y, Ctrl+Y | cofnięcie podziału przywraca A z całym wpisem (stan z rekordu), ponowienie wstawia pierwszą połowę (stan złapany przy cofaniu) i ożywia B, które przez cały czas trzyma swoją definicję. Zgodne, bo nic o barierze nie jest zapisane u sąsiadów |
| Usunięcie odcinka w środku bariery → Ctrl+Z | `revive` idzie przez `refresh`, sąsiedzi przebudowani, zakończenia znikają |
| Wielokrotne Ctrl+Z / Ctrl+Y tam i z powrotem | stan przechodzi przez `define()` → `normalize()` za każdym razem; normalizacja idempotentna, więc tekst zapisu nie dryfuje (test) |
| Cofnięcie zmiany bariery, gdy na drodze są pojazdy | `look_only` → `apply_look`, pasy nietknięte |
| Cofnięcie przy ustawionym pierwszym punkcie narzędzia | `restore_road_snapshot` woła `road_cancel()`, kotwica znika. Cofnięcie czegoś spoza dróg (teren) jej nie rusza; wskaźnik jest ważny, bo odcinki nie są zwalniane, a `m_editorremoved` sprawdzane przy drugim kliknięciu |
| Historia przycięta do limitu | bez znaczenia: rekord nie odwołuje się do innych rekordów |
| Panel po cofnięciu | `road_select` odświeża `tool.settings` z definicji; lista wierszy rysowana z definicji co klatkę, bez własnych indeksów |
| Lista propozycji automatu otwarta, a drogi zmienione albo cofnięte | każda propozycja sprawdzana przy „Zastosuj” (odcinek istnieje, długość ta sama do 0,5 m); nieaktualne pominięte z licznikiem |

**Przejścia między odcinkami i rodzajami dróg**

| Przypadek | Wynik |
|---|---|
| Asfalt → gruntowa (`rough`) | prowadnica ciągła i prosta (pkt 4.9) |
| Pobocze → chodnik na krawężniku | wysokość ciągła (pkt 4.2), słupki różnej długości |
| Zmiana liczby pasów (`lanesto`) | linia idzie skosem za krawędzią; ciągłość na stykach, bo krawędzie się tam schodzą |
| Sąsiad niedopasowany (komunikat „wasn't fitted”: różne szerokości na styku) | krawędzie rozjechane > 0,3 m → dwa zakończenia obok siebie. Widać od razu, znika po dopasowaniu |
| Jednojezdniowa → dwujezdniowa (pas dzielący od 0) | boczne idą za krawędziami; `middle` zaczyna się od 1,2 m szerokości pasa |
| Odcinki o przeciwnych osiach | reguła ciągłości jest geometryczna, strony nie są porównywane |
| Trzy odcinki w jednym punkcie bez skrzyżowania | ciągłość z tym, którego linia trafia w punkt; przy dwóch kandydatach pierwszy według nazwy (deterministycznie) |
| Różne odsunięcie albo typ po dwóch stronach styku | dwa zakończenia; narzędzie kładzie jednym odsunięciem na całym zakresie, więc powstaje tylko z ręcznej edycji |
| Różne prędkości odcinków | długość zakończenia jest polem wpisu, a nie pochodną prędkości; brana z wpisu, na którym bariera się kończy |
| Zakończenie dłuższe niż ostatni odcinek | przechodzi przez styki (pkt 4.3) |
| Bariera krótsza niż dwa zakończenia | skalowanie (pkt 4.4) |
| Rondo | pętla bez końca, licznik kroków; wjazd dorobiony później wycina barierę przez `part()` |
| Koniec drogi bez niczego | zwykłe zakończenie |
| Odcinek skrócony przeciągnięciem punktu poniżej zakresu wpisu | wpis przycięty; krótszy niż 0,5 m znika; Ctrl+Z przywraca |

**Poza edytorem**

| Przypadek | Wynik |
|---|---|
| Tryb gry | `road_table::create_geometry` woła to samo `create_shapes()`, wszystkie drogi są już wczytane, więc reguła ciągłości działa tak samo; geometria scalona ze scenerią |
| Koszt szukania sąsiada | przegląd liniowy tylko wśród odcinków z barierami, robiony tylko dla odcinków z barierami; indeks końców dopiero, gdy pomiar pokaże potrzebę |
| Stary release | dwie linie `unknown property` na barierę, droga wczytana poprawnie. Do sprawdzenia, czy `road` w ogóle jest w ostatnim wydaniu |
| `serialize_` / `deserialize_` drogi | dziś puste (TODO), nic do zrobienia |

**Czego ta koncepcja nie załatwia (świadomie)**

- Tor położony później w poprzek drogi z barierą: narzędzia torów nie wiedzą o drogach. Wyłapuje to kontrola „bariera przez tor”
  w automacie (pkt 8), nie dzieje się samo.
- Bariera przez skrzyżowanie jednym gestem i bariery na łukach narożników skrzyżowań.
- Cofanie pełną ścieżką (`apply`) dla operacji innych niż bariery zdejmuje pasy także spod pojazdów. To zachowanie istniejące,
  nie zmieniamy go tutaj.

## 8. Automat „Bariery tam, gdzie trzeba”

Lista propozycji z „Przejdź do” i ptaszkami; kładzie dopiero „Zastosuj”, jednym krokiem historii.

1. **Wysoki nasyp**: krawędź korony > 2,0 m (V ≥ 100) / 3,5 m nad terenem przy skarpie stromszej niż 1:3 (próbki co 5 m).
2. **Droga nad torem**: odcinek przecina tor wyżej niż 4 m nad główką szyny; bariera na obiekcie + LB2 z obu stron.
3. **Droga przy torze**: tor bliżej niż odsunięcie + 3 m na długości > 20 m.
4. **Przeszkoda**: czoła i filary obiektów z generatora, słupy sieci trakcyjnej bliżej niż 3 m od krawędzi.

Zawsze: przerwa na przejeździe (10 m przed torem), przy skrzyżowaniu i na końcu drogi; dziury krótsze niż 20 m łączone.
Do tego kontrola istniejących barier: „bariera przez tor”, „bariera z zakończeniem w skrzyżowaniu”.

## 9. Testy

**Jednostkowe** (`tests/unit/test_road_barrier.cpp`):

- normalizacja: idempotentna; przyciąganie do 0 i −1; sklejanie; przycinanie; NaN i nieskończoność odrzucone;
- `barriers_part` + `barriers_joined` po podziale w dowolnym miejscu dają z powrotem wpis wyjściowy (do 1 cm);
- `barriers_flipped` dwa razy = to samo; strona i zakończenia zamienione;
- parser: zła liczba pól, nieznane słowa, pusty opis; zapis → wczytanie → zapis bajt w bajt;
- geometria: 0,75 m na prostej i na łuku z przechyłką; zakończenie schodzi pod grunt i odgina się 1:20; bariera krótsza niż
  zakończenia; odcinek 2 m w środku zakończenia; jeden słupek na styku; pętla (rondo) kończy się licznikiem; brak NaN.

**Selftest edytora** (scenariusz na kopii scenerii, z `wait 15` po `screenshot`): `barrier` (dwa punkty), potem po kolei
podział, usunięcie punktu, usunięcie odcinka, odgałęzienie drogi w środku bariery, przejazd; po każdym kroku Ctrl+Z i Ctrl+Y
i porównanie tekstu zapisu sceny z oczekiwanym. To jest test na „tydzień poprawiania”: każdy wiersz tabeli z pkt 6 jako komenda.

## 10. Kolejność wdrażania

| Etap | Zakres | Gotowe, gdy |
|---|---|---|
| **1** | format, normalizacja, geometria `wbeam` z ciągłością i zakończeniami, zaczep w `refresh()`, `apply_look` + `look_only`, sekcja w panelu, testy jednostkowe | wpisy ręczne rysują się, przeżywają zapis, cofanie i usuwanie sąsiadów |
| **2** | narzędzie Bariera z gumką, `build_barriers`, trzy funkcje zakresów w `split` / `part` / `flipped` / `dissolve`, poprawka `road_apply`, wycięcie przy przejeździe, selftest z tabelą pkt 6 | selftest zielony |
| **3** | automat (reguły 1–3) i kontrole | scena testowa: wiadukt i nasyp z barierami, przejazd z przerwą |
| **4** | `wbeam2`, `concrete`, `middle`, `cap`, reguła 4, krótszy zasięg słupków | – |

Etap 1 jest bezpieczny sam w sobie: dopóki nie ma narzędzia, wpisy powstają tylko ręcznie na całą długość, a te nie potrzebują
arytmetyki zakresów. Funkcje zakresów muszą wejść razem z narzędziem, które jako pierwsze tworzy zakresy częściowe.

## 11. Co zmienił przegląd (wersja 1 → 2)

| Wersja 1 | Problem znaleziony w kodzie | Wersja 2 |
|---|---|---|
| pola `przed` / `po` i końce `none` zapisane u każdego odcinka, `settle_barriers` po każdej operacji | 12 miejsc tworzy lub zmienia odcinki; sąsiedzi musieliby wchodzić do rekordów cofania i do zapisu; zablokowana warstwa sąsiada = nieaktualne dane w pliku | ciągłość wyliczana z geometrii, jeden zaczep w `refresh()` |
| strona `median`, 11 luźnych tokenów | stary parser bierze `median` za właściwość drogi i psuje odcinek | `middle`, opis w jednym tokenie |
| sekcja barier w `render_road_layout` | `road_apply` z „na całą drogę” skopiowałoby zakresy na wszystkie odcinki | osobna sekcja, `road_apply` zachowuje bariery odcinków |
| zmiana bariery przez `editor_road::apply` | zdejmuje i odtwarza pasy, wymaga drogi bez pojazdów, odpina przejazdy | `apply_look`, `look_only` w rekordzie |
| przycisk w panelu z dwoma kliknięciami | nowy stan, który trzeba by osobno czyścić przy Esc, cofaniu i zmianie narzędzia | czwarte narzędzie dróg, czyszczone przez `road_cancel` |
| nowe drogi dziedziczą stan z `tool.settings` | po zaznaczeniu drogi siedzą tam jej zakresy częściowe | osobne `build_barriers`, wpisy czyszczone |
| wysokość nad lokalnym podłożem | uskok 12 cm na styku pobocza z chodnikiem | wysokość nad płaszczyzną jezdni |
| „na gruntowych się nie stawia” | bariera falowałaby z drogą albo urywała się na styku z asfaltem | dokładana po falowaniu, dozwolona |
| ciąg przez `chain()` | staje na odcinkach zmiany pasów i nie ma kolejności | własne przejście po stykach |
| brak | Del przy wierszu listy usuwałby drogę; `radius_()` bez odgięcia; krótka bariera z ujemnym środkiem; pętla na rondzie; koniec w narożniku skrzyżowania; przejazd postawiony na barierze | pkt 5.3, 4.10, 4.4, 4.3, 4.5, 6 |

## 12. Do decyzji

1. Esc w narzędziu Bariera: dwa kroki (pkt 5.6) czy jak Buduj.
2. Rozstaw słupków domyślnie 2 m czy 4 m.
3. Czy w SVN jest tekstura ocynku na domyślny materiał.
4. Czy `road` jest w ostatnim publicznym wydaniu (od tego zależy, czy dwie linie w logu starego release w ogóle kogoś dotyczą).

## 13. Co weszło w etapie 1 (17.11.2026)

- Format `barrier <opis>`, normalizacja, cięcie / odwracanie / sklejanie zakresów (`world/RoadBarrier`, testy
  `tests/unit/test_road_barrier.cpp`).
- Geometria `wbeam` po lewej i prawej: prowadnica, słupki co 2 m, przekładki, zakończenia `ramp` / `flare` / `cap`, ciągłość
  między odcinkami z geometrii, koniec przed skrzyżowaniem (`ramp`, 3 m cofnięcia). `wbeam2`, `concrete` i `middle` są wczytywane
  i zapisywane, ale jeszcze nie rysowane.
- Edytor: sekcja „Bariery ochronne” przy zaznaczonym odcinku (lista, „Dodaj z lewej / z prawej”, „Usuń barierę”), lekka ścieżka
  zmian (`can_restyle`, `apply_look`, `record.look_only`), zaczep w `refresh()` dla odcinków zależnych.
- Funkcje zakresów są już wpięte w `split`, `part`, `flipped` i `dissolve` (plan przewidywał to na etap 2), bo wpis częściowy
  da się wpisać ręcznie w panelu.
- Nowe odcinki i ronda nie dziedziczą barier z `tool.settings`.

Różnice względem opisu:

- Słupki schodzą 1 m pod płaszczyznę jezdni zamiast do lokalnego podłoża (pkt 4.2): bez liczenia przekroju pobocza i skarpy,
  a w typowych przekrojach koniec słupka i tak jest pod ziemią.
- Odcinki zależne (pkt 5.5) są zbierane po odległości końców osi (zasięg + 60 m), nie przejściem po stykach. Prostsze, a
  przebudowuje co najwyżej kilka odcinków za dużo.
- Przy dwóch kandydatach do ciągłości wygrywa pierwszy w kolejności wczytania, nie według nazwy.
- Brak jeszcze: narzędzia „Bariera” i gumki, `build_barriers`, wycięcia przy stawianiu przejazdu, automatu, selftestu z tabelą
  pkt 6.

**Nie było sprawdzane w grze.** Zbudowane GCC i sprawdzone testami jednostkowymi; wygląd (kolor, cieniowanie dwustronnej
prowadnicy, zakończenia na łuku z przechyłką) trzeba obejrzeć w edytorze.
