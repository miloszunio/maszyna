# Koncepcja: przebieg drogi w planie i przejścia między drogami

Stan: **etap 1 (B1) w kodzie** (17.11.2026: `editor/editorRoadLayout`, `road_apply`, testy `tests/unit/test_road_layout.cpp`), bez
przechodzenia zmian wyglądu przez odcinki zmiany pasów. Reszta to koncepcja, pisana 17.11.2026 na `ficzery2` (po nr 184) po
przeczytaniu `editor/editorRoad.cpp`, `application/editormoderoad.cpp` i `world/Road.cpp`.

Dwa tematy:

- **A.** Drogi gruntowe (i każde inne) mają dać się poprowadzić w planie dowolnie: po śladzie z ortofoto, z ciasnymi łukami,
  i poprawić po zbudowaniu.
- **B.** Przejścia między drogami różnego rodzaju i o różnej liczbie pasów mają powstawać same i nie psuć sąsiada.

## 1. Co kod robi dziś (ustalenia)

### 1.1 Plan

| Ustalenie | Gdzie | Skutek |
|---|---|---|
| Każde kliknięcie daje **jeden łuk kołowy styczny do poprzedniego kierunku**, który trafia w kliknięty punkt | `editor_road::plan`, gałąź „leaves the start in set direction” | promień wynika z geometrii (`span² / 2·side`), nie z woli użytkownika. Kierunek na końcu łuku odchyla się dwa razy bardziej niż cięciwa, więc przy klikaniu po śladzie kolejne łuki przestrzeliwują raz w lewo, raz w prawo |
| Promień < 6 m albo skręt > 170°: odmowa „The bend is too tight” / „would have to turn back” | `kTightestRadius`, stała dla wszystkich dróg | polna droga z narożnikiem pola (r ≈ 4–5 m) jest nie do zbudowania jednym ruchem |
| Ctrl = „prosto”: cel rzutowany na dotychczasowy kierunek | `Straight` w `plan` | nie da się zrobić prostej w nowym kierunku, czyli załamania |
| Koniec przywiązany z obu stron (dołączenie do drogi): jedna krzywa Béziera z uchwytami = 1/3 cięciwy | `plan`, gałąź z `Direction` i `Enddirection` | kształt bywa przypadkowy, odmowa przy kącie > ok. 84° do cięciwy |
| **Po zbudowaniu punktu nie da się przesunąć w planie.** Punkty mają tylko wysokość (przyciski, Alt+przeciąganie, okno profilu) i szerokość | `road_point_heights`, `road_point_width`; nic nie zmienia x/z osi | zły łuk poprawia się tylko przez usunięcie i zbudowanie od nowa |
| Rodzaj drogi (asfaltowa / gruntowa) **nie jest zapisany**: to ustawienie narzędzia Buduj, które zmienia `markings`, `rough` i limity profilu | `render_road_kind` | nic w planie od niego nie zależy; po zaznaczeniu drogi edytor nie wie, jakiego jest rodzaju |
| Gęstość siatki na łuku liczona z pola `radius` odcinka, nie z krzywizny | `road_axis::step()` | odcinek z nieaktualnym `radius` (prosty z uchwytami, po sklejeniu) na ciasnym łuku wychodzi kanciasty |

### 1.2 Przejścia

