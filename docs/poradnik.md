# Poradnik: jak korzystać z dodatków ficzery2

Krótko i po kolei: co kliknąć, żeby zrobić typowe rzeczy w scenerii. Opis zmian w każdej wersji jest wyżej, tu jest sposób pracy.
Wszystko, co zmienia scenerię, ma **żółty** przycisk, a podpowiedzi w widoku są żółte. Prawie wszystko cofa **Ctrl+Z**.

## Na start

- Numer wersji jest w tytule okna i w `WERSJA.txt`. Zgłaszając błąd, podawaj go.
- **~** (tylda, klawisz na lewo od 1) włącza i wyłącza ortofotomapę na terenie, **Ctrl+~** najpierw ściąga kafelki tam, gdzie patrzy kamera.
- **GeoTools** (dawniej Ficzery) to dział **F7** na górnej belce, z zakładkami **Pola**, **Drzewa**, **Torowisko i skarpy**, **Idź do** (te same narzędzia są na pasku). Drogi to **F4**, Tory **F3**.
- Ciężkie operacje (torowisko i skarpy na całej scenerii, przemalowanie, film, dociąganie terenu, obiekty pod torami) pokazują **klepsydrę** na zamrożonym ekranie.
- **Nowy chunk** (UTWÓRZ, z NMT, z gruntu scenerii, rosnący przy torach) **zszywa się sam** z istniejącymi sąsiadami: jego krawędzie dostają ich wysokości, różnica wygasa w głąb na 40 m. Bez przycisku. Szczelina, która już jest, znika po Ctrl+Z i ponownym UTWÓRZ.
  Postęp w procentach jest w tytule okna, **Esc** przerywa tam, gdzie się da. To, co zrobione, zostaje i da się cofnąć.


## Pierwsze uruchomienie nowej wersji: czarny ekran do ~minuty

- Od nr 163 ekran ładowania zostaje na ten czas z napisem „Przygotowanie grafiki (przy pierwszym uruchomieniu nowej wersji trwa dłużej)” zamiast czarnego obrazu. Reszta poniżej bez zmian: trzeba poczekać.
- Po wczytaniu scenerii pierwsza klatka przygotowuje shadery wszystkich materiałów. Przy **pierwszym uruchomieniu nowego exe** sterownik karty kompiluje je od zera (u nas ok. 35 s, w `log.txt` widać to jako „slow frame before, … ms” zaraz po „scenery lamp shadow maps”), a ekran ładowania już wtedy zniknął: obraz jest czarny i wygląda, jakby gra się zawiesiła. **Trzeba poczekać**, nie zamykać.
- Dzieje się tak, bo sterownik trzyma skompilowane shadery osobno dla każdego programu, a każdy build ma inną nazwę exe (`eu07-x64_<data>_<hash>.exe`). Drugie uruchomienie tego samego exe startuje już szybko.

## Teksturowanie terenu

1. GeoTools (F7) → paleta: kafelki materiałów w grupach. LPM wybiera materiał, **1–9** pierwsze dziewięć, **Alt+LPM** w widoku bierze materiał spod kursora.
2. Pędzel: **Mieszaj** (przenika), **Kryj** (zastępuje), **Zmazuj**. Pod paletą widać, co jest pod kursorem i ile materiałów ma kawałek (max 8; czerwone = brak miejsca).
3. PPM na kafelku: skala, obrót, zmiana materiału w całej scenerii naraz (bez dokładania nowej pozycji), grupa, zdjęcie z palety z zamiennikiem.
4. **Zamień w obrysie** (Teren → Wypełnij): narysuj obrys (albo magiczną różdżką), wybierz materiał „z” i kliknij „na zaznaczony”. Reszta scenerii zostaje.
5. **Wyczyść malowanie całego terenu**: każdy kawałek wraca do pierwszego materiału. Przed tym robi się kopia `terrain\<nazwa>_przed_czyszczeniem` (Ctrl+Z tego nie cofa).

## Pola: wycinanie powierzchni (pola, łąki, place)

Od wydania po nr 111 Pola są w **GeoTools (F7) → Pola** (zakładka i ikona trawy na pasku). Wcześniej (nr 80–111) były w Teren → Pola; z Terenu zniknęły.

