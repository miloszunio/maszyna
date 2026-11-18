Gałąź `integ/staging-weather` to **build-staging** (gałąź `staging` z 18.11, commit `dd733861`, wydanie
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