| Ustalenie | Gdzie | Skutek |
|---|---|---|
| **„Zmiany na całą drogę” wpisuje cały stan zaznaczonego odcinka wszystkim odcinkom ciągu**, z kilkoma wyjątkami (oś, `taper`, szerokość pasa dzielącego, skarpy, zmiany pasa) | `road_apply`: `auto state{tool.settings}` | ciąg (`chain()`) urywa się tylko na odcinku zmiany pasów i na rozwidleniu. Asfalt i dołączona do niego droga gruntowa o tych samych pasach to jeden ciąg. **Zmiana czegokolwiek na asfalcie (np. wysokości krawężnika) zamienia gruntową w asfaltową**: nawierzchnia, oznakowanie, `rough` = 0 |
| **Budowanie od końca istniejącej drogi zawsze bierze jej układ** | `road_click`: `tool.settings = end.road->definition().section(...)` | nie da się kliknąć końca asfaltu i pojechać dalej gruntową. Lista „Rodzaj drogi” dalej pokazuje „gruntowa”, a powstaje asfalt |
| Po zbudowaniu nikt nie dopasowuje sąsiada | `road_click` nie woła `settle` | nie ma znaczenia dziś (układ jest ten sam), ale będzie po poprawce powyżej |
| Te same pasy, inna szerokość: sąsiad zwęża się **na całej swojej długości** (2 m albo 50 m, jak wypadnie) | `settle` → `taper` sąsiada | krótki sąsiad = skok szerokości, długi = klin na 50 m |
| Inna liczba pasów: sąsiad oddaje **30 m** końca na odcinek zmiany pasów (jeśli ma > 45 m, inaczej cały) | `settle`, `kTransitionLength` | długość nie zależy od prędkości ani od tego, ile pasów przybywa |
| Odcinka zmiany pasów nie da się podzielić, skleić ani wyprowadzić z niego drogi; `chain()` na nim staje | `split`, `dissolve`, `follow`, `chain` | „na całą drogę” nie przechodzi przez poszerzenie na pas do skrętu; zmiana nawierzchni drogi z trzema poszerzeniami to siedem osobnych zmian |
| Najwyżej dwa pasy przybywają naraz; kierunek nie może się skończyć w odcinku | `import`: „gains more than two lanes”, „lanes which lead nowhere” | przejście dwukierunkowa → jednokierunkowa nie istnieje. Jeden pas wspólny dla obu kierunków (typowa polna droga) też nie: pas to jednokierunkowy `TTrack` |
| Nierówność gruntowej kończy się uskokiem do `rough` m na styku z równą drogą i skrzyżowaniem | podpowiedź w `render_road_kind` to przyznaje | widoczny próg na każdym wjeździe z pola |
| Nawierzchnia, pobocza, krawężniki, oznakowanie kończą się na styku odcinków prostą linią | `create_shapes` rysuje każdy odcinek osobno | dla nawierzchni to zgodne z rzeczywistością (krawędź asfaltu), dla pobocza i chodnika wygląda na ucięte |

## 2. Propozycja A: plan

### A1. Tryb „Krzywa po punktach” w narzędziu Buduj

Nowy wybór w opcjach budowania: **„Łuk styczny”** (dzisiejsze zachowanie) albo **„Krzywa po punktach”**. Domyślnie: asfaltowa →
łuk styczny, gruntowa → krzywa po punktach. Wybór użytkownika jest pamiętany. Dla dróg asfaltowych nic się samo nie zmienia.

Zasada: kierunek drogi w punkcie Pᵢ wynika z punktów sąsiednich (krzywa Catmulla-Roma, wariant dośrodkowy: styczna wzdłuż
Pᵢ₊₁ − Pᵢ₋₁, uchwyty po 1/3 cięciwy z każdej strony). Krzywa przechodzi dokładnie przez kliknięte punkty i nie przestrzeliwuje.

Żeby znać kierunek w Pᵢ, trzeba znać Pᵢ₊₁. Dlatego budowanie idzie **o jedno kliknięcie z tyłu**:

- kliknięcie n buduje odcinek od Pₙ₋₂ do Pₙ₋₁ (kierunki na obu końcach są już znane);
- ostatni odcinek, do Pₙ, jest tylko podglądem (linia przerywana), dopóki nie padnie następne kliknięcie albo zakończenie;
- zakończenie (Esc, „Zakończ”, zmiana narzędzia) buduje ostatni odcinek z kierunkiem końcowym wzdłuż cięciwy.

Dlaczego tak, a nie „szkic z wielu punktów zatwierdzany Enterem”:

| | O jedno kliknięcie z tyłu | Szkic całej drogi |
|---|---|---|
| Stan narzędzia poza historią | jeden punkt | cała lista punktów |
| Ctrl+Z | jak dziś: zdejmuje ostatni zbudowany odcinek | trzeba przechwycić Ctrl+Z, żeby zdejmował punkt szkicu, i zablokować Ctrl+Y |
| Esc | jak dziś „kończy drogę” (dobudowuje jeden odcinek) | Esc musiałby zatwierdzać całość; `road_cancel` wołane przy cofaniu nie może nic budować, więc szkic by przepadał |
| Zbudowane odcinki | nigdy nie są potem zmieniane | – |
| Strata przy przerwaniu z zewnątrz | jeden odcinek | wszystko |

Szczegóły:

- **Początek od końca istniejącej drogi albo ramienia skrzyżowania**: kierunek startowy narzucony (jak dziś `tool.hasdirection`).
- **Koniec na istniejącej drodze / skrzyżowaniu** (`plan.snap`, `plan.hasend`): kierunek końcowy narzucony; ostatni odcinek
  budowany od razu, bo nic już nie jest nieznane.
