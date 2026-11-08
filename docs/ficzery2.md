Gałąź `ficzery2`: nowa generacja `ficzery`. Na kodzie Hirka, mastera i jerrrrycho leżą najnowsze zmiany maj00ra, a na tym wszystkie nasze wdrożenia.

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