1. GeoTools → **Pola**. Rysuj obrys: LPM punkt, Shift+LPM cofa. Kształt obrysu: zaokrąglenia, gładka krzywa, Bézier, z ręki, wzdłuż toru (tego u Hirka jeszcze nie ma).
2. Albo **magiczna różdżka**: włącz i kliknij na ortofoto w pole, obrys zrobi się sam z obszaru o podobnym kolorze.
3. Materiał wybierasz w palecie trybu Teren (nad panelem). Do wyboru:
   - **Wmalowane w teren** (domyślnie): bez szwu, przejście i szum krawędzi, trawa 3D pod spodem; zajmuje jeden z 8 materiałów chunka.
   - **Osobna powierzchnia**: dziura w gruncie i własna siatka po wysokościach terenu; poza limitem 8 materiałów, ale krawędź ostra i może być widać szew.
4. Jeśli ma falować: „Falowanie terenu” (pagórki, bruzdy, pasy pól, kratka…), profil fali, chropowatość.
5. Żółty „Wytnij pole”.
6. Lista „Pola i wycięte powierzchnie”: zmiana materiału, „Idź do”, „Obrysuj ponownie”. Powierzchnie z dziurą (osobne albo stare z IRT) zamienia na malowane „Przemaluj je w teren”.
7. „Naturalne skarpy” są w tym samym panelu (i dalej w GeoTools → Torowisko i skarpy).

### Obrys z działki, edycja punktów, strzałka rzędów (od nr 111)

- W opcjach obrysu (Wypełnij, Pola): **„Działka z ewidencji (kliknij ją)”**. Klik w teren pobiera obrys działki w tym miejscu z ewidencji (ULDK GUGiK), **Shift+klik** dokłada sąsiednią (wspólne boki znikają) albo zabiera już wziętą. Można też wpisać pełny numer (np. `021903_2.0003.176/5`) i **Pobierz po numerze**. Teren musi mieć ustawione położenie w PUWG 1992 (to samo co dla ZIELSKO).
- **Punkty każdego obrysu da się poprawiać:** przeciągnij punkt, **Alt+klik na punkcie** go usuwa, **Alt+klik na boku** wstawia nowy.
- Przy fali **„Pole uprawne”** w środku obrysu jest **żółty pierścień obrotu** leżący na ziemi (zawsze ok. 90 px na ekranie, z góry i w perspektywie), a strzałka w nim pokazuje kierunek rzędów. Złap pierścień w dowolnym miejscu i obróć (Ctrl: co 5°), a wyłączy się „Rzędy wzdłuż najdłuższego boku”; tekstura uprawy idzie za rzędami.
- Jeśli obrys (np. działka) wchodzi na miejsce **bez terenu** (chunka nie ma ani w pamięci, ani na dysku), pod obrysem pojawia się żółte ostrzeżenie z liczbą takich chunków i żółty przycisk **„Dodaj brakujące chunki pod obrysem”**: z NMT, gdy teren ma georeferencję, inaczej z gruntu scenerii. Pole wycięte wcześniej kończy się na krawędzi terenu, a gdy chunk zostanie dodany (tym przyciskiem, narzędziem Chunki albo z NMT), pole od razu na niego wchodzi: dziura pod osobną powierzchnią i malowanie malowanej. Falowanie gruntu pola nie jest na nim powtarzane.

### Uprawy (od nr 83)

W panelu Pól jest **„Obsiane”**: wybierz uprawę, a pole dostanie jej teksturę, trawę 3D (dopisaną do palety traw przy pierwszym użyciu) i kształt ziemi:

- **Zaorane**, **Zbronowane i obsiane**, **Ozimina młoda**, **Zboże zielone**, **Zboże kłoszenie**, **Pszenica / Jęczmień / Żyto / Owies dojrzałe**, **Rzepak kwitnący / dojrzały**, **Ściernisko**, **Łąka wysoka**, **Łąka niska, pastwisko**, **Ugór**. Kukurydzy nie ma.
- Tekstury dojrzałych zbóż, kłoszenia, rzepaku i ścierniska są w `textures/uprawy/` (zrobione z `farm00`/`farm01` Maszyny skryptem `tools/zielsko/uprawy.py`, nic z sieci). Zima jak w `farm00.mat` (śnieg na polu), jesień zaorana.
- Fala **„Pole uprawne”**: rzędy wzdłuż najdłuższego boku obrysu (albo kierunek ręcznie), lekko falujące („Rzędy falują”, 0,5–1 m na ~100 m), **zagony** z grzbietem i bruzdami rozorywkowymi, **uwrocie** wzdłuż brzegu z rzędami dookoła i poszarpaną wewnętrzną krawędzią, **ścieżki technologiczne** (dwie koleiny co 18/24/36 m i dookoła środkiem uwrocia: ziemia namalowana, trawa zabrana, płytkie zagłębienie).
- Teren ma punkt co metr (malowanie też), więc pojedyncze rzędy zbóż robi tekstura, a rzeźba robi to, co widać z pociągu: zagony, uwrocia, koleiny.
- Tekstura sama kładzie się wzdłuż rzędów; „Tekstura obrócona o” to już tylko poprawka.
- Trawa 3D zbóż jest gęsta i wysoka. Zasięg trawy: `gfx.grass.distance` (pełna gęstość, domyślnie 60 m) i `gfx.grass.fadedistance` (do tej odległości się przerzedza) w `eu07.ini`. Gdyby gubiło klatki, zmniejsz `gfx.grass.density` albo gęstość trawy uprawy w palecie traw.
- Od nr 84 tekstury upraw mają kolor swojej trawy (pole nie zmienia barwy tam, gdzie kończy się trawa) i nie zamieniają się jesienią w zaorane pole. Pola z nr 83: zmień materiał na liście albo wytnij je jeszcze raz.

### Ładniejszy obraz (od nr 84)

W pliku **`eu07_user.ini`** obok `eu07.ini` (`%APPDATA%\Maszyna`), od nr 85. Starter przepisuje `eu07.ini` tylko ze znanymi sobie kluczami i gubi resztę, a `eu07_user.ini` zostawia w spokoju; gra czyta go po `eu07.ini`. Można też suwakami w oknie ekspozycji, obok balansu bieli:

```
gfx.atmosphere.haze 0.4          // mniej zamglenia w pogodny dzień (domyślnie 1)
gfx.postfx.grade.saturation 1.1  // nasycenie (1 bez zmian)
gfx.postfx.grade.vibrance 0.35   // żywsze blade kolory: trawa, zboże, ziemia
gfx.postfx.grade.contrast 1.15   // kontrast (1 bez zmian)
gfx.postfx.sharpen 0.4           // wyostrzenie po TAA (0 wyłączone)
```
Najwięcej daje niższe słońce (rano, popołudnie) przy pogodnym niebie: rzędy i zagony rzucają cień.

## Torowisko i skarpy

1. GeoTools → **Torowisko i skarpy**. Wybierz materiały torowiska i skarpy w palecie, długość kawałka, zasięg skarpy, przejścia.
2. Zakres przy przycisku: **Wokół kamery** (promień) albo **Cała sceneria**.
3. Żółty „Wytnij torowisko i skarpy”. Kawałki robione wcześniej wzdłuż tych samych modyfikatorów są zastępowane.
4. **Naturalne skarpy**: ustaw falowanie podnóża, łagodnienie, chropowatość i kliknij „Zrób naturalne skarpy”. „Z powrotem skarpy według projektu” przywraca trapez. Po tym wytnij torowisko i skarpy jeszcze raz.

## Drogi: wysokości i profil

1. Drogi (F4) → Zaznaczanie. LPM na odcinek albo na **punkt** (biała kropka). Punkty łapią się też **pod terenem**.
2. **Alt+LPM** na punkcie i ciągnij w górę lub w dół: zmienia wysokość zaznaczonych punktów (Ctrl dokładniej), ustawia się po puszczeniu.
   W inspektorze jest też pole „Wysokość [m]”, „Obniż” / „Podnieś” o krok i „Połóż na terenie”.
3. **Profil** na pasku Dróg albo klawisz **P**: okno z profilem drogi (szara) i terenu (brązowy).
   - Przewyższenie wykresu: 1:10 to typowe dla profili dróg; „dopasuj do okna” robi z każdej zmiany górę.
   - Węzły to białe kropki: LPM zaznacza, Shift dodaje, przeciągnięcie po wykresie zaznacza odcinek, przeciągnięcie węzła w pionie zmienia jego wysokość.
   - Na zaznaczonych: **Poziomo**, **Stałe pochylenie**, **Łuk pionowy** (promień), **Po terenie** (+ przesunięcie, minus to wykop), **W górę/w dół o**, **Wygładź**. Żółty „Zastosuj do zaznaczonych węzłów”.
   - **Klasa drogi** (polna … autostrada) ustawia największe pochylenie i promienie łuków; „Wyrównaj całą drogę” stosuje je do całej drogi.