- **Ctrl**: ten odcinek jest prostą między dwoma punktami, a kierunek w obu jego końcach to kierunek tej prostej. To daje
  załamanie z krótkim łukiem po obu stronach zamiast dzisiejszego „prosto przed siebie”.
- **Profil**: bez zmian, `road_profile` działa na odcinkach jednego kroku jak dziś (`refine` + teren + `profile`).
- **Pole `radius`** każdego odcinka = `tightest_radius()` tego odcinka, żeby siatka była wystarczająco gęsta.
- Odcinek dłuższy niż `kPieceLength` (50 m) dzielony jak dziś.

### A2. Promień: ostrzeżenie zamiast odmowy

Dzisiejsze 6 m to jedna stała. Proponowane dwa progi:

- **Geometryczny (odmowa)**: promień osi ≥ połowa szerokości drogi z poboczami + 0,5 m. Poniżej wewnętrzna krawędź i ścieżki
  pasów (`offset_path`) zawijają się na siebie. Dla polnej 3 m to ok. 2 m, dla 2 × 3,5 m z poboczami ok. 5 m.
- **Użytkowy (żółte ostrzeżenie, buduje się)**: z klasy drogi. Klasy już są w oknie profilu (polna … autostrada) dla pochyleń
  i łuków pionowych; dochodzi do nich najmniejszy promień w planie. Wartości do wpisania z WR-D-22-2, nie zgaduję ich tutaj.

W podglądzie odcinek poniżej progu użytkowego jest żółty z promieniem w metrach przy kursorze, poniżej geometrycznego czerwony
z nazwą przyczyny. Tryb „Łuk styczny” dostaje te same progi zamiast stałej 6 m (dla dróg o typowej szerokości wychodzi podobnie).

### A3. Przesuwanie punktów w planie

Punkty da się już zaznaczać (klik, Shift, ramka w oknie profilu). Dochodzi:

- **Przeciągnięcie zaznaczonego punktu LPM** przesuwa go po terenie; zmiana wchodzi po puszczeniu przycisku, jeden krok historii.
  Mechanizm jak dzisiejsze Alt+przeciąganie wysokości (`road_lift_start` / `road_lift_finish`): stan `tool.lifting`, który
  `road_cancel` już czyści przy Esc.
- **Kierunki w punkcie**: zachowane (uchwyty skalowane proporcjonalnie do zmiany cięciw). Przycisk **„Wygładź w punkcie”**
  ustawia kierunek z sąsiadów jak A1. Pole **„Kierunek [°]”** do wpisania.
- **Wysokość**: bez zmian przy przeciąganiu; istniejące „Postaw na terenie” robi resztę.
- Realizacja: `point_changes` + `road_points_apply` (istnieją). `road_points_apply` już sprawdza `can_edit`, zapisuje rekord,
  woła `settle` i obsługuje skrzyżowania, więc cofanie i dopasowanie sąsiadów nie wymagają nowego kodu.

Ograniczenia pierwszej wersji: punkt na ramieniu skrzyżowania się nie przesuwa (odmowa z komunikatem); przesunięcie, po którym
któryś odcinek schodzi poniżej progu geometrycznego, jest odrzucane w całości.

### A4. Siatka na ciasnych łukach

`road_axis::step()` bierze mniejszą z dwóch wartości: dzisiejszej (z pola `radius`) i liczonej z największej krzywizny osi.
Siatka może być tylko gęstsza niż dziś, nigdy rzadsza, więc istniejące drogi nie zmieniają kształtu poza wygładzeniem kantów.

## 3. Propozycja B: przejścia

### B1. „Zmiany na całą drogę” przenoszą tylko to, co zmieniono

To jest poprawka błędu, nie ulepszenie. Zamiast wpisywać cały stan i przywracać wyjątki, `road_apply` porównuje stan zaznaczonego
odcinka **przed zmianą i po niej** i przenosi na pozostałe odcinki tylko pola, które się różnią.

- Zmiana krawężnika na asfalcie nie rusza nawierzchni, `rough` ani oznakowania sąsiedniej gruntowej.
- Lista pól jest jawna (tabela w kodzie: nawierzchnia, długość i obrót tekstury, pobocza, krawężniki, oznakowanie, tarcie,
  otoczenie, `rough`, pasy, prędkość…). Pole, którego ktoś zapomni dopisać, **nie jest przenoszone**. To bezpieczna strona
  błędu: dziś nowe pole stanu jest domyślnie kopiowane wszystkim, czego przykładem byłyby bariery.
