Gałąź `ficzery2`: nowa generacja `ficzery`. Na kodzie Hirka, mastera i jerrrrycho leżą najnowsze zmiany maj00ra, a na tym wszystkie nasze wdrożenia.

## Zmiany w tym wydaniu

- **Uwaga przy pierwszym uruchomieniu nowej wersji:** po wczytaniu scenerii do ~minuty czarnego ekranu, bo sterownik kompiluje shadery dla nowego exe od zera. Trzeba poczekać; drugie uruchomienie startuje szybko (opis w poradniku).
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