4. Droga z pasami „w niebie” (po wyrównaniu ze starej wersji): Profil → „Zaznacz wszystkie” → „Po terenie” → Zastosuj, albo Ctrl+Z.

## Budowanie dróg: rodzaj, tekstura, rondo

1. Drogi (F4) → budowanie. **Rodzaj drogi**: asfaltowa (jak dotąd) albo **gruntowa**: bez malowanych pasów, pochylenie 3 razy większe, łuki pionowe 3 razy ciaśniejsze, nawierzchnia nierówna („Nierówna o [cm]”, domyślnie 5) albo „Równa jak stół”. Nierówność dotyczy tego, co widać; pojazdy jadą po osi.
2. **Obrót tekstury [°]** obok miniatury tekstury: obraca mapowanie nawierzchni drogi i skrzyżowania.
3. **Rondo**: zjazdy mogą leżeć blisko siebie (między nimi zostaje 2 m ronda), więc mieści się ich więcej niż dawniej.
4. **Esc** kończy budowanie albo stawianie i wraca do zaznaczania.

## Teren pod drogą

1. W oknie profilu: „Pochylenie skarpy 1:n”, potem żółty **„Dociągnij teren do drogi”**. Pod jezdnią robi się płasko, skarpy schodzą lub wchodzą do terenu.
2. „Puść teren” zdejmuje dociąganie (z tej drogi albo, z ptaszkiem, ze wszystkich).
3. Po zmianie profilu drogi dociągnij teren jeszcze raz.

## Droga pod torem: przepust, wiadukt, most

1. Ustaw drogę na **właściwej rzędnej** pod nasypem (okno profilu: Poziomo / Stałe pochylenie / W górę-w dół o). Droga ma przechodzić w nasypie, pod jego powierzchnią.
2. Okno profilu → **Obiekt pod torem** → **Rodzaj** (od nr 172, osiem wyraźnie różnych, według typowych obiektów polskich linii):
   - **Rura z blachy falistej**: okrągły przekrój, końce ścięte po skarpie, betonowy wieniec na skarpie wokół wlotu. Bez skrzydeł.
   - **Przepust skrzynkowy (żelbet)**: prostokąt ze skosami w górnych narożach, niska ścianka czołowa nad stropem, skrzydła ukośne (30°) wzdłuż skarp.
   - **Sklepienie murowane (kamień, cegła)**: łuk odcinkowy albo półkolisty, archiwolta wystająca z muru czołowego, gzyms, murowana attyka, skrzydła lekko rozchylone.
   - **Wiadukt ramowy (żelbet)**: ściany i strop w jednym, duże skosy w narożach, skrzydła w linii ścian, balustrada, gdy tor biegnie tuż nad nim.
   - **Płyta z belek stalowych obetonowanych na przyczółkach**: cienka płyta z wysuniętą krawędzią, ciemne przekładki łożysk nad przyczółkami, oczepy pod płytą, skrzydła ukośne (20°).
   - **Blachownica z jazdą górą**: dźwigary z żebrami i stężeniami pod korytem balastowym, łożyska na ciosach na ławie, ścianka zaplecza do torowiska, balustrada.
   - **Blachownica z jazdą dołem**: dwa wysokie dźwigary obok toru, koryto na poprzecznicach między nimi; zabiera pod torowiskiem tylko ok. 0,55 m, więc mieści się nad niską drogą.
   - **Kratownica z jazdą dołem**: kratownice po bokach; przęsło od 32 m wysokie ze stężeniami nad torem i portalami, krótsze niskie z ramami „U” (zastrzałami do poprzecznic).
   Pod listą: **ile dany rodzaj zabiera pod torowiskiem** ponad skrajnię. Mosty z jazdą dołem zabierają najmniej.
   „Szukaj tekstur” zbiera z `textures/` te z betonem, cegłą, kamieniem, murem w nazwie. Jedna tekstura pokrywa mur i beton; „Tekstura powtarza się co (m)” ją zagęszcza albo rozciąga. „Tekstura stali” dotyczy dźwigarów, kratownic, koryta, balustrad i rury; puste pole daje ciemny kolor.
   „Na czołach”: attyka z muru, stalowa balustrada albo nic; domyślnie według rodzaju.
   „Przęsła”: więcej niż jedno stawia filary (w mostach z łożyskami na filarach); ustaw wtedy „szerokość w świetle” na całość.
   **Podgląd** (od nr 173): obok listy rysuje się wybrany rodzaj z bieżącymi ustawieniami, zbudowany tym samym generatorem na nasypie 7 m, z terenem zszytym jak w edytorze. Przeciągnij myszą, żeby obrócić.