- Pola „wyglądu” (wszystko poza pasami, szerokością pasa i prędkością) przechodzą **także przez odcinki zmiany pasów**: do tego
  osobne przejście po stykach zamiast `chain()`. Zmiana nawierzchni drogi z poszerzeniami to wtedy jedna zmiana.
- Pola pasów zatrzymują się jak dziś.

### B2. Budowanie od końca drogi własnym układem

W opcjach budowania pole **„Przedłużaj układem istniejącej drogi”** (domyślnie włączone = dzisiejsze zachowanie).
Wyłączone: `tool.settings` zostaje, a po zbudowaniu pierwszego odcinka wołane jest `settle()` na nim, w tym samym rekordzie.
Przejście (zwężenie albo odcinek zmiany pasów) powstaje samo, Ctrl+Z zabiera drogę razem z przejściem.

Przełączenie „Rodzaju drogi” na inny niż ma droga, od której się zaczyna, wyłącza to pole samo, z komunikatem w pasku stanu.
Żeby to rozpoznać, rodzaj istniejącej drogi jest **wyliczany** (`rough > 0` albo brak oznakowania przy nawierzchni innej niż
domyślna = gruntowa), nie zapisywany. Bez zmiany formatu.

### B3. Przejście o określonej długości

- **Zwężenie / poszerzenie przy tych samych pasach**: zamiast całego sąsiada, jego koniec o długości L staje się osobnym odcinkiem
  i tylko on się zwęża (ten sam wzorzec, którym `settle` już wycina odcinek zmiany pasów przez `split`). Sąsiad krótszy niż
  1,5 · L zwęża się cały, jak dziś.
- **Długość L** zamiast stałych 30 m: z różnicy szerokości i prędkości, skos 1:n. Proponowane n: 10 do 50 km/h, 15 do 70,
  20 powyżej; gruntowa 5. **To wartości robocze, do zastąpienia tymi z WR-D-22-2**; L nie krótsze niż 6 m i nie dłuższe niż 60 m.
- Pole „Długość przejścia [m]” przy zaznaczonym odcinku przejściowym do ręcznej zmiany (przesuwa punkt podziału).

### B4. Nierówność gruntowej wygaszana przy styku

Amplituda `rough` spada do zera na ostatnich 8 m (albo na połowie odcinka, jeśli krótszy) przed końcem odcinka, przy którym leży:
skrzyżowanie, przejazd kolejowy, droga o `rough` = 0 albo o innym `rough` / `roughscale`. Przy sąsiedzie o tych samych wartościach
bez zmian (szum jest wspólny dla terenu, więc odcinki się schodzą).

Wyliczane przy budowaniu kształtów z sąsiadów, tak jak ciągłość barier w `bariery-drogowe.md` (pkt 0.3 i 5.5): **nic nie jest
zapisywane**, a ten sam zaczep w `refresh()` przebudowuje odcinki zależne. Cofanie, usuwanie i dobudowanie sąsiada działają bez
dodatkowego kodu.

### B5. Czego nie ruszamy (świadomie)

- **Krawędź nawierzchni** na styku asfaltu z gruntem zostaje prostą linią. Tak to wygląda w rzeczywistości. Rozmyte przejście
  wymaga półprzezroczystej nakładki z tekstury z SVN; możliwe później jako opcja z zachowaniem dzisiejszego wyglądu bez tekstury.
- **Wygaszanie pobocza, chodnika i krawężnika** przed stykiem z drogą, która ich nie ma: wymaga szerokości pobocza osobno na
  początku i końcu odcinka (dziś jedna liczba), czyli rozszerzenia formatu. Osobny etap.
- **Jeden pas dla obu kierunków i przejście dwukierunkowa → jednokierunkowa**: to model ruchu (pas = jednokierunkowy tor), nie
  rysowanie. Polna droga zostaje 1 + 1 z wąskimi pasami.

## 4. Przegląd funkcjonalny

**Jak wygląda praca po zmianach**

1. Polna po śladzie z ortofoto: Buduj → Gruntowa → klikanie punktów po śladzie. Droga rośnie o jeden odcinek za kursorem,
   przechodzi przez każdy punkt. Narożnik pola: dwa punkty blisko siebie albo Ctrl na krótkim odcinku. Esc kończy.
