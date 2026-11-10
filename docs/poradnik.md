# Poradnik: jak korzystać z dodatków ficzery2

Krótko i po kolei: co kliknąć, żeby zrobić typowe rzeczy w scenerii. Opis zmian w każdej wersji jest wyżej, tu jest sposób pracy.
Wszystko, co zmienia scenerię, ma **żółty** przycisk, a podpowiedzi w widoku są żółte. Prawie wszystko cofa **Ctrl+Z**.

## Na start

- Numer wersji jest w tytule okna i w `WERSJA.txt`. Zgłaszając błąd, podawaj go.
- **~** (tylda, klawisz na lewo od 1) włącza i wyłącza ortofotomapę na terenie, **Ctrl+~** najpierw ściąga kafelki tam, gdzie patrzy kamera.
- **Ficzery** to dział **F7** na górnej belce. Drogi to **F4**, Tory **F3**.
- Ciężkie operacje (torowisko i skarpy na całej scenerii, przemalowanie, film, dociąganie terenu, obiekty pod torami) pokazują **klepsydrę** na zamrożonym ekranie.
  Postęp w procentach jest w tytule okna, **Esc** przerywa tam, gdzie się da. To, co zrobione, zostaje i da się cofnąć.

## Teksturowanie terenu

1. Ficzery (F7) → paleta: kafelki materiałów w grupach. LPM wybiera materiał, **1–9** pierwsze dziewięć, **Alt+LPM** w widoku bierze materiał spod kursora.
2. Pędzel: **Mieszaj** (przenika), **Kryj** (zastępuje), **Zmazuj**. Pod paletą widać, co jest pod kursorem i ile materiałów ma kawałek (max 8; czerwone = brak miejsca).
3. PPM na kafelku: skala, obrót, zmiana materiału w całej scenerii naraz (bez dokładania nowej pozycji), grupa, zdjęcie z palety z zamiennikiem.
4. **Zamień w obrysie** (Teren → Wypełnij): narysuj obrys (albo magiczną różdżką), wybierz materiał „z” i kliknij „na zaznaczony”. Reszta scenerii zostaje.
5. **Wyczyść malowanie całego terenu**: każdy kawałek wraca do pierwszego materiału. Przed tym robi się kopia `terrain\<nazwa>_przed_czyszczeniem` (Ctrl+Z tego nie cofa).

## Wycinanie powierzchni (pola, łąki, place)

1. Ficzery → **Wytnij powierzchnię**. Rysuj obrys: LPM punkt, Shift+LPM cofa. Kształt obrysu: zaokrąglenia, gładka krzywa, Bézier, z ręki, wzdłuż toru.
2. Albo **magiczna różdżka**: włącz i kliknij na ortofoto w pole, obrys zrobi się sam z obszaru o podobnym kolorze.
3. Wybierz materiał w palecie, przejście i szum krawędzi, trawę 3D pod spodem.
4. Jeśli ma falować: „Falowanie terenu” (pagórki, bruzdy, pasy pól, kratka…), profil fali, chropowatość.
5. Żółty „Wytnij jako osobną powierzchnię”. Powierzchnia jest malowana w teren, bez szwu.
6. Lista „Powierzchnie wycięte z terenu”: zmiana materiału, „Idź do”, „Obrysuj ponownie”. Stare powierzchnie z dziurą (np. z IRT) zamienia na malowane „Przemaluj je w teren”.

## Torowisko i skarpy

1. Ficzery → **Torowisko i skarpy**. Wybierz materiały torowiska i skarpy w palecie, długość kawałka, zasięg skarpy, przejścia.
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

Ficzery → **ZIELSKO z kosmosu** (ikona trawy). Drzewa z modeli 3D drzew GUGiK: każde stoi tam, gdzie w rzeczywistości, i ma swoją wysokość.
Dane nie mówią nic o gatunku i koronie (korona jest szablonem skalowanym do wysokości), więc modele bierzesz z banku węzłów.

1. **Dane.** Ustaw, do ilu metrów od torów zachowywać drzewa (domyślnie 300 m), i kliknij żółte „Znajdź powiaty i wczytaj ich drzewa”.
   Edytor pyta Geoportal, które powiaty leżą pod scenerią, i w tle czyta ich paczki prosto z sieci (powiat to 1,5–5 GB; na dysk nic z tego nie trafia).
   Zostają tylko drzewa nad terenem w pasie wzdłuż torów, zapisane w `terrain/<teren>/zielsko/<TERYT>.drz`. Wczytany powiat jest pomijany przy następnym razie,
   chyba że zaznaczysz „Wczytaj ponownie”. Można w tym czasie pracować dalej; „Przerwij wczytywanie” zatrzymuje.
2. **Sito.** Zasięg: wokół kamery, cała sceneria albo w obrysie (LPM stawia punkty jak przy wycinaniu powierzchni).
   Najbliżej i najdalej od osi toru, odstęp od krawędzi dróg, najniższe drzewo, odstęp między drzewami (przerzedzenie lasu, zostaje wyższe), limit liczby.
   Woda terenu zawsze zostaje pusta. Gdy jest więcej drzew niż limit, idą najpierw te najbliżej torów.
3. **Modele.** Grupa banku węzłów na drzewa (domyślnie „Duze drzewka”) i na niskie (domyślnie „Krzaki”, poniżej 8 m).
   Wysokość modeli w skali 1: „z modelu” bierze promień modelu, a jeśli drzewa wychodzą za duże albo za małe, wpisz ją ręcznie.
4. „Policz i pokaż” rysuje kropki do 1,5 km od kamery (żółte niskie, zielone wysokie) i podaje liczbę. Żółte **„Posadź”** stawia modele `zielsko_<n>`
   (wcześniej posadzone przez ZIELSKO najpierw znikają). „Wykarczuj posadzone” je zdejmuje.
5. W opisie scenerii podaj źródło: **GUGiK, modele 3D drzew (geoportal.gov.pl), CC BY 4.0**.

## Idź do

Ficzery → **Idź do**: mapa z kamerą, wyszukiwarka miejscowości (polskie znaki obojętne), zapisane kamery w `<sceneria>.kamery`.

## Gdy coś nie działa

- Sprawdź numer wersji w tytule okna.
- Długa operacja bez klepsydry albo z „Nie odpowiada”: zapisz, co klikałeś, i `log.txt`.
- Zrzut ekranu z widocznymi podpowiedziami na dole bardzo pomaga.
