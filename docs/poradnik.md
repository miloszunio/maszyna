# Poradnik: jak korzystać z dodatków ficzery2

Krótko i po kolei: co kliknąć, żeby zrobić typowe rzeczy w scenerii. Opis zmian w każdej wersji jest wyżej, tu jest sposób pracy.
Wszystko, co zmienia scenerię, ma **żółty** przycisk, a podpowiedzi w widoku są żółte. Prawie wszystko cofa **Ctrl+Z**.

## Na start

- Numer wersji jest w tytule okna i w `WERSJA.txt`. Zgłaszając błąd, podawaj go.
- **~** (tylda, klawisz na lewo od 1) włącza i wyłącza ortofotomapę na terenie, **Ctrl+~** najpierw ściąga kafelki tam, gdzie patrzy kamera.
- **GeoTools** (dawniej Ficzery) to dział **F7** na górnej belce, z zakładkami **Pola**, **Drzewa**, **Torowisko i skarpy**, **Idź do** (te same narzędzia są na pasku). Drogi to **F4**, Tory **F3**.
- Ciężkie operacje (torowisko i skarpy na całej scenerii, przemalowanie, film, dociąganie terenu, obiekty pod torami) pokazują **klepsydrę** na zamrożonym ekranie.
  Postęp w procentach jest w tytule okna, **Esc** przerywa tam, gdzie się da. To, co zrobione, zostaje i da się cofnąć.


## Pierwsze uruchomienie nowej wersji: czarny ekran do ~minuty

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

## Teren pod drogą

1. W oknie profilu: „Pochylenie skarpy 1:n”, potem żółty **„Dociągnij teren do drogi”**. Pod jezdnią robi się płasko, skarpy schodzą lub wchodzą do terenu.
2. „Puść teren” zdejmuje dociąganie (z tej drogi albo, z ptaszkiem, ze wszystkich).
3. Po zmianie profilu drogi dociągnij teren jeszcze raz.

## Droga pod torem: łuk, wiadukt, przepust

1. Ustaw drogę na **właściwej rzędnej** pod nasypem (okno profilu: Poziomo / Stałe pochylenie / W górę-w dół o). Droga ma przechodzić w nasypie, pod jego powierzchnią.
2. Okno profilu → **Obiekt pod torem**: rodzaj (**łuk sklepiony**, **wiadukt ramowy**, **przepust skrzynkowy**), skrajnia nad drogą, szerokość w świetle (0 = jak droga), tekstura.
   „Szukaj tekstur” zbiera z `textures/` te z betonem, cegłą, kamieniem, murem w nazwie; jedna tekstura pokrywa cały obiekt.
3. Żółty **„Postaw obiekty pod torami”**. Na każdym skrzyżowaniu drogi z nasypem toru (modyfikatorem terenu) powstaje obiekt:
   czoła równoległe do toru, skrzydła wzdłuż skarp do miejsca, gdzie teren schodzi do drogi, teren przed czołami wycięty, podłoga przy drodze.
   Teren nad obiektem i tor zostają bez zmian. Jeśli droga była dociągnięta do terenu, dociąganie robi się od nowa z przerwą na obiekty.
4. Jeśli nasyp jest za niski, status mówi, o ile obniżyć drogę albo zmniejszyć skrajnię.
5. „Usuń je” zdejmuje obiekty tej drogi i zamyka dziury. Modele są w `models/kabina_obiekty/`.

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

## Idź do

GeoTools → **Idź do**: mapa z kamerą, wyszukiwarka miejscowości (polskie znaki obojętne), zapisane kamery w `<sceneria>.kamery`.

## Gdy coś nie działa

- Sprawdź numer wersji w tytule okna.
- Długa operacja bez klepsydry albo z „Nie odpowiada”: zapisz, co klikałeś, i `log.txt`.
- Zrzut ekranu z widocznymi podpowiedziami na dole bardzo pomaga.