2. Zjazd z asfaltu na polną: klik w bok asfaltu (skrzyżowanie, jak dziś) albo klik w koniec asfaltu z „Gruntowa” → przejście
   szerokości samo, próg nierówności wygaszony.
3. Poprawka łuku po fakcie: Zaznacz → klik w punkt → przeciągnięcie. Ctrl+Z cofa.
4. Zmiana nawierzchni całej drogi z poszerzeniami: jedna zmiana z „na całą drogę”; gruntowe odgałęzienie nietknięte.

**Przypadki brzegowe**

| Przypadek | Zachowanie |
|---|---|
| Krzywa po punktach: jedno kliknięcie i Esc | nic nie powstaje (jak dziś) |
| Dwa kliknięcia i Esc | jeden odcinek prosty |
| Ctrl+Z w trakcie klikania | jak dziś: zdejmuje ostatni zbudowany odcinek, `road_cancel` kończy budowanie i porzuca odcinek w podglądzie. Ponowne kliknięcie w koniec drogi wznawia |
| Ctrl+Y potem | przywraca zdjęty odcinek; odcinek z podglądu nie istniał w historii, więc nie wraca |
| Esc w polu tekstowym | `road_cancel` idzie dziś także wtedy; ostatni odcinek ma zbudować tylko Esc poza polem tekstowym, zmiana narzędzia i „Zakończ” |
| Zmiana trybu (łuk ↔ krzywa) albo rodzaju w trakcie klikania | dotyczy odcinków od następnego kliknięcia; oczekujący odcinek budowany starym trybem |
| Dwa punkty w tym samym miejscu (< 2 m, `kShortestRoad`) | drugi odrzucony z komunikatem, jak dziś „Too short” |
| Nawrót (trzy punkty w ciasnym „V”) | krzywa robi pętlę o małym promieniu → próg geometryczny, odcinek czerwony, nie buduje się; komunikat wskazuje punkt |
| Koniec na skrzyżowaniu, którego ramię ma inne pasy | odmowa jak dziś (`plan.error`), nic nowego |
| Start z boku drogi (nowe skrzyżowanie) w trybie krzywej | pierwszy odcinek ma kierunek ramienia; skrzyżowanie powstaje z pierwszym zbudowanym odcinkiem, czyli **przy drugim kliknięciu po starcie**, nie przy pierwszym. Podgląd pokazuje je wcześniej |
| „Po terenie” wyłączone (rampa, wiadukt) | bez wpływu, profil liczony jak dziś |
| Przeciągnięcie punktu wspólnego trzech odcinków | wszystkie końce w punkcie idą razem (`point_changes` bierze wszystkie `ends_at`) |
| Przeciągnięcie punktu przy przejeździe kolejowym | przejazd zostaje w miejscu; jeśli droga spod niego odjechała, żółty komunikat z nazwą przejazdu |
| Przeciągnięcie przy pojazdach na drodze | odmowa `can_edit`, jak każda zmiana osi dziś |
| „Na całą drogę”: zmiana pasów na zaznaczonym, gdy ciąg zawiera gruntową | pasy przenoszone na cały ciąg jak dziś (to jedna droga); nawierzchnia i `rough` nie, bo się nie zmieniły |
| „Na całą drogę”: użytkownik chce jednak przemalować także gruntową część | zaznacza ją i zmienia; albo zmienia nawierzchnię na asfalcie, co jest przenoszone, bo to pole się zmieniło. Czyli: **przenosi się to, co ruszono** |
| Gruntowa między dwoma skrzyżowaniami krótsza niż 16 m | wygaszenia z obu końców spotykają się w połowie; środek ma pełną amplitudę tylko w jednym punkcie |
| Dwie gruntowe o różnym `rough` | zero na styku, z obu stron |
| Przejście B3 na sąsiedzie krótszym niż L | cały sąsiad, jak dziś |
| Sąsiad w zablokowanej warstwie albo z pojazdami | „wasn't fitted” jak dziś; droga powstaje bez przejścia, komunikat mówi dlaczego |

## 5. Przegląd techniczny

**Gdzie wchodzi kod**