3. Żółty **„Postaw obiekty pod torami”**. Na każdym skrzyżowaniu drogi z nasypem toru (modyfikatorem terenu):
   - **Kilka torów obok siebie** (osobny modyfikator na każdy tor) to od nr 173 jedno skrzyżowanie: obiekt obejmuje wszystkie tory. Mosty z jazdą dołem mają dźwigary albo kratownice **na zewnątrz** skrajnych torów (2,2 m od ich osi), nigdy między torami; tory dalej od siebie niż 8 m dostają osobne przęsła. Mosty z jazdą górą mają jedno koryto i balustrady tylko na krawędziach.
   - **Droga w przekopie**: przed budową obiektu przekop drogi jest prowadzony aż do stopy nasypu na rodzimym gruncie (wcześniej kończył się dużo wcześniej i przed licem zostawał wał ziemi). Dno otworu między skrzydłami biegnie jak przekop: płasko przy drodze, skarpami w górę do skrzydeł.
   - **Obiekty pod nasypem** (rura, przepust, sklepienie, rama, płyta): czoło stoi tam, gdzie **rzeczywisty teren** nasypu schodzi do góry obiektu, skrzydła idą wzdłuż skarp. Teren nad obiektem jest podniesiony tak, żeby go przykrył.
   - **Mosty**: nasyp wycięty między przyczółkami od końca do końca skrzydeł; skrzydło schodzi łagodnie od torowiska do skarpy.
   - **Zszycie z terenem** (od nr 173): teren jest wycinany **całymi komórkami siatki** (2 m). Część tych komórek poza obiektem model obiektu wypełnia **pasem gruntu**: to te same trójkąty co teren, na tych samych wysokościach, przycięte do muru, z materiałem, który przeważa w chunku. Nie ma piły ani szpar na krawędzi. Każda krawędź dziury leży w ścianie, skrzydle albo ściance czołowej.
   - **Płyta z belek obetonowanych** od spodu pokazuje rdzawe stopki belek co ok. 45 cm, leży na osobnym przyczółku z oczepem i przekładkami łożysk, a na czole ma stalową balustradę. **Rama** ma skosy w narożach, skrzydła w linii ścian i betonową attykę.
4. Jeśli nasyp jest za niski: z ptaszkiem „Tak wysoki, jak się zmieści…” obiekt dostaje skrajnię, jaka się mieści (min. **2,5 m**), i status to mówi. Gdy nawet to się nie mieści, obiekt nie powstaje, a **żółty napis pod przyciskami** mówi dokładnie: ile metrów nasypu jest nad drogą, ile zabiera ten rodzaj, **o ile obniżyć drogę** i że most z jazdą dołem zabiera najmniej.
5. „Usuń je” zdejmuje obiekty tej drogi i zamyka dziury (teren podniesiony nad nimi zostaje; Ctrl+Z cofa całość). Modele są w `models/kabina_obiekty/`.
6. Klikanie w dno otworu (np. żeby zacząć nową drogę) łapie punkt na dnie, które widać, a nie na wyciętym terenie pod nim.

## Z filmu z kabiny

1. `kabina_meta.py` na filmie: odczytuje prędkość i kilometry, rozpoznaje otoczenie toru, zapisuje `.meta`.
2. Ficzery → film: wczytaj `.meta` i `.trasa`, ustaw materiały klas (sucha trawa, zarośla, tłuczeń), odsunięcie od osi (4 m), drzewa i krzaki z banku.
3. Żółty „Rozmieść wzdłuż trasy”. Powierzchnie cofa Ctrl+Z, drzewa „Usuń to, co rozmieszczono z filmu”.

## ZIELSKO z kosmosu (drzewa z Geoportalu)

