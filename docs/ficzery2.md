Gałąź `ficzery2` (od 18.11 to samo co `integ/staging-weather`) to **build-staging** (gałąź `staging` z 18.11, commit `dd733861`, wydanie
[build-staging](https://github.com/MaSzyna-EU07/maszyna/releases/tag/build-staging)) **plus wszystko nasze**: renderery
OpenGL 4.6 i Vulkan z `feat/gl46-migration`, cała `ficzery2` (nasze wdrożenia, najnowsze gałęzie maj00ra i jerrrrycho)
oraz pogoda z porami roku z `feat/weather-seasons`.

To punkt odniesienia do build-staging: wszystko, co jest w build-staging, jest i tutaj (0 commitów za staging). Różnica
to rzeczy opisane niżej w częściach 2–4. Przy każdej pozycji jest PR w `MaSzyna-EU07/maszyna` i jego autor. „Bez PR” znaczy,
że rzecz jest tylko w naszych gałęziach i PR trzeba dopiero wystawić.

Jak powstała: `feat/weather-seasons` + `feat/gl46-migration` (master z 13.11 w środku), commit `5608e7de`; potem
+ `build-staging`, commit `5505778e`. Testy jednostkowe: 890/890 (MSVC, Release).

## 1. Ze staging: PR-y wcielone od 8.11

To jest w build-staging i tutaj tak samo.

**Kabina, pojazdy, fizyka (jerrrrycho)**

| PR | Co i jak działa |
|---|---|
| #267 | Programator świateł gasi światła od strony sprzęgniętej, nie przeciwnej |
| #268 | Bistabilna dźwignia cewki podtrzymującej wyłącznika szybkiego |
| #269 | Dym z wydechu spalinowego dopiero, gdy silnik zaskoczy |
| #270 | `fpslimit`: klatki odmierzane w całych tykach zegara, pokazywana prawdziwa liczba FPS |
| #271 | Znacznik położenia nastawnika w lokomotywach spalinowych |
| #272 | Wyłącznik szybki przed pantografami (`Cntrl.PantRequiresMain` w `.fiz`) |
| #273 | 6Dg: zadawanie mocy co 5% |
| #274 | Elf: pozycja hamowania nagłego w nastawniku |
| #275 | SM42: tryb manewrowy przy pierwszym ruchu przełącznika, CA wyłączone w manewrowym |
| #276 | ET42: styczniki liniowe blokowane z wentylatorami sekcji, lampka wentylatorów własnej sekcji; ET22: lampka żaluzji |
| #278 | Bocznikowanie zablokowane na zerze i na pozycjach oporowych |
| #279 | Brak sterowania wielokrotnego między elektryczną a spalinową |
| #280 | Bezpiecznik radia odcina jego zasilanie |
| #281 | AI: zalecany układ pantografów do manewrów w `.fiz` |
| #282 | Rewident AI ustawia nastawy hamulca G/P/R według instrukcji Cw-1 |
| #283 | Licznik kilometrów zapamiętywany dla wszystkich prędkościomierzy |
| #284 | Programator świateł: flaga oświetlenia kabiny dla każdej pozycji |
| #285 | Przyrządy kabiny z więcej niż jednym podmodelem |
| #286 | Pozycja ściemniacza reflektorów trzymana w zakresie `DimmerList` |
| #292 | EN57: ładowanie hamulca przy holowaniu na samym przewodzie, hamulec EP Knorra tylko przy zasilaniu sterowania |
| #293 | Skrzydła drzwi: wnętrze low-poly może mieć własną, ruchomą stronę drzwi |
| #294 | Osobne przełączniki rozruchu silników pojazdów w ukrotnieniu |
| #339 | AI zwalnia hamulec ręczny, gdy pierwszy rozkaz to Connect |
| #359 | Pomoc w jeździe pokazuje hamowanie ED i pomija hamulec lokalny, którego kabina nie obsługuje |
| #377 | Wykolejenie z nadmiernej prędkości według kryteriów fizycznych: przewrócenie, wspinanie obrzeża (`physics.derailment`) |
| #378 | Wyboczenie toru bezstykowego w upale: rysowane, osie po nim jadą, kryteria wykolejenia je widzą |
| #379 | Zderzenia zgniatają końce pojazdów, wjeżdżają na siebie i spychają z toru |
| #387 | Powietrze przewodu głównego wymieniane między pojazdami po kroku wszystkich, w podkrokach |
| #388 | To samo dla przewodu zasilającego |
| #391 | Tempomat oddzielony od bocznikowania |
| #394 | Hamulec dynamiczny pojazdów spalinowo-elektrycznych, włączany w `.fiz` |
| #397 | Własny rodzaj animacji skrzydła drzwi w `.mmd` |
| #398 | Flagi rozkładu dla AI: drzwi środkowe, stopień, kierownik, próba hamulca |
| #401 | Uniwersalne przyrządy kabiny przeciągane myszą do dowolnej pozycji, włączane w `.mmd` |
| #402 | Widoki pociągu: orbita, manewrowy, sprzęgania, z boku toru |

**AI i ruch (pwarszawski, dos1, maj00r)**

| PR | Autor | Co i jak działa |
|---|---|---|
| #231 | dos1 | Pomoc w jeździe: przełączanie maszynisty AI |
| #265 | pwarszawski | AI na wzniesieniu trzyma pociąg przy zatrzymaniu i rusza na hamulcu zespolonym |
| #301 | pwarszawski | Ekrany Pythona: Pillow dołączany do buildu Windows, moduł ekranów dla Pillow 10+ |
| #304 | maj00r | `PointVelocity`: ograniczenie prędkości czoła pociągu w punkcie za komórką |
| #305 | maj00r | Zmniejszona prędkość sygnału kończy się na oznaczonym końcu okręgu nastawczego (`SignalAreaEnd`) |
| #321 | maj00r | Punkt zatrzymania na W4 trzyma zapas jak przy przystanku osobowym od końca peronu |

**Edytor, tory, czarna skrzynka (maj00r)**

| PR | Co i jak działa |
|---|---|
| #289 | Stos 3–5c w masterze: iglice rozjazdów, podkłady 3D, rdzeń edytora, stacje SRK, samouczek |
| #298 | Klipy lekcji samouczka edytora |
| #299 | Podpowiedzi i okienka zostają w oknie głównym, długa podpowiedź się zawija |
| #322 | Czarna skrzynka sesji: przejazd, kabina, sygnały i stacje do pliku |
| #334 | Ortofoto pokazywane na jeden z trzech sposobów; teren z NMT według jednej zasady |
| #342 | Build Linuksa z libcurl, żeby edytor pobierał mapy |

**Renderer w masterze (Hirek193)**: #303 skala instancji modeli w starym i prostym rendererze; #308 tekstury dalekiego pojazdu
nie wracają czarne; #319 `AGENTS.md` jako jedne zasady dla agentów; #320 Linux ładuje biblioteki z folderu gry; #329 CI: limit
czasu i ponowienia `apt`.

**Nasze, już wcielone (miloszunio)**: #302 obrót modelu gizmem nie przeskakuje za 90°; #309 kursor rysowany w obrazie (streamy,
nagrania); #310 punkty dróg łapane na ekranie, także pod terenem, Alt+ciągnij zmienia wysokość; #313 kółko wciśnięte i ciągnięte
przesuwa widok w perspektywie; #314 punkty narzędzi terenu na promieniu przez kursor; #327 `Mouse_Hit()` nie mówi „tak”, gdy nie wie.

**Jakość i testy (jerrrrycho)**: #323 cztery błędy bramki Sonara na masterze; #324 CI anuluje buildy zastąpionego commita; #325,
#333 pokrycie biblioteki testami (parser, narzędzia, PLC); #326, #337 scenariusze testowe (sprzęg automatyczny, raporty misji);
#358 skrypty Lua scenerii w piaskownicy jak ekrany Lua; #365 `localtime` wielowątkowo bezpieczne, bez `goto`.

## 2. Renderery OpenGL 4.6 i Vulkan, PR #216 (Hirek193, szkic do mastera)

`feat/gl46-migration` zastępuje renderer OpenGL 3.3 rendererem OpenGL 4.6 i dodaje Vulkan 1.4. Na tej gałęzi weszły też:

- #223 złączki rozjazdów (maj00r);
- #237 nierówności toru (maj00r);
- #246 iglice rozjazdów (maj00r);
- #240 głębia kursora edytora w gl46 (miloszunio);
- gałąź `jano211/siec-radio`: radio FM w kabinie (`fmradio.*`, IwanXXI) i edytor sieci trakcyjnej: przęsła według typów
  sieci, wysięgniki, bramki, kotwienia (`catenary.*`).

Od Hirka jest też:

- **fizyka na wielu wątkach** (`performance.multithreading`): grupy sprzęgniętych pojazdów liczone równolegle;
- **usypianie fizyki** pojazdów stojących na hamulcach (`physics.sleepheld`);
- **raporty awarii** z kontrolą procesora na starcie i `eu07_starter` (`crashdump.full`).

Ustawienia renderera są w `eu07.ini` (klucze `gfx.*`). Co w nim jest:

- objętościowe chmury z cieniami, objętościowa mgła, gwiazdy;
- SSR i odbicia ray tracing w Vulkanie, DLSS i generowanie klatek (`gfx.antialiasing dlss`, `gfx.framegeneration`);
- AO, auto-ekspozycja, balans bieli, oświetlenie kabiny światłem z okien i lampami modelu;
- trawa 3D, cząsteczkowy deszcz i śnieg, dym liczony na GPU.

Opis: `docs/opengl46-renderer.md`.

## 3. Otwarte PR-y wcielone tutaj (jeszcze nie w staging)

| PR | Autor | Do | Co i jak działa |
|---|---|---|---|
| #239 | jerrrrycho | staging | Długie funkcje Movera podzielone na mniejsze (stan z 8.11); poprawki staging są wpisane w nowe funkcje |
| #241 | miloszunio | gl46 | Wypełnianie obszaru: Enter kończy obszar, Esc porzuca obrys, lista wypełnionych obszarów do zmiany |
| #243 | miloszunio | switch-blades | Położenie NMT z kamery; `//$g` szukane też w dołączanych plikach |
| #311 | miloszunio | master | Profil podłużny drogi w osobnym oknie (P), robiony na węzłach |
| #312 | miloszunio | master | ~ pokazuje i chowa ortofoto, Ctrl+~ najpierw pobiera to, co widzi kamera; prędkość kamery w menu Widok |
| #315 | miloszunio | gl46 | Pola w trybie Teren, falowanie terenu pod nimi, naturalne skarpy |
| #375 | miloszunio | master | Pędzel modeli: jeden model albo wiele w okręgu (losowo, równomiernie, rzednąco ku brzegowi), obrót i skala losowe |
| #376 | miloszunio | master | Nazwy torów spoza ASCII wypisane (Narzędzia → „Track names outside of ASCII...”), nie zmieniane; edytor nadaje tylko nazwy ASCII |

**Bez PR, gotowe do wystawienia:**

- **Poprawka ortofoto (miloszunio).** Gałąź `fix/orthophoto-chunks-in-memory` do mastera. Ortofoto kładzie się tylko na chunkach
  terenu, które są już w pamięci, i dopasowuje się do ziemi, gdy chunk dojdzie. Po starcie edytora nie „pływa” z kamerą.
- **Najnowsza praca maj00ra.** Łańcuch `split/*` → `feat/signal-descriptions` → `feat/sbl-chain`: sygnały SBL znane po łańcuchu,
  opisy sygnałów, `sleepers3d` i `frogs` w `eu07.ini`. Tu jest w wersji z zakładkami sygnalizacji z mastera i z łańcuchami SBL.

## 4. Tylko w naszych gałęziach, bez PR (miloszunio)

### Pogoda i pory roku (`feat/weather-seasons`)

Pogoda scenerii (sekcja `weather` albo stara `atmo`) to warstwy chmur, mgła i opad. Opis: `docs/weather.md`.

- **Śnieg leżący na scenerii, zawiewany przez wiatr w zaspy.** Śnieg leży na terenie, torach i modelach, a na liściach drzew z
  płaszczyzn bielą się górne krawędzie kęp. Na torach jest tak, jak widzi go pociąg: zaspy na szynach znikają tam, gdzie
  przejechał. Włączany przez `gfx.snow`.
- **Fizyka śniegu na torze** (`physics.snow`): zalegający śnieg hamuje pociąg, a pociąg go odgarnia.
- **Klimat.** Normy klimatyczne stacji IMGW są wbudowane. Pogoda brana jest z klimatu dla daty jazdy (`scenario.date`) i miejsca
  scenerii albo z ustawień: `weather.mode` (`auto` zostawia pogodę startera tam, gdzie miejsce nie jest znane) i
  `weather.manual`. Do tego wiatr przy ziemi, wilgotność i temperatura dnia.
- **Zima bez zimowych tekstur** (`seasons.wintertextures`): śnieg rysowany na zwykłych teksturach. Szare światło pod opisanym
  zachmurzeniem. Bez poświaty w zamieci i ulewie.
- **Okno „Czas i otoczenie”**: data, czas jako gg:mm i cała pogoda.
- **Ekran ładowania czeka na kompilację shaderów** gl46. Przy pierwszym starcie nowej wersji nie ma czarnego okna „nie odpowiada”.
  Szkic PR jest w `pr-reviews/PR-gl46-shaders-before-first-frame.md`.

### Edytor i teren (`ficzery2`)

Obsługa wszystkiego: `docs/poradnik.md`. Lista zmian wydanie po wydaniu: `docs/changelog.md`.

- **GeoTools (F7)**: Pola, Drzewa, Torowisko i skarpy, Tory, Idź do. Obrys z działki ewidencyjnej (ULDK GUGiK), edycja punktów
  obrysu, różdżka na ortofoto, „UTWÓRZ” nad brakującym chunkiem.
- **ZIELSKO**: sadzenie drzew i krzewów, także przy torze (`zielsko_tor_<n>`), z licznikami, czemu nie sadzi.
- **Uprawy jako rośliny**: archetypy w `terrain.yaml` (zboża z kłosem, owies, rzepak, ściernisko), zasięg `gfx.grass.crops`,
  obsada w Polach.
- **Pędzel w Otoczeniu**: Modele, Trawa 3D (też wzdłuż ścieżki), Tekstura gruntu; Alt+LPM pobiera, Esc cofa pociągnięcie.
- **Obiekty pod torem i mosty**: przepusty, przęsła z filarami, most blachownicowy i kratownicowy z torem na konstrukcji,
  obsypka, jeden model T3D. Mosty z jazdą dołem na grupę torów.
- **Drogi**: droga gruntowa, obrót tekstury (`texangle`), tunel z konstrukcją, rondo z większą liczbą zjazdów, teren pod drogą.
- **Tory z mapy IRT** (GeoTools → Tory): linie z GeoJSON zaprojektowane według norm i ułożone wokół kamery.
- **NMT i teren**: model terenu z map, obszar pobierania (`//$a`), paleta materiałów, falowanie terenu, teren trzyma punkt
  odniesienia scenerii.
- **Bank obiektów**: „Dodaj model...”, miniatury robione w tle, grupy alfabetycznie.
- **Edytor startuje w rzucie pionowym i z wyłączonym ortofoto.** Ortofoto włącza się dopiero, gdy początek układu scenerii jest
  ustalony.

### Gra

- **`eu07_user.ini`**: własne ustawienia czytane po `eu07.ini`; starter ich nie rusza.
- **Obraz „PRO”** (`gfx.postfx.grade.*`, `gfx.postfx.sharpen`): nasycenie, żywość, kontrast, wyostrzenie. Domyślnie bez zmian.
- **Wymiana powietrza w przewodach przy fizyce wielowątkowej**: każda grupa sprzęgniętych pojazdów wymienia je w swoim wątku,
  czego wymaga #387. Naprawiony wykolejony pojazd budzi fizykę i znów rusza po spadku. To poprawki styku staging z #216, zrobione
  przy scalaniu.

## Co warto wiedzieć

- **Tam, gdzie master świadomie cofnął zmiany ze stosu maj00ra, zachowanie jest jak w masterze**, dla zgodności z istniejącymi
  sceneriami:
  - oś na rozjeździe jedzie po rozjeździe tak, jak jest ustawiony;
  - o tym, czy tor jest prosty, decyduje pierwszy wektor kontrolny.
- **Sygnalizacja w edytorze**: zakładki z mastera (Sygnały, Stacja, Sygnały stacji, Blokada liniowa) z łańcuchami SBL.
- **Samouczek edytora** (Pomoc → Samouczek) jest z powrotem, jak w masterze.
- **Wszystkie klucze `eu07.ini` i komendy selftestu edytora** z `ficzery2` i `feat/weather-seasons` są.
- **Do budowania ze źródeł**: nowy submoduł `ref/wamr` (`git submodule update --init ref/wamr`).

---

# Wcześniejsza historia ficzery2

Gałąź `ficzery2`: nowa generacja `ficzery`. Na kodzie Hirka, mastera i jerrrrycho leżą najnowsze zmiany maj00ra, a na tym wszystkie nasze wdrożenia.

## Zmiany w tym wydaniu

Lista zmian wydanie po wydaniu, z zaznaczeniem, co jest w PR-ach do mastera: [changelog.md](changelog.md).

- **Pędzel w Otoczeniu kładzie trzy rzeczy, każdą pod jasną nazwą** („Pędzel kładzie:”): **Modele** (jak dotąd, w tym nowe „wiele wzdłuż linii” z PR #375), **Trawa 3D** (źdźbła z shadera terenu Hirka, paleta traw ta sama co w trybie Teren) i **Tekstura gruntu** (nasze malowanie materiałów palety terenu: Mieszaj/Kryj/Zmazuj). Trawa 3D ma do tego **„Wzdłuż ścieżki”**: LPM klika punkty, Enter sadzi trawę pasem (szerokość w każdą stronę, miękka krawędź, pełność), Shift+Enter ją zabiera, Backspace cofa punkt, Esc porzuca ścieżkę; cała ścieżka to jeden krok Ctrl+Z. W kole: trzymany LPM, Shift zabiera trawę / maluje pierwszy materiał, **Alt+LPM pobiera** rodzaj trawy albo materiał spod kursora, **Esc przy trzymanym przycisku cofa całe pociągnięcie** (także pędzla modeli). Promień i siła wspólne z pędzlami trybu Teren. W trybie Teren narzędzia nazywają się teraz tak samo: „Malowanie” → **„Tekstura gruntu”**, „Trawa” → **„Trawa 3D”**. Pliki scenerii i terenu bez zmian.
- **Edytor startuje z wyłączonym ortofoto:** oprócz rzutu pionowego inicjalizacja odbywa się zawsze bez ortofoto. Ortofoto włącza się samo (jeśli było włączone) dopiero, gdy origin sceny jest ustalony na dobre (przejęty z terenu, `sync_terrain_origin`) i edytor narysował pierwsze klatki w rzucie pionowym. Włączenie w tym czasie (~, menu Mapa, model terenu) czeka i wchodzi po starcie; zapis znacznika `//$o` sceny widzi zamierzony stan, nie chwilowe wyłączenie.
- **Uprawy jako rośliny (etapy E1–E2 z `docs/koncepcje/pola-uprawy.md`):** rodzaj trawy ma archetyp rośliny (`archetype` w `terrain.yaml`: `blade`, `cereal`, `awned`, `panicle`, `rape`, `stubble`, `straw`). Zboża mają kłos (pszenica pionowy, jęczmień zwisający z ośćmi, żyto smukłe z krótkimi ośćmi), owies ma wiechę z wiszących kłosków, rzepak czapę kwiatów albo łuszczyn, ściernisko kikuty ścięte płasko z jaśniejszym końcem. Łan uprawy: AO w głębi łanu, cień słońca w łanie, przeświecanie pod słońce, połysk przy kątach ślizgowych (`grass_plant.glsl`, `grass_canopy.glsl`, wspólne dla GL 4.6 i Vulkana). Zasięg upraw własny (`reach`, domyślnie 220 m, `gfx.grass.crops` w `eu07.ini` go mnoży; 0 = jak trawa), rzednięcie dopiero od ok. 60 m z pełną kompensacją szerokości, do 262 144 roślin na kafel. Presety upraw z nowymi kolorami i realną gęstością; w Polach suwak **„Obsada (źdźbeł/m²)”** (zapis `dens=` w polu powierzchni) i przycisk **„Odśwież uprawy w palecie traw”**. Trawa (archetyp `blade`) i stare palety wyglądają jak dotąd. Tekstury `uprawy/*` do przegenerowania `tools/zielsko/uprawy.py` (nowy `canopy_colour()`), patrz poradnik.
- **Mosty, etap A (nr 173):** `group_tracks` łączy skrzyżowania z torami równoległymi (do 12°, do 14 m, formacja ±1,5 m) w jedno, z `site.axes`; mosty z jazdą dołem budują przęsło na grupę torów (`maker::deck`). `terrain_streamer::cut_cells` wycina całe komórki siatki (próbki na krawędzi 127, w środku 255; maska zagęszczana do 2 próbek na komórkę), `cells_under` podaje komórki z wysokościami, `crossing::ground_outside` kroi ich trójkąty po dziurach (wypukłych) w pas gruntu modelu (`editor_mode::ground_of_structure`, UV jak teren). Dno otworu idzie po przekopie (`site.bench`, `site.bank`, `site.natural`). `under_formation` pomija miejsca, gdzie skarpa nasypu schodzi do rodzimego gruntu ponad drogą. `crossing::preview` buduje scenę do podglądu przy liście (rysowana `ImDrawList`, malarzem).
- **Obiekty pod torem od nowa (nr 172):** `editor/editorCrossing` (bez silnika, test `test_crossing.cpp`) buduje model, dziury i podniesienie terenu dla ośmiu rodzajów. Czoła z próbek terenu, skrzydła rozchylone do miejsca, gdzie teren schodzi do drogi, krawędzie dziur 0,45 m w murze, dno z wielokątów dziur poszerzonych o 0,8 m. Za niski nasyp: odmowa z brakującą wysokością (`refusal::too_low`), droga prawie wzdłuż toru: `along_track`. Chunk przy tworzeniu zszywany z sąsiadami (`terrain_streamer::stitch`), raycast terenu pomija dziury.
- **Mosty z torem na konstrukcji (nr 164):** dwa nowe rodzaje obiektu pod torem: „Most blachownicowy, tor na konstrukcji” i „Most kratownicowy, tor między kratownicami”. Nasyp jest wycinany na całej szerokości między przyczółkami, przyczółek ma ławę pod łożyska i ściankę żwirową do poziomu torowiska, tor leży na pomoście (podmodel `pomost`). Blachownicowy: dźwigary pod pomostem co ok. 2 m, wysokość 1/12 rozpiętości (0,8–2,6 m). Kratownicowy: poprzecznice pod pomostem, dwie kratownice (podmodel `kratownice`): pasy, słupki, krzyżulce opadające ku środkowi; przęsło od 24 m wysokie (min. 7,5 m) ze stężeniami nad torem, krótsze niskie. Wieloprzęsłowe: filary, na nich łożyska (także w przęśle belkowym pod nasypem). `builder::member` w `editor/editorStructure`, test w `test_structure.cpp`.
- **Ekran ładowania przed pierwszą klatką (nr 163):** po wczytaniu scenerii ekran ładowania pokazuje „Przygotowanie grafiki…” przez dwie klatki i dopiero wtedy przełącza na symulację, więc przy kompilacji shaderów nowego exe zostaje napis, a nie czarny obraz.
- **Uwaga przy pierwszym uruchomieniu nowej wersji:** po wczytaniu scenerii do ~minuty czarnego ekranu, bo sterownik kompiluje shadery dla nowego exe od zera. Trzeba poczekać; drugie uruchomienie startuje szybko (opis w poradniku).
- **Droga i teren (okno profilu drogi → Teren):**
  - „Doprowadź teren do drogi” zostawia teren pod nasypem toru w spokoju (wcześniej ciął tam klin, z którym walczyła formacja toru); wykop drogi kończy się prosto u podnóża nasypu, a przejście robi obiekt;
  - nowe pole „Płaski teren poza drogą (m)”, domyślnie 1,5 m: niweleta terenu szersza niż droga;
  - „Usuń trawę spod drogi”: trawa terenu znika pod nawierzchnią i poboczami.
- **Obiekty pod torem, dalsze elementy:** „Przęsła” 1-6 z filarami między nimi (podmodel `filary`; sklepienia osobno nad każdym przęsłem, płyta i dźwigary ciągłe), nowy rodzaj „Rura z blachy falistej” (podmodel `rura`: okrąg przez krawędzie dna i klucz, szerszy niż dno w połowie wysokości; gdy skrajnia jest mniejsza niż połowa szerokości, powstaje sklepienie półkoliste), „Tekstura stali” dla dźwigarów, balustrad i rury (puste = kolor).
- **Obiekty pod torem jako jeden model T3D z elementami:** generator pisze jeden plik `.t3d` w klasycznym formacie tekstowym (podmodele `Parent/Type/Name/…/Map/…/Transform/NumVerts`, trójkąty jednostronne z maską -1 i jawnymi normalnymi, tak jak czyta je `TSubModel::Load`). Podmodele: `obiekt` (przyczółki, skrzydła, dno), `sklepienie` albo `plyta`, `czola` (czoła z gzymsami), `attyka` albo `balustrady`, a w przęśle belkowym `dzwigary` (dwuteowniki pod płytą, co ok. 2,2 m) i `lozyska`. Mur i beton mają wybraną teksturę, stal jest w kolorze bez tekstury (`Map: none`). Nowe pole „Na czołach”: jak dla rodzaju / nic / balustrada / attyka. Kod: `editor/editorStructure`, testy `tests/unit/test_structure.cpp`.
- **Rondo, więcej zjazdów:** zjazd z ronda może leżeć bliżej sąsiedniego zjazdu. Dotąd każdy potrzebował po obu stronach pełnego miejsca na zaokrąglone naroża (ok. 4 m ponad styk krawędzi) i przy kolejnym zjeździe narzędzie mówiło „Too close to a junction”; teraz na rondzie ramię skraca się do styku krawędzi, zostawiając 2 m ronda między zjazdami.
- **Droga gruntowa:** w budowaniu drogi wybór „Rodzaj drogi”: asfaltowa albo gruntowa. Gruntowa: bez pasów, pochylenie 3 razy większe i promienie łuków pionowych 3 razy mniejsze, nierówna (domyślnie 5 cm, fale co ok. 6 m) albo „Równa jak stół”. Nierówność dotyczy tego, co widać; pojazdy jadą po osi. W scenerii: `rough <m> <m>`. Ograniczeń promieni i przejściówek w planie narzędzie dróg nie ma, więc nie było czego luzować.
- **Obrót tekstury drogi:** pole „Obrót tekstury [°]” przy długości tekstury (drogi i skrzyżowania); w scenerii zapisywane jako `texangle`, tylko gdy różne od zera.
- **Obiekty pod torem:** dwa nowe kształty (sklepienie półkoliste, przęsło belkowe na przyczółkach), „Tekstura powtarza się co (m)”, a gdy skrajnia się nie mieści, obiekt powstaje z taką, jaka się mieści (min. 2,2 m), zamiast odmowy; skrzydła nie wychodzą poza podnóże nasypu.
- **Okno „Opcje narzędzia” wraca samo:** wybranie obszaru pracy albo narzędzia GeoTools otwiera je, jeśli było zamknięte, i wysuwa, jeśli jego dok był odpięty. Zamknięcie okna jest odnotowane w `log.txt`.
- **UTWÓRZ nad brakującym chunkiem:** w GeoTools nad każdym brakującym chunkiem w pobliżu kamery (do 3 chunków) wisi żółty przycisk; tworzy chunk z NMT albo z podłoża scenerii.
- **Esc:** w GeoTools czyści obrys, wybrane pole, działki, podgląd drzew, projekt torów i zaznaczenie; w Drogach kończy budowanie albo stawianie i wraca do zaznaczania.
- **GeoTools → Tory (nowe):** linie z mapy IRT (GeoJSON w `terrain/<teren>/tory/`) projektowane według norm narzędziami maj00ra i układane jako tory w chunkach wokół kamery: łuki, przejściówki, przechyłki, lokalne ograniczenia prędkości, dwa tory na liniach dwutorowych, niweleta dopasowana do terenu. Bez stacji, rozjazdów i kształtowania terenu. Opis w poradniku („Tory z mapy”) i w `docs/koncepcje/tory-z-danych.md`.
- **Poprawki z recenzji PR-ów** przeniesione: dziura osobnego pola po wczytaniu sceny, „Obrysuj ponownie” po wyczyszczeniu obrysu, dziury nakładających się pól, „Wyrównaj” na wolnym końcu drogi.
- **GeoTools** zamiast Ficzery (F7): zakładki Pola, Drzewa, Torowisko i skarpy, Idź do; Pola przeniesione tu z Terenu.
- **Drzewa przy torze** (do 30 m od osi, do ustawienia): nazwa `zielsko_tor_<n>`, niebieskie w podglądzie, osobna grupa modeli 3D do sadzenia, przyciski „Wymień drzewa przy torze” i „Zaznacz drzewa przy torze”.
- **ZIELSKO mówi, czemu nie sadzi:** liczniki każdego etapu sita i sadzenia w statusie i w `log.txt`.
- **Kierunek rzędów** przy fali „Pole uprawne”: żółty pierścień na ziemi, łapany w dowolnym miejscu, działa z góry i w perspektywie (Ctrl: co 5°); strzałka pokazuje rzędy.
- **Przeciąganie punktów obrysu** (też z działki) wreszcie działa: przycisk trzymany nie docierał do obrysu (w nr 111 przeciąganie kończyło się w pierwszej klatce).
- **Obrys nad pustym chunkiem:** żółte ostrzeżenie i przycisk „Dodaj brakujące chunki pod obrysem” (NMT albo grunt scenerii); chunk dodany pod istniejącym polem dostaje od razu jego dziurę i malowanie (wcześniej pole dorysowywało się dopiero po odjechaniu kamerą, bez dziury).
- **Punkty obrysu działki** rysowane i chwytane na terenie pod nimi (wcześniej narożniki nad niewczytanym terenem dostawały wysokość 0 i w widoku z ukosa wisiały obok obrysu).

### Z nr 111

- **Obrys z działki ewidencyjnej** (ULDK GUGiK): klik w teren albo numer działki, Shift+klik dokłada sąsiednią (wspólne boki odpadają). `editor/editorParcel.cpp`.
- **Edycja punktów obrysu:** przeciąganie, Alt+klik usuwa punkt albo wstawia go na boku.

## Zmiany w nr 109

Poprawki z review maj00ra do PR-ów #309–#315, przeniesione do nas:
- **Pola / powierzchnie:** suwaki we właściwościach pola zapamiętują wartość (wcześniej przeciągnięcie się nie zapisywało); „Obrysuj ponownie” kładzie punkty na ziemi i ponowne wycięcie zmienia to samo pole zamiast robić drugie; osobne powierzchnie budowane tak daleko, jak rysowany jest teren (bez dziur za 1,5 km); pole na krawędzi ostatniego chunka budowane z tego, co ma ziemię, bez przebudowy co sekundę; „Oddaj terenowi” zamyka tylko dziurę po polu (dziury pól mają znacznik 254), ręczne dziury zostają; ponowne kładzenie formacji nie kasuje naturalnych skarp.
- **Profil drogi:** koniec przebiegu ciągnie za sobą inne odcinki w tym punkcie (zmiana pasów, trzy odcinki bez skrzyżowania); pętla ma jedną wysokość w punkcie zamknięcia; „Wyrównaj” zostawia nachylenia na końcach; wykres odświeża się też przy pełnej historii; „Poziomo” i „Nad terenem” zmieniają tylko zaznaczone węzły.
- **Punkty dróg:** najpierw punkt przy klikniętej ziemi, potem odcinek albo skrzyżowanie pod kursorem, szukanie na ekranie tylko gdy nic nie ma i nie dalej niż to, co pod kursorem; Alt+przeciąganie przerywa Esc, zamknięcie okna, inne narzędzie albo tryb torów; Ctrl+Z czeka na koniec przeciągania.
- **Promień do terenu:** nie trafia pod ziemię sąsiedniego chunka przez lukę; w widoku z góry pędzle działają nad terenem wyższym niż kamera.
- **Ctrl+~:** w widoku z góry bierze cały widok; działa za pierwszym razem przy wyłączonym ortofoto. „Jak było” w prędkości kamery znów bez przyspieszania z przybliżeniem.
- **Kursor do streamów:** wyłączany po wyjściu z edytora, przywracany po powrocie.
- **`Mouse_Hit()`** domyślnie „nie wiem” zamiast „trafiono” (PR #327).

## Zmiany w nr 84–85

- **`eu07_user.ini`** (nr 85): własne ustawienia czytane po `eu07.ini`, z `%APPDATA%\Maszyna` albo z folderu gry. Starter przepisuje `eu07.ini` tylko ze znanymi sobie kluczami (np. `gfx.atmosphere.haze` znikało), tego pliku nie rusza.

- **Obraz „PRO” w ustawieniach** (renderer GL 4.6 i Vulkan, domyślnie bez zmian, włączane w `eu07_user.ini` albo suwakami w oknie ekspozycji, tam gdzie balans bieli):
  - `gfx.postfx.grade.saturation` (1 = bez zmian), `gfx.postfx.grade.vibrance` (podbija nasycenie bladych kolorów: trawy, zboża, ziemi; nieba i żywych kolorów prawie nie rusza), `gfx.postfx.grade.contrast` (krzywa S jasności);
  - `gfx.postfx.sharpen` (0–1): wyostrzenie po TAA, w granicach sąsiednich pikseli, bez obwódek.
  - Tonemapper AgX daje celowo płaski obraz (nasycenie 0,9); te ustawienia go „wywołują”.
- **Uprawy bez jesieni:** pod zielonym zbożem w listopadzie leżała zaorana ziemia, bo `.mat` z `farm01` itd. ma jesienny wariant „zaorane”. Wszystkie uprawy mają teraz własne `.mat` w `textures/uprawy/` (zimą tylko śnieg).
- **Kolor pola za granicą trawy 3D:** tekstura każdej uprawy i łąki ma średni kolor jej trawy 3D, więc za 60–250 m pole nie zmienia barwy (nowe: `zboze_zielone`, `laka_wysoka`, `pastwisko`, `ugor`, `ozimina`).
- **Ziemia cieplejsza:** zaorane i zbronowane mają brązowe, nie sine tekstury (`zaorane`, `zbronowane`).
- **Ścieżki technologiczne:** trawa w koleinach przerzedzona, nie zabrana do gołej ziemi, ziemia namalowana słabiej: z daleka ciemniejszy pas w zbożu zamiast kropek.
- Pola wycięte w nr 83 mają stare materiały: zmień je na liście pól albo wytnij jeszcze raz.

## Skąd jest kod

- **Hirek:** `feat/gl46-migration` (OpenGL 4.6 i Vulkan, mieszanie tekstur torów, sieć trakcyjna, raporty awarii, `eu07_starter`).
  `feat/vhg` (pojazdy z grafów) na razie **nie wchodzi**: Hirek i jerrrrycho po swojemu pocięli te same funkcje fizyki w `Mover.cpp`, wejdzie, gdy Hirek dopasuje je do nowej fizyki.
- **master:** do 5.11 (sprzęgi przegubowe, polskie litery z windows-1250).
- **jerrrrycho:** `refactor/mover-long-functions` z `test/core-unit-coverage` (porządki po SonarCloud, testy).
- **maj00r:** `feat/switch-blades` (wszystkie 33 nowe commity z 3.11) i jego najnowszy łańcuch `split/*` → `feat/signal-descriptions` → `feat/sbl-chain`:
  - zakładka Sygnalizacja: sygnalizacja stacji rozkładana od jej rozjazdu jak na liniach PKP PLK, schemat stacji;
  - semafory i tarcze dobierane po opisie tego, co pokazują, SBL rozpoznawana po łańcuchu semaforów i obwodach torowych;
  - narzędzie kładzenia samo bierze największy promień, rozjazd dobierany po prędkości na torze zwrotnym, przejścia rozjazdowe ciągnięte od rozjazdu;
  - wspólna niweleta: tory dokładane i zdejmowane ręcznie; tekstury dróg, terenu i wody z podglądu; przenoszenie właściwości toru;
  - podkłady 3D (`sleepers3d`) i krzyżownice (`frogs`) włączane w `eu07.ini`.
  Uwaga: maj00r zajął znaczniki `//$w` (niwelety), `//$t` i `//$z`, więc nasze wypełnione obszary są teraz zapisywane jako `//$u`.
- **Nasze:** wszystkie wdrożenia z `ficzery` (lista niżej) i nowe rzeczy z tej generacji.

## Nowe w ficzery2

- **Paleta materiałów terenu:** przy każdej pozycji strzałki ▲▼ (kolejność na liście) i **X** (usunięcie z palety).
  Usuwając, wybierasz, jaki materiał dostanie teren nim pomalowany. Pierwszej pozycji nie da się usunąć, bo pokrywa nowe kafle.
  Kafle nie są przepisywane, więc działa to od razu na całej scenerii.
- **Falowanie terenu przy wycinaniu powierzchni** (Ficzery > Wytnij powierzchnię > „Falowanie terenu”).
  Rodzaje: łagodne pagórki, bruzdy na polu, garby i kretowiska, długie fale, niecka albo kopiec.
  Ustawiasz wysokość, długość fali, kierunek (przycisk „jak tekstura”), zróżnicowanie i narastanie od krawędzi. Ctrl+Z cofa razem z powierzchnią.
  Nowe wzory: kratka garbów i pasy pól (każdy pas na swojej wysokości, z miedzami).
  **Profil fali**: zaokrąglony, ostre grzbiety, ostre doliny, tarasy (z liczbą stopni) i płaskie wierzchy, z suwakiem ostrości, żeby pola nie były obłe.
  **Chropowatość**: grudy i kępy ok. 3 m, do 20 cm, na całej powierzchni.
  Pod wycinaną powierzchnią siatka terenu zagęszcza się sama (do 1 m przy chropowatości, do 1/3 długości fali), bo przy punkcie co 5 m drobne fale i grudy znikały.
- **Teren bez prześwietlenia**: shader terenu bierze siłę odblasków (`param_specular`) z materiału warstwy, tak jak model z tym samym materiałem.
  Trawa (`grass`, specular 0.01) jest teraz matowa jak na nasypach, a nie lśniąca w słońcu.
- **Bez mgły w rzucie z góry:** widok z góry pokazuje prawdziwe kolory terenu, bo wcześniej kamera patrzyła przez ponad kilometr powietrza z mgłą.
  W Widok > „Prędkość kamery i przesuwanie” jest też przełącznik „Bez mgły w edytorze” dla perspektywy.
- **Tory z IRT jako warstwy `.scm`:** konwerter zapisuje `irt_tory.scm`, który dołącza `irt_tory_rozjazdy.scm` i osobny plik na każdą linię.
  Plik `.inc` silnik traktował jak szablon, więc tory z niego nie dawały się chwycić, przesunąć ani usunąć.
  Teraz zachowują się jak położone w edytorze. Stary plik przerabia `tools/irt/tory_na_warstwy.py`.

## Dział Ficzery (F7)

Jest na górnej belce obok Otoczenia, Torów, Dróg i Terenu. Ma cztery narzędzia.

**Wytnij powierzchnię.**
- Obrys rysuje się jak w Wypełnieniu: LPM dodaje punkt, Shift+LPM cofa ostatni.
- Duży przycisk „Wytnij jako osobną powierzchnię” maluje obszar materiałem z palety prosto w teren. Nie ma dziury ani osobnej siatki, więc nie ma szwu. Wysokości zostają terenu.
- **Trawa 3D pod spodem:** zostaje, jest usuwana (domyślnie) albo w jej miejsce sadzony jest inny rodzaj z palety trawy. Samo malowanie zmienia tylko teksturę, a źdźbła rosłyby dalej. „Oddaj terenowi” sadzi z powrotem pierwszy rodzaj z palety.
- Ustawia się:
  - **szerokość przejścia** w materiał dookoła;
  - **szum krawędzi**, czyli na ile krawędź faluje w obie strony, żeby nie była od linijki;
  - **fale szumu**: małe dają krawędź poszarpaną, duże łagodnie falistą.
- Przejście jest co najmniej 1 m. Po zagęszczeniu chunku do 0,5 m może być węższe, bo malowanie terenu jest zapisane w punktach siatki.

**Kształt obrysu.** Działa przy wycinaniu powierzchni i przy Wypełnieniu w Terenie. Każda opcja ma własny przełącznik i można je łączyć:
- **Zaokrąglone narożniki:** każdy narożnik dostaje łuk o zadanym promieniu. Przy gładkiej krzywej zaokrąglane są tylko ostre punkty.
- **Gładka krzywa przez punkty:** boki są krzywymi przez kliknięte punkty. Ctrl+klik stawia ostry narożnik, a suwak „napięcie” przechodzi od okrągłych krzywych do prawie prostych boków.
- **Uchwyty Béziera:** wciskasz przycisk tam, gdzie ma być punkt, i przeciągasz. Wychodzą uchwyty krzywej, widoczne na niebiesko.
- **Rysowanie z ręki:** trzymasz LPM i rysujesz po terenie albo ortofotomapie. Po puszczeniu linia jest upraszczana, z ustawianą dokładnością, i wygładzana.
- **Boki wzdłuż toru:** punkty siadają w odległości od osi najbliższego toru, kliknięcie albo zadanej. Bok między takimi punktami idzie równolegle do toru, także po łukach.

Powierzchnia pamięta kliknięte punkty i opcje, więc „Obrysuj ponownie” przywraca obrys dokładnie tak, jak był rysowany. Krzywe zamieniają się na obrys co 0,5 m.

**Torowisko i skarpy.**
- Wzdłuż modyfikatorów torowiska (tych, którymi tory kształtują teren) wycina kawałki nie dłuższe niż 100 m:
  - **torowisko:** pas do krawędzi formacji (3,52 m od osi). Przy kilku torach granica wypada w połowie odstępu między nimi.
  - **nasypy i przekopy:** od krawędzi torowiska do miejsca, gdzie skarpa 1:1,5 spotyka teren. Osobno lewa i prawa strona.
- Kawałki są malowane w teren, tak samo jak wycinane powierzchnie.
- Trawę 3D ustawia się osobno dla torowiska (domyślnie usuwana) i dla skarp (domyślnie zostaje).
- Przejścia ustawia się osobno: torowisko–skarpa (domyślnie 0,5 m) i skarpa–teren (1,5 m). Do tego szum krawędzi.
- Nazwy wyglądają tak: `L132 t1 torowisko km 012.300-012.400`, `L132 t1 nasyp L km …`.
- Zakres to tory wokół kamery (promień do ustawienia) albo cała sceneria.
- Ponowne uruchomienie zastępuje kawałki wycięte wcześniej w tym samym miejscu.
- Nasypy dalej usypują modyfikatory, a kawałki służą tylko do teksturowania.

**Naturalne skarpy** (w narzędziu Torowisko i skarpy).
- Nasypy i przekopy przestają być idealnym trapezem.
- Torowisko i jego krawędzie zostają dokładnie według ST-T1-A6. Zmienia się tylko to, co dalej:
  - **falowanie podnóża:** linia, gdzie skarpa spotyka teren, wchodzi i wychodzi o zadane metry, w falach o zadanej długości;
  - **łagodniejsze o najwyżej:** skarpa 1:1,5 miejscami łagodnieje, np. do 1:1,8, ale nigdy nie jest bardziej stroma niż w projekcie;
  - **miękkie podnóże:** zaokrąglenie podnóża rośnie z wysokością nasypu, więc wysokie nasypy łagodnie wchodzą w teren;
  - **nierówności skarpy:** powierzchnia skarpy jest lekko pofałdowana, jak ziemia ułożona przez lata.
- „Zrób naturalne skarpy” działa wokół kamery albo w całej scenerii. Ctrl+Z cofa, a „Z powrotem skarpy według projektu” przywraca trapez.
- Ustawienia zapisują się przy modyfikatorach w `terrain.yaml` (`natural: {...}`).
- Do łagodzenia skarp edytor potrzebuje terenu sprzed nasypów, czyli wycinka `HBAS`. Konwerter robi go opcją `--terrain-base`, a skarpy naturalne od razu opcją `--natural-slopes`. Ten sam szum jest w silniku i w konwerterze co do bitu, więc teren z konwertera i z edytora wychodzi taki sam.

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

## Punkt odniesienia trzyma teren

- Teren zapisuje w `terrain.yaml`, gdzie leży: `origin: {system: PUWG1992, east, north}`.
- Gdy edytor wczytuje teren z zapisanym położeniem, bierze z niego punkt odniesienia dla ortofotomapy, NMT, mapy i Idź do. Nie ma znaczenia, co było wcześniej wpisane albo kliknięte. Na pasku stanu pokazuje, że to zrobił.
- Teren bez zapisanego położenia w scenerii z linią `//$g` dostaje je przy pierwszym wczytaniu.
- Teren robiony narzędziem NMT dostaje położenie od razu. Robi to też konwerter IRT.
- W narzędziu „Prawdziwy teren z map (NMT)”, gdy teren zna swoje położenie, przyciski „Sceneria jest tutaj” i „Przypisz z kamery” są zablokowane. Odblokowuje je dopiero „Przestaw mimo to”, i wtedy teren zapamiętuje nowe miejsce. Przycisk „Środek zapisany w terenie” wraca do niego.

## Zamykanie edytora

- Okno znika od razu po wyjściu. Pobierania ortofotomapy i NMT, które jeszcze czekają na serwer, nie trzymają już zamrożonego okna.
- Najpóźniej po 6 s proces kończy się sam.
- W menu Plik jest „Zamknij edytor”, który zamyka bez dodatkowego okna.
- Okno „Wyjść?” z menu Ogólne pokazuje się na środku, nad panelami.

## Pozostałe

- **Punkt odniesienia scenerii.** Edytor czyta linię `//$g` także z pliku dołączonego przez `include`. W narzędziu „Prawdziwy teren z map (NMT)” w dziale Teren, w sekcji Położenie, są dwa nowe przyciski:
  - **„Przypisz z kamery”:** punkt kliknięty na mapie to miejsce, gdzie stoi kamera, a z tego wylicza się środek scenerii. Cofa przestawienie przyciskiem „Sceneria jest tutaj”. Wystarczy podlecieć kamerą nad znaną stację, kliknąć ją na mapie i przypisać.
  - **„Środek zapisany w pliku scenerii”:** bierze środek z linii `//$g`.
- **Ułożony tor da się poprawić i usunąć od razu.** W zakładce kładzenia torów edytor nie zaznacza torów, więc wcześniej nie dało się ich ani chwycić, ani usunąć: Delete kasował tylko ostatni 2-metrowy kawałek. Teraz:
  - panel kładzenia pokazuje „Ułożony ostatnio: … m, … odcinków” z przyciskami **„Popraw go”** (przejście do zakładki torów z całym ułożonym torem zaznaczonym) i **„Usuń go (Del)”**,
  - **Delete** w zakładce kładzenia usuwa cały ostatnio ułożony tor, a nad innym torem ten tor,
  - **Ctrl+klik** na torze przenosi go do zakładki torów do poprawy.
- **Prędkość kamery (Widok > Prędkość kamery i przesuwanie).**
  - Suwak prędkości.
  - Suwak „Przyspieszenie z Ctrl”: było na sztywno 30×, przez co Ctrl+Shift gnał ok. 1,5 km/s.
  - Opcja „W widoku z góry prędkość zależna od powiększenia”, domyślnie włączona.
  - Przesuwanie wciśniętą rolką: w widoku z góry, opcjonalnie także w perspektywie.
- **Gotowe warstwy WMS.** W oknie warstw, nad „Dodaj warstwę WMS”, jest lista do zaznaczania:
  - linie i stacje PKP PLK,
  - kolej, linie energetyczne ze słupami, budynki, lasy, drogi i wody z BDOT10k,
  - uzbrojenie terenu (KIUT),
  - budynki, granice działek i użytki z EGiB,
  - lasy z BDL,
  - ulice i adresy,
  - plany miejscowe,
  - zabytki.

  Po zaznaczeniu edytor czyta usługę i dobiera jej warstwy. Gdy usługa nie odpowiada albo nie rysuje w EPSG:2180, przy pozycji pojawia się komunikat.
- **Dokumentacja.** Jak powstaje sceneria IRT jest w `docs/irt-konwersja.md`, normy PLK w `docs/plk-normy.md`, literatura w `docs/literatura-kolejowa.md`.
- **Narzędzia.** Konwerter jest w `tools/irt/`, audyt PLK w `tools/plk/`.

## Numer wersji

Każdy build ma kolejny numer, np. `ficzery nr 18`. Numer jest widoczny:
- w tytule okna gry i edytora,
- w logu,
- w pliku `WERSJA.txt` w paczce,
- w tytule wydania na GitHubie.

Numer rośnie o jeden z każdym buildem. Exe zbudowane w domu ma numer z dopiskiem `-dom`.

- **Z filmu z kabiny** (Ficzery, na dole panelu): wczytuje metadane z `kabina_meta.py` (`filmy\wroclaw.meta`) i trasę filmu po torach scenerii (`filmy\wroclaw.trasa`).
  Suchą trawę, podszyt pod krzakami i żwir pobocza maluje w teren jako powierzchnie „kabina …” (z przejściem i szumem krawędzi, Ctrl+Z cofa),
  a drzewa i krzaki stawia z grup banku obiektów („Duze drzewka”, „Krzaki”) z losowym obrotem i proporcjami. Torowiska nie rusza.
  Przycisk „Usuń to, co rozmieszczono z filmu” zdejmuje wszystko naraz; ponowne rozmieszczenie najpierw usuwa poprzednie.
- **Magiczna różdżka na ortofoto** (opcje obrysu: wycinanie powierzchni i wypełnianie terenu): LPM na ortofoto zaznacza obszar o kolorze
  zbliżonym do klikniętego (tolerancja, zasięg, dokładność, prostowanie, wypełnianie dziur) i robi z niego cały obrys.

- **Tab** pokazuje i chowa ortofoto (tam, gdzie Tab nie ma innej roli), **Ctrl+Tab** pobiera w pierwszej kolejności kafle ortofoto widoczne w kamerze i od razu ponawia te, które się nie udały.
- **Drogi: profil pionowy i teren** (zaznaczona droga, sekcja „Profil pionowy i teren”): wyrównanie wysokości całej drogi z największym pochyleniem
  i promieniami łuków wypukłych i wklęsłych (końce zostają na miejscu), oraz dociągnięcie terenu do drogi modyfikatorem jak przy torowisku:
  płasko pod jezdnią i poboczami, skarpy 1:n z naturalną nierównością; dla zaznaczonej drogi albo wszystkich dróg scenerii, z możliwością puszczenia terenu.
- **Drogi: tunel** („Wytnij tunel” w sekcji „Profil pionowy i teren”): tam, gdzie zaznaczona droga wchodzi w nasyp albo wzgórze (teren o ustawioną wysokość nad nawierzchnią),
  w terenie wycinana jest dziura na szerokość drogi z poboczami; nasyp i tor zostają nad drogą, a teren dociągnięty do drogi kończy się na portalach. Sekcja jest zawsze widoczna.
- **Profil pionowy drogi w osobnym oknie** (klawisz P przy zaznaczonej drodze albo przycisk w panelu drogi): wykres drogi i terenu wzdłuż całej drogi,
  wyrównanie profilu, dociągnięcie terenu, tunele.
- **Tunel z konstrukcją**: przy wycinaniu tunelu powstaje model: ściany wzdłuż drogi, płyta (pod nasypem) albo sklepienie (pod wzgórzem), czoło nad otworem i skrzydła przy portalach, z betonu.
- **Zakres „Wokół kamery / Cała sceneria”** jest teraz przy każdym przycisku, który działa na obszar (wycinanie torowiska, naturalne skarpy, przemalowanie w teren).
- **Skrzyżowania**: asfalt dróg i skrzyżowań mapowany z góry w jednej skali, bez szwów na styku wlotów.
- **Widok > Kursor rysowany w obrazie (stream)**: wskaźnik widoczny w streamie i nagraniu okna.
- **Nowa paleta tekstur**: kafelki z podglądem w grupach (zgadywanych z nazw, przeciąganie na nazwę grupy przenosi), przesuwanie przeciąganiem,
  PPM na kafelku: skala, obrót, zmiana materiału wszędzie naraz (bez nowego wpisu), grupa, zdjęcie z palety.
  Pędzel: **Mieszaj / Kryj / Zmazuj**; pod paletą: co leży pod kursorem (w %) i ile z 8 materiałów ma kawałek (na czerwono, gdy brak miejsca);
  **Alt+LPM** pipeta, **1–9** pierwsze dziewięć kafelków.
- **Zamień w obrysie** (narzędzie Wypełnij): materiał A na zaznaczony tylko w obrysie (np. z różdżki), reszta scenerii bez zmian.
- **Wyczyść malowanie całego terenu** (pod paletą): każdy kawałek z więcej niż jednym materiałem dostaje sam pierwszy; najpierw kopia `terrain\<nazwa>_przed_czyszczeniem`.


## Usunięty Kapelusz

Narzędzie Kapelusz (prostokąt nad torem i dobór rozjazdu) wypadło z działu Ficzery jako zbędne.

## Build 47: profil drogi na węzłach, łapanie punktów pod terenem, klepsydra

- **Pasy w niebo**: wyrównanie profilu traktowało wektory kontrolne łuków jak punkty i wpisywało w nie wysokość bezwzględną. Teraz wysokości i pochylenia trafiają do wektorów poprawnie. Drogę popsutą starym wyrównaniem naprawia „Zaznacz wszystkie” → „Po terenie” (albo Ctrl+Z).
- **Punkty pod terenem**: punkty drogi łapane po odległości na ekranie (12 px), więc także te schowane pod terenem. **Alt+LPM** na punkcie i ciągnięcie w pionie zmienia wysokość zaznaczonych punktów (Ctrl: dokładniej), ustawia się po puszczeniu.
- **Okno profilu (P)**: węzły drogi jako kropki; LPM zaznacza, Shift dodaje, ramka zaznacza odcinek, przeciąganie węzła zmienia wysokość. Zaznaczenie widać też w widoku. Na zaznaczonych: Poziomo, Stałe pochylenie, Łuk pionowy (promień), Po terenie (+ przesunięcie), W górę/w dół o, Wygładź. Klasy dróg (polna … autostrada) ustawiają pochylenie i promienie. Przewyższenie wykresu 1:1 … 1:50 z siatką w metrach.
- **Obrót modeli wokół osi pionowej**: brany z przyrostu w tej klatce, a nie z rozkładu macierzy na kąty (który za 90° przeskakiwał na 180° − kąt), więc model nie skacze w lewo i w prawo.
- **Klepsydra**: ciężkie funkcje (torowisko i skarpy, naturalne skarpy, przemalowanie, wycinanie powierzchni, zamiana w obrysie, różdżka, film, teren do drogi, tunele) najpierw zamrażają ekran z klepsydrą i nazwą zadania. Postęp w procentach na pasku tytułu, Esc przerywa tam, gdzie się da, okno nie łapie „Nie odpowiada”.
- **Żółte**: przyciski wprowadzające zmiany i podpowiedzi w widoku.

## Build 48: obiekty pod torami zamiast tunelu z progu

- **Usunięty „Wytnij tunel”** z progiem wysokości. Robił kilka kawałków tunelu na łuku drogi, dziury bez dna i konstrukcję z odwróconymi osiami (model miał x i wysokość w osiach scenerii, a modele MaSzyny mają x odwrotnie i wysokość jako trzecią oś; stąd trójkąty w powietrzu).
  Przy stawianiu nowych obiektów stare modele „tunel” tej drogi znikają, a dziury wzdłuż niej są zamykane.
- **Obiekt pod torem** (okno profilu drogi): łuk sklepiony, wiadukt ramowy albo przepust skrzynkowy, na każdym skrzyżowaniu drogi z nasypem toru (modyfikatorem terenu).
  Czoła równoległe do toru (skos jak w rzeczywistości), skrzydła wzdłuż skarp do miejsca, gdzie teren schodzi do drogi, gzyms na czołach, podłoga przy drodze.
  Teren wycinany tylko przed czołami, między skrzydłami; nad obiektem nasyp i tor zostają. Brzegi dziur chowają się w ścianach.
  Skrajnia, szerokość w świetle, jedna tekstura z listy (wyszukiwanej w `textures/`). Modele i obrysy dziur w `models/kabina_obiekty/`.
- **Poradnik** (`docs/poradnik.md`) dołączany do opisu każdej wersji.
- Poprawka obrotu modeli wysłana do maj00ra jako gałąź `fix/gizmo-yaw-rotation` (na bazie `feat/switch-blades`).

## ZIELSKO z kosmosu, poprawka shaderów, Sonar (po nr 69)

- **Nowe narzędzie w Ficzerach: „ZIELSKO z kosmosu”.** Drzewa z modeli 3D drzew GUGiK (CityGML LoD1, z lotniczego skanowania laserowego, drzewa powyżej 4 m, licencja CC BY 4.0) sadzone wzdłuż torów.
  - Powiaty pod scenerią znajduje usługa WMS `ModeleDrzew3D` Geoportalu (zapytanie o punkt co 2,5 km pasa wzdłuż torów).
  - Paczki powiatów (`opendata.geoportal.gov.pl/InneDane/Drzewa3D/LOD1/2023/<woj>/<TERYT>.zip`, 1,5–5,4 GB) są czytane strumieniowo w osobnym wątku: zip po nagłówkach lokalnych, każdy arkusz GML rozpakowany w pamięci (inflate z `stb_image`), z każdego drzewa brany środek podstawy pnia i wysokość. Na dysk trafia tylko mały plik powiatu z drzewami nad terenem w pasie wzdłuż torów (`terrain/<teren>/zielsko/<TERYT>.drz`, 10 bajtów na drzewo).
  - W danych współrzędne są w EPSG:2180 w kolejności wschód, północ; sprawdzone zapytaniem do WMS (arkusz Wałbrzycha trafia w powiat 0265).
  - Sito: zasięg (kamera, cała sceneria, obrys), pas od osi toru, odstęp od dróg, woda terenu, najniższe drzewo, przerzedzenie, limit (najpierw najbliżej torów).
  - Modele z grup banku węzłów, skala według wysokości drzewa, losowy obrót i lekki rozrzut, nazwy `zielsko_<n>`, karczowanie jednym przyciskiem.
- `editor_http::client::get_stream`: pobieranie bez trzymania całości w pamięci (WinHTTP i curl).
- **Shadery ogl46:** cofnięta zmiana z mastera (alpha to coverage), która przy scalaniu trafiła do shaderów GL 4.6 bez swoich funkcji i zawieszała ładowanie scenerii.
- **Sonar:** „Idź do” nie sięga za listę miejscowości; wysokość wczytywanego chunka bez pętli.

## Pola w trybie Teren (nr 80)

- Wycinanie powierzchni przeniesione z zakładki Ficzery do **Teren → Pola**, w to samo miejsce co w PR [MaSzyna-EU07 #315](https://github.com/MaSzyna-EU07/maszyna/pull/315) do `feat/gl46-migration`.
  U nas zostają nasze obrysy (łuki, Bézier, z ręki, wzdłuż toru, różdżka), których w PR-ze nie ma.
- Nowy wariant **osobnej powierzchni** (dziura i własna siatka, poza limitem 8 materiałów chunka), jak w PR-ze.
- Lista pól i naturalne skarpy dostępne w panelu Pól; w Ficzerach zostały Torowisko i skarpy, ZIELSKO i Idź do.
- **ZIELSKO przebudowane:** jeden zasięg (kamera, obrys, cała sceneria) dla czytania i sadzenia. „Posadź” sam doczytuje brakujące arkusze mapy (nazwy arkuszy liczone z PUWG 1992 przez współrzędne geograficzne, siatka mapy 1:1 000 000), czyta paczkę powiatu tylko do ostatniego potrzebnego arkusza i zapisuje wszystkie drzewa każdego minionego arkusza (format `.drz` w wersji 2: lista arkuszy, znacznik „cały powiat”; wersja 1 czytana jako pas). Odpowiedzi WMS o powiatach zapamiętane w `zielsko/counties.txt`. Sadzenie i karczowanie dotyczą tylko zasięgu, numery `zielsko_<n>` idą dalej zamiast od zera.

## Uprawy na polach (nr 83)

- **Teren → Pola → „Obsiane”**: 15 upraw (zaorane, zbronowane, ozimina, zboże zielone i w kłoszeniu, dojrzała pszenica, jęczmień, żyto, owies, rzepak kwitnący i dojrzały, ściernisko, łąka wysoka, łąka niska, ugór). Każda daje materiał, rodzaj trawy 3D i ustawienia rzeźby.
- **Nowa fala „Pole uprawne”**: kierunek z najdłuższego boku, rzędy wędrujące na boki, zagony z grzbietem, uwrocia z rzędami dookoła i poszarpanym końcem rzędów, ścieżki technologiczne (koleiny w rzeźbie, ziemia w malowaniu, bez trawy).
- Ścieżki zapisane w powierzchni (nowa kolumna `tram=… angle=… head=… drift=… soil=…` w pliku powierzchni), więc przemalowanie i Ctrl+Z je odtwarzają.
- `tools/zielsko/uprawy.py`: tekstury upraw z `farm00`/`farm01` (przekolorowanie, plamy dojrzewania, kwiaty rzepaku, słoma ścierniska), kafelkowane, z `.mat` jak `farm00.mat`.
- `terrain_streamer::paint_where` / `grass_where`: malowanie i trawa według funkcji miejsca.

## Uprawy: kolory, ścieżki, obraz (nr 84–85)

Opis w „Zmiany w tym wydaniu” na górze.

## Historia wersji

Od pierwszego buildu `ficzery2`. Numery to kolejne buildy CI (wspólna numeracja z innymi gałęziami, dlatego są dziury). Wcześniejsza gałąź `ficzery` (pierwsza generacja) jest opisana w części „Dział Ficzery”.

- **nr 29–31** (5.11): pierwsze składanie `ficzery2` z kodu Hirka, mastera, jerrrrycho i maj00ra; buildy nieudane (błędy po scaleniu).
- **nr 32** (5.11): pierwszy działający. Paleta terenu z kolejnością i usuwaniem pozycji, falowanie terenu pod wycinaną powierzchnią, widok z góry bez mgły, tory IRT jako warstwy `.scm`; poprawki po scaleniu: podkłady 3D w GL 4.6 i Vulkanie, parametry materiałów.
- **nr 33** (6.11): profile fal (ostre grzbiety, doliny, tarasy, płaskie wierzchy), kratka garbów, pasy pól, chropowatość; teren tak jasny jak model z tym samym materiałem.
- **nr 35** (6.11): siatka terenu zagęszczana pod falami i grudami (do 1 m).
- **nr 38** (7.11): rozmieszczanie z filmu z kabiny (powierzchnie, drzewa, krzaki), magiczna różdżka na ortofoto.
- **nr 39–40** (7.11): drzewa z filmu na ziemi i odsunięte od toru, ciemna trawa pod krzakami.
- **nr 41** (8.11): Tab/Ctrl+Tab dla ortofoto; profil pionowy drogi i teren dociągany do drogi.
- **nr 42–43** (8.11): tunel tam, gdzie droga wchodzi w nasyp, okno profilu drogi z tunelami jako konstrukcją, zakres przy każdym przycisku, asfalt skrzyżowań bez szwów, kursor w streamie.
- **nr 44–45** (8.11): nowa paleta tekstur (kafelki w grupach, pędzel Mieszaj/Kryj/Zmazuj, pipeta), zamiana materiału w obrysie, czyszczenie malowania z kopią.
- **nr 46** (8.11): usunięty Kapelusz; „Przemaluj w teren” zawsze widoczne.
- **nr 47** (8.11): profil drogi na węzłach, punkty dróg łapane pod terenem, obrót bez skoków, klepsydra przy ciężkiej pracy (Esc przerywa), przyciski zmieniające scenerię na żółto.
- **nr 49** (8.11): obiekty pod torami (przepust łukowy, wiadukt ramowy, przepust skrzynkowy) zamiast tunelu z progu; poradnik dołączany do każdego wydania.
- **nr 51** (9.11): wciągnięte nowe zmiany mastera (tryb testów, AI, kabiny, alpha to coverage).
- **nr 52** (9.11): nasze narzędzia wydzielone do własnych plików (łatwiejsze scalanie z Hirkiem).
- **nr 54** (9.11): SonarCloud dla forka.
- **nr 60** (9.11): ortofoto pod ~ i Ctrl+~ (Tab wrócił do kładzenia torów, jak w masterze).
- **nr 69** (9.11): shadery materiałów GL 4.6 z własnym alpha testem (poprawka zawieszania).
- **nr 70** (10.11): poprawki z Sonara.
- **nr 72** (10.11): ZIELSKO z kosmosu: drzewa z modeli 3D drzew Geoportalu sadzone wzdłuż torów.
- **nr 80** (10.11): wycinanie powierzchni przeniesione do Teren → Pola (jak w PR #315 do Hirka), osobna powierzchnia.
- **nr 82** (10.11): ZIELSKO przebudowane: jeden zasięg czytania i sadzenia, arkusze doczytywane do ostatniego potrzebnego.
- **nr 83** (10.11): uprawy na polach (15 upraw z trawą 3D i teksturami), fala „Pole uprawne” (rzędy, zagony, uwrocia, ścieżki technologiczne).
- **nr 84** (11.11): korekcja obrazu i wyostrzanie, uprawy bez jesieni, kolor pola zgodny z trawą, cieplejsza ziemia, miękkie ścieżki technologiczne.
- **nr 85** (11.11): `eu07_user.ini`, ustawienia, których starter nie kasuje.
- **nr 110** (11.11): mapa drogowa (`docs/roadmap.md`).
- **nr 111** (11.11): obrys z działki ewidencyjnej, edycja punktów obrysu, strzałka kierunku rzędów.
- **nr 109** (11.11, numeracja przeskoczyła przez buildy gałęzi PR-ów w forku): poprawki z review PR-ów #309–#315 (pola, profil drogi, punkty dróg, promień do terenu, Ctrl+~, kursor), `Mouse_Hit()` bez optymizmu.