| Element | Plik | Rozmiar zmiany |
|---|---|---|
| A1 styczne i odcinki krzywej (`plan_through`: trzy punkty + opcjonalne kierunki końców → odcinki) | `editorRoad.cpp`, czysta funkcja obok `plan` | nowa, ok. 80 linii, testowalna bez silnika po wydzieleniu z `plan` do pliku bez zależności |
| A1 punkt oczekujący, podgląd, budowanie przy kliknięciu i zakończeniu | `editormoderoad.cpp`: `road_tool` (+2 pola), `road_preview`, `road_click`, obsługa Esc | średnia; `road_click` po decyzji „co budować” idzie dalej istniejącą ścieżką (skrzyżowania, `create`, rekord) |
| A2 progi | `plan` i `plan_through` zwracają promień zamiast błędu; ocena w `road_preview` | mała |
| A3 przeciąganie | `editormoderoad.cpp`, wzorem `road_lift_*`; routing myszy w `editormode.cpp` obok Alt+LPM | średnia, bez nowego kodu historii |
| A4 krok siatki | `road_axis::step()` | kilka linii |
| B1 przenoszenie różnic | `road_apply` + tabela pól | średnia; zastępuje dzisiejsze ręczne wyjątki |
| B2 | `road_click` (warunek przy „Carrying on”, `settle` po `create`) | mała |
| B3 | `settle`: gałąź „te same pasy” dostaje `split` jak gałąź „inne pasy”; `kTransitionLength` → funkcja | mała do średniej |
| B4 | `create_shapes` (mnożnik amplitudy z odległości od końców) + zaczep w `refresh` wspólny z barierami | mała |

**Historia i Esc**

- A1 nie dodaje nowego rodzaju wpisu historii. Każde kliknięcie to jeden `record` z `roads_created` (i skrzyżowaniem), jak dziś.
- Jedyna nowa reguła: **`road_cancel()` porzuca odcinek oczekujący i nigdy nie buduje.** Jest wołane z `restore_road_snapshot`,
  a budowanie w środku cofania dopisałoby wpis do historii i wyczyściło ponawianie. Budowanie ostatniego odcinka robi osobna
  funkcja `road_finish()`, wołana z trzech miejsc: Esc (poza polem tekstowym), przycisk „Zakończ”, `road_choose_tool`.
  W obsłudze Esc `road_finish()` musi stać **przed** bezwarunkowym `road_cancel()`.
- Zamknięcie okna dróg (`m_roadtool.window = false`, trzy miejsca w `editormode.cpp` i jedno w `editormodetrack.cpp`): odcinek
  oczekujący przepada. Do decyzji, czy tam też wołać `road_finish()` (pkt 8).
- A3: stan przeciągania czyszczony przez `road_cancel` (Esc w trakcie = nic się nie zmienia), zapis przez `road_points_apply`.
- B2: `settle` po `create` dopisuje do tego samego rekordu przez `remember`, więc jeden Ctrl+Z.
- B3: `split` wewnątrz `settle` już dziś trafia do rekordu (`roads` + `roads_created`).
- B1: rekord jak dziś (`record.roads` dla każdego zmienionego odcinka).

**Zgodność (CLAUDE.md)**

- Format scenerii: **bez zmian** w całej koncepcji. Krzywa po punktach to zwykłe odcinki Béziera; rodzaj drogi i wygaszenie
  nierówności są wyliczane.
- Wygląd istniejących scenerii: B4 zmienia wygląd styków dróg gruntowych, A4 zagęszcza siatkę. Drogi gruntowe istnieją od
  13.11 tylko na `ficzery2`, więc w wydaniu nie ma czego zmieniać; A4 tylko wygładza. Przełącznik w `eu07.ini` niepotrzebny, ale
  w PR trzeba to napisać wprost.
- Domyślne zachowanie narzędzia dla dróg asfaltowych zostaje (łuk styczny, przedłużanie układem drogi).
- Renderery i shadery: nietknięte.
- Sonar: `road_click` ma już dużą złożoność; nowa gałąź trybu jako osobna funkcja, która zwraca „co zbudować teraz”, a nie
  kolejne `if` w środku (S3776, S134). Tabela pól w B1 jako `std::array` par wskaźników na składowe albo małych funkcji, bez lambd
  ponad 20 linii (S1188).

**Ryzyka**