GeoTools → **Drzewa** (ZIELSKO z kosmosu). Drzewa z modeli 3D drzew GUGiK: każde stoi tam, gdzie w rzeczywistości, i ma swoją wysokość.
Dane nie mówią nic o gatunku i koronie (korona jest szablonem skalowanym do wysokości), więc modele bierzesz z banku węzłów.

1. **Zasięg** (jeden dla wszystkiego): wokół kamery (promień), w obrysie (LPM stawia punkty) albo cała sceneria.
2. **Sito**: najbliżej i najdalej od osi toru, odstęp od krawędzi dróg, najniższe drzewo, przerzedzenie (zostaje wyższe), limit (najpierw najbliżej torów). Woda terenu zostaje pusta.
3. **Modele**: grupa banku węzłów na drzewa (domyślnie „Duze drzewka”) i na niskie (domyślnie „Krzaki”, poniżej 8 m); wysokość modeli w skali 1 („z modelu” albo wpisana ręcznie).
4. Żółte **„Posadź”**: edytor sprawdza (raz, odpowiedzi zapamiętuje), które powiaty leżą pod zasięgiem i których arkuszy mapy jeszcze nie ma,
   i w tle czyta paczki powiatów tylko do miejsca, gdzie te arkusze się pojawią. Serwer daje paczkę tylko od początku, więc ile trzeba przeczytać, zależy od tego, gdzie w paczce leżą potrzebne arkusze.
   Każdy arkusz, który po drodze przejdzie, zostaje zapisany ze wszystkimi drzewami w `terrain/<teren>/zielsko/<TERYT>.drz` i nigdy więcej nie jest pobierany.
   Gdy dane dojdą, znikają drzewa ZIELSKA posadzone wcześniej **w tym zasięgu** i sadzone są nowe (`zielsko_<n>`). Drzewa w innych miejscach zostają.
5. „Policz i pokaż” liczy z tego, co już jest na dysku, i rysuje kropki. „Wykarczuj posadzone w zasięgu” zdejmuje drzewa z zasięgu.
6. **Dane**: postęp, przerwanie (to, co doszło, zostaje), lista zapisanych powiatów i „Wczytaj całą scenerię w tle” (na noc, dziesiątki GB, bez sadzenia).
   Pliki z pierwszej wersji (tylko pas przy torach) dalej działają; brakujące arkusze zostaną doczytane.
7. W opisie scenerii podaj źródło: **GUGiK, modele 3D drzew (geoportal.gov.pl), CC BY 4.0**.

### Drzewa przy torze (od wydania po nr 111)

- Drzewa bliżej osi toru niż **„Do (m od osi)”** (domyślnie 30 m) dostają nazwę `zielsko_tor_<n>` (pozostałe `zielsko_<n>`), a w podglądzie („Policz i pokaż”) są **niebieskie**. Po sadzeniu status mówi, ile ich jest.
- **„Grupa dla nich”**: grupa banku węzłów z drzewami 3D. Gdy jest podana, drzewa przy torze od razu sadzą się z niej, zamiast z atlasów.
- **„Wymień drzewa przy torze”** (z grupy w „Wstaw z”): drzewa `zielsko_tor_` w zasięgu wstawia od nowa z tej grupy, w tym samym miejscu, z tym samym obrotem i wysokością z danych. Bez historii (Ctrl+Z tego nie cofa); atlasy wracają po ponownym „Posadź”.
- **„Zaznacz drzewa przy torze”**: wrzuca je do zaznaczenia w Otoczeniu, żeby przesunąć, obrócić albo usunąć razem.
- Status sadzenia i `log.txt` (linie `ZIELSKO:`) mówią, ile drzew odpadło na każdym etapie sita i przy sadzeniu.

## Tory z mapy (GeoTools → Tory, od wydania po nr 145)

Układa tory linii kolejowych z mapy IRT (Geoportal Dolnego Śląska) w chunkach wokół kamery.

1. **Dane.** Plik `IRT_koleje_dolnoslaskie.geojson` (PUWG 1992) leży w folderze `tory/` obok terenu scenerii, np. `terrain/irt/tory/`. Teren musi mieć ustawione położenie (PUWG). Po dołożeniu pliku: „Wczytaj mapę ponownie”.
2. **Obszar.** Stań kamerą tam, gdzie mają być tory, i ustaw „Chunki wokół tego pod kamerą”. Obszar jest narysowany żółtą ramką na ziemi, osie linii z mapy na niebiesko, z nazwą odcinka i prędkością.
3. **„Zaprojektuj tory w obszarze”** (żółty). Każdy odcinek linii przechodzący przez obszar jest projektowany w całości, do kilku sekund na odcinek. Podgląd: zielone tory, pomarańczowe tam, gdzie łuk wymusza prędkość niższą niż na linii, czerwone tam, gdzie projekt leży ponad 10 m od punktów mapy (obejrzyj na ortofotomapie, zanim ułożysz). W tabeli: długość, liczba łuków, ile z nich „wolniej”, odchyłka od mapy (średnia / największa), ile kawałków czeka na ułożenie.
4. **„Ułóż tory w obszarze”** (żółty). Układa tylko kawałki leżące w obszarze; już ułożonych nie rusza. Ctrl+Z cofa całość.

Co dostajesz:
- łuki, krzywe przejściowe i przechyłki według norm z ustawień układania torów (te same, których używa narzędzie do układania);
- łuk za ciasny na prędkość linii dostaje własną, niższą prędkość (w dziesiątkach km/h) i przechyłkę dla niej;
- linia dwutorowa (NT ≥ 2): dwa tory po połowie odstępu z każdej strony osi z mapy;
- wysokości: niweleta dopasowana do terenu, krótkie zagłębienia (przepust, droga pod torem) są pomijane, a nie odwzorowywane;
- tory nazywają się `l<linia>_s<odcinek>_t<tor>_<kawałek>`, np. `l275_s07_t1_00042`; po tych nazwach edytor poznaje, co już leży.

Sąsiednie obszary stykają się zawsze, bo podział linii na odcinki i projekt każdego odcinka zależą tylko od mapy, a nie od obszaru. Warunek: nie zmieniaj między obszarami pól „Kawałki toru do”, „Punkt mapy jest załomem od”, „Prędkość tam, gdzie mapa jej nie podaje” ani norm, bo wtedy projekt wyjdzie inny i tory się rozejdą.

Czego jeszcze nie ma: torów stacyjnych i rozjazdów (mapa ma tylko osie linii), kształtowania terenu pod torami (nasyp, przekop: na razie przyciskami modelu terenu po ułożeniu), mostów i wiaduktów, sieci trakcyjnej. Mapa IRT jest poglądowa: punkty co ok. 50 m, miejscami co 2 km, więc tor może leżeć kilka metrów od prawdziwego.

## Drobne rzeczy w edytorze

- **~** pokazuje albo chowa ortofoto, **Ctrl+~** pobiera najpierw to, co widzi kamera. Prędkość kamery: menu Widok.
- **Kółko myszy wciśnięte i ciągnięte** przesuwa widok także w perspektywie; teren pod kursorem jedzie z nim.
- **Wypełnienie obszaru:** Enter kończy obszar, Esc porzuca obrys; wypełnione obszary są na liście i dają się zmienić później.
- **Paleta materiałów terenu:** ▲▼ zmienia kolejność, X usuwa materiał (wybierasz, czym go zastąpić).
- **Ustawienia użytkownika:** `eu07_user.ini` jest czytany po `eu07.ini`, którego starter nadpisuje; własne klucze trzymaj tam.

## Idź do

GeoTools → **Idź do**: mapa z kamerą, wyszukiwarka miejscowości (polskie znaki obojętne), zapisane kamery w `<sceneria>.kamery`.

## Gdy coś nie działa

- **Zniknęło okno „Opcje narzędzia” (boks narzędzia):** kliknij dowolne narzędzie GeoTools albo obszar pracy, okno wróci. Ręcznie: menu Okna → Opcje narzędzia. Okno zamyka tylko jego X albo kliknięcie środkowym przyciskiem w jego zakładkę.
- **Brak terenu pod kamerą:** w GeoTools nad pustym chunkiem wisi żółty przycisk UTWÓRZ.
- **Esc** w GeoTools czyści obrys i zaznaczenia, w Drogach wraca do zaznaczania.

- Sprawdź numer wersji w tytule okna.
- Długa operacja bez klepsydry albo z „Nie odpowiada”: zapisz, co klikałeś, i `log.txt`.
- Zrzut ekranu z widocznymi podpowiedziami na dole bardzo pomaga.