| Ryzyko | Ocena | Zabezpieczenie |
|---|---|---|
| B1 zmienia zachowanie, do którego ktoś się przyzwyczaił (zmiana jednego pola „wyrównywała” resztę ciągu) | realne | komunikat w pasku stanu wymienia przeniesione pola: „Zmieniono na 12 odcinkach: krawężnik” |
| B1: pole pominięte w tabeli nie przenosi się | małe, widoczne od razu | test: każda składowa `road_node::state` jest w tabeli albo na liście „tylko dla odcinka” (statyczne sprawdzenie liczby pól) |
| A1: pojazdy AI na łuku 3–4 m | nieznane | sprawdzić w grze prędkość i zachowanie na pasie o takim promieniu, zanim próg geometryczny zejdzie poniżej 6 m |
| A1: skrzyżowanie startowe powstaje kliknięcie później niż dziś | drobne, ale zaskakujące | podgląd skrzyżowania od pierwszego kliknięcia; komunikat „skrzyżowanie powstanie z pierwszym odcinkiem” |
| A3: przesunięcie punktu przy skrzyżowaniu przez `settle` zmienia ramię | istniejące zachowanie przy zmianie wysokości | punkty na ramionach wyłączone w pierwszej wersji |
| B3: więcej krótkich odcinków w scenerii | małe | L ≥ 6 m; odcinek przejściowy powstaje tylko przy faktycznej różnicy szerokości > 1 cm (jak dziś warunek 0,001) |
| B4 i bariery dzielą zaczep w `refresh` | zależność między dwiema koncepcjami | zaczep jako jedna funkcja „odcinki, których kształt zależy od sąsiadów”, wdrażana raz, z tym, co wejdzie pierwsze |

## 6. Testy

**Jednostkowe** (funkcje czyste):

- `plan_through`: krzywa przechodzi przez punkty (do 1 mm); ciągłość kierunku na stykach; punkty współliniowe → proste; narzucony
  kierunek początku i końca; Ctrl; nawrót wykrywany progiem; brak NaN przy punktach zdublowanych.
- próg geometryczny: dla każdej szerokości wewnętrzna krawędź nie zawija się przy promieniu progowym.
- B1: zmiana jednego pola przenosi dokładnie to pole; ciąg asfalt + gruntowa po zmianie krawężnika ma niezmienione `surface`,
  `markings`, `rough` na gruntowej.
- B3: długość przejścia z tabeli; sąsiad krótszy niż 1,5 · L zwęża się cały.
- B4: amplituda 0 na styku i pełna 8 m dalej; odcinek krótszy niż 16 m.

**Selftest edytora** (kopia scenerii, `wait 15` po `screenshot`): budowa krzywą po punktach (5 punktów, Esc), Ctrl+Z ×3,
Ctrl+Y ×3; start od końca asfaltu z „Gruntowa”; przeciągnięcie punktu i cofnięcie; zmiana krawężnika „na całą drogę” na ciągu
z gruntową; po każdym kroku porównanie tekstu zapisu sceny.

## 7. Kolejność

| Etap | Zakres | Dlaczego w tej kolejności |
|---|---|---|
| **1** | B1 (przenoszenie różnic) | naprawia psucie dróg gruntowych przy każdej zmianie „na całą drogę”; bez tego reszta jest budowaniem na piasku. Warunek wstępny barier (zastępuje tam „jedną linię w `road_apply`”) |
| **2** | A1 + A2 + A4 (krzywa po punktach, progi, siatka) | największy zysk dla dróg gruntowych, bez zmian w istniejących drogach |
| **3** | A3 (przesuwanie punktów) | poprawianie tego, co już stoi |
| **4** | B2 + B3 (własny układ od końca drogi, długość przejścia) | wymaga 1 |
| **5** | B4 (wygaszanie nierówności) | razem z zaczepem dla barier albo zaraz po nim |

## 8. Do decyzji

1. Esc w trybie krzywej dobudowuje ostatni odcinek (proponowane, zgodne z dzisiejszym „Esc kończy drogę”) czy go porzuca.
2. Zamknięcie okna dróg w trakcie klikania: dobudować ostatni odcinek czy porzucić.
3. Przeciąganie punktu zwykłym LPM (proponowane) czy z klawiszem; zwykły LPM na punkcie dziś tylko zaznacza.
4. Skosy przejść (B3) i najmniejsze promienie klas (A2): kto wyciąga liczby z WR-D-22-2 i czy trzymamy je w tym samym miejscu
   co klasy profilu.
5. Czy próg geometryczny ma zejść poniżej 6 m od razu, czy dopiero po sprawdzeniu pojazdów AI w grze.
