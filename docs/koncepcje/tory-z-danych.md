# Koncepcja: tory z danych (GeoTools → Tory)

Stan: **koncepcja do omówienia**, nic z tego nie jest jeszcze w kodzie edytora. Pisane 12.11.2026, uzupełnione o nasze skrypty IRT v4/v5.

Cel: zaznaczyć kawałek scenerii (chunk + promień), pobrać dla niego przebieg torów z geoportalu, ułożyć je w edytorze jako **połączone tory MaSzyny z pełnymi parametrami**, wyprostowane do przepisów (ST-T1-A6), posadzone na danych wysokościowych (NMT), z terenem dociągniętym do torowiska. To, co robiły nasze skrypty w Pythonie, tylko w edytorze i kawałkami.

## 0. Co już mamy (skrypty IRT v4/v5, Python)

Cały łańcuch działa już poza edytorem i dał torowisko całego Dolnego Śląska (v4: 120 107 torów, 3335 km, 62 rozjazdy; v5: do tego 78 827 powierzchni torowiska i skarp):

| Skrypt | Co robi | Reguły do przeniesienia |
|---|---|---|
| `irt2maszyna.py` | osie IRT (EPSG:2180) → proste, przejściówki, łuki, przechyłka; tor 2; zapis `.inc` | rozdz. 9 A6: Rmin P0/P1/P2 = 600/300/180 m, Idop 110/130/150 mm, dD/ds, dD/dt, dI/dt, Dmin 20 mm, Dmax z tab. 9.6 i (9.8), przejściówki z tab. 9.10, proste między łukami z tab. 9.1, scalanie łuków, lokalne V = √((D+Idop)·R/11,8) w dół do 10 km/h; **tor 2 = 4,00 m w lewo (w kierunku kilometracji), gdy NT ≥ 2**, odstęp zaokrąglony do 0,05 m |
| `uklad.py` | rozjazdy i dowiązania | katalog Rz z `editorSwitchDesign.cpp` (60E1/49E1, 1:9/1:12/1:18,5), wybór typu z prędkości linii i jazdy na bok, wpis `track switch` jak `editor_track::create_switch`, rekord `//$r` jak `editorSwitchStore` |
| `profil.py` | niweleta z NMT 1 m (WCS GUGiK) | rozdz. 10 A6: pochylenia tab. 10.1, łuki pionowe tab. 10.2, strefy stacji i peronów |
| `teren.py` | heightmapa z formacją | spód podsypki 0,2 m pod torem, półszerokość formacji 3,52 m, skarpy 1:1,5 do 60 m, zaokrąglenie 1,5 m; **pod mostami i w tunelach teren nietknięty** |
| (v4) obiekty | tunel/most z terenu | teren 8 m nad niweletą = tunel, niweleta 5 m nad terenem = most; linia nad linią ≥ 2,5 m = wiadukt na całej długości, gdzie nasyp zasypałby dolną |
| (v5) powierzchnie | torowisko i skarpy L/P po ≤ 100 m | nazwy „L132_2 torowisko km …”, „nasyp L”, „przekop P”; to samo co nasze GeoTools → Torowisko i skarpy |

**Dane**: `IRT_koleje_dolnoslaskie.geojson` (943 obiekty, EPSG:2180): 605 osi linii (`MultiLineString`) z atrybutami `LN` (nr linii), `km_s`/`km_e`, `v_k` (prędkość), `NT` (liczba torów: 1, 2, 11), `EL` (elektryfikacja), oraz 338 punktów stacji (`SN` nazwa, `TYPE`, `SI`). **To osie linii, nie osie torów**: tor 2 i tory stacyjne powstają z reguł. Plik pochodzi z usługi ArcGIS REST za przeglądarką [`geoportal.dolnyslask.pl/imap/#gpmap=gp107`](https://geoportal.dolnyslask.pl/imap/#gpmap=gp107): `https://geoportal.dolnyslask.pl/gprest/services/UMWD_IRT/MapServer`, warstwa **8** (linie), **4** i **5** (stacje główne i pozostałe), zapytanie `/{warstwa}/query?where=1=1&outFields=*&returnGeometry=true&outSR=2180&f=json`. Usługa **wymaga tokenu**, który przeglądarka mapy dostaje przy otwarciu kompozycji; bez niego odpowiada odmową, a z naszego serwera geoportal nie jest osiągalny, dlatego plik pobrano w przeglądarce (31.10.2026). Edytor nie ma skąd wziąć tokenu, więc źródłem pozostaje plik lokalny, odświeżany ręcznie. Licencja IRT: bezpłatnie z podaniem źródła.

**Wniosek dla edytora**: nie wymyślamy reguł. Przenosimy te z Pythona, a tam, gdzie maj00r ma w `editor/editorAlignment` to samo (przejściówki, przechyłka, rampy, `limits`), bierzemy jego kod i porównujemy wynik z v4 na tych samych odcinkach (raporty `raport_lukow.csv`, `raport_profilu.json`, `raport_ukladu.csv` jako wzorzec). Różnice zapisujemy, zanim cokolwiek pójdzie do PR.

## 1. Na czym to stoi (co już jest w edytorze)

Najważniejsze odkrycie: **rdzeń „naginania do przepisów” jest już w masterze** (maj00r, `editor/editorAlignment`, narzędzie układania toru `editormodelay`, trasa `editormoderoute`, profil `editormodeprofile`). Nie piszemy go od nowa, podajemy mu punkty z danych zamiast klikania.

| Potrzeba | Co już jest (master) |
|---|---|
| Proste, łuki, krzywe przejściowe (parabola 3. st. / klotoida), przechyłka, rampy przechyłki wg ST-T1-A6 | `alignment::design`, `vertex`, `limits`, `recommend()`, `transition_length()` |
| Dopasowanie łuków do punktów wzdłuż osi toru | `fit_trace_stretch()`, `fit_going_on()`, `ease_corners()` (+ `stretch_fitter` w osobnym wątku) |
| Ułożenie torów z projektu (kawałki, połączenia, rozjazdy) | narzędzie układania (`lay_*`), `pieces()` / `pieces_at()`, narzędzie zwrotnic |
| Niweleta: spadki i łuki pionowe | profil podłużny (`editormodeprofile`): niweleta dopasowana tak, żeby **formacja** szła po gruncie |
| Teren pod torem | kształtowanie terenu jako **modyfikator** wzdłuż osi (formacja + skarpy), „Teren podąża za edytowanymi torami” (NMT), ostrzeżenie o torze zakopanym (`nmt_check_buried`) |
| Wysokości | NMT z GUGiK (chunki terenu z modelu, `nmt_make`) |
| Nasze z GeoTools | wycinanie powierzchni (Pola, Torowisko i skarpy), znaczniki dziur (254 pola, 255 ręczne), pobieranie z cache per obszar (jak ZIELSKO) |

## 2. Źródła danych (do wyboru, w tej kolejności sprawdzenia)

1. **IRT: „Mapa sieci kolejowej” na Geoportalu Dolny Śląsk** (Instytut Rozwoju Terytorialnego; linie PLK i wojewódzkie; licencja: bezpłatnie z podaniem źródła). Na start **lokalny plik** `IRT_koleje_dolnoslaskie.geojson` (osie linii + stacje, opis w pkt 0) wczytany raz; usługa na żywo (`UMWD_IRT/MapServer`, pkt 0) wymaga tokenu z przeglądarki, więc pobierania per obszar z edytora nie planujemy.
2. **BDOT10k (GUGiK), cała Polska**: obiekty torów (klasa „tor lub zespół torów”) z atrybutami. Pobieranie per powiat (GML). Do sprawdzenia: dokładność osi i czy rozróżnia pojedyncze tory na stacjach.
3. **OpenStreetMap (Overpass)**: na wielu polskich liniach **oś każdego toru osobno**, rozjazdy jako węzły (`railway=switch`), `gauge`, `electrified`, `voltage`, `maxspeed`, `railway:track_ref`, a do tego **obiekty**: `bridge=yes` / `tunnel=culvert` na odcinkach linii i ciekach. Licencja ODbL: wymaga podania źródła; czy scenerię zrobioną z OSM traktować jako bazę pochodną, trzeba ustalić z zespołem, zanim cokolwiek z tego pójdzie do paczki.

Każde źródło jako osobny „dostawca” za jednym interfejsem: obszar w PUWG 1992 → lista linii (punkty + atrybuty). Wtedy dołożenie kolejnego województwa albo źródła nie rusza reszty.

## 3. Obszar i pobieranie

- W GeoTools nowa zakładka **Tory**. Klik w chunk (albo obrys, jak w Polach/Drzewach) + **promień w chunkach**; podgląd zaznaczonego obszaru.
- Pobranie per chunk, wynik w cache terenu (`terrain/<sceneria>/tory/<chunk>.json`), jak ZIELSKO: drugi raz się nie pobiera, „Odśwież” wymusza.
- Linie na krawędziach obszaru dociągnięte do najbliższego punktu przecięcia z jego brzegiem, żeby kolejne pobranie obok łączyło się z tym, co leży.

## 4. Od linii z danych do torów MaSzyny

1. **Graf**: końce linii z danych sklejone w tolerancji (np. 0,5 m), węzły stopnia 3+ = kandydaci na rozjazdy. Oś linii wielotorowej (jeśli źródło tak daje) rozdzielona na tory z rozstawem z normy (domyślnie 4,0 m, do ustawienia; wartość do sprawdzenia w ST-T1-A6 dla danej prędkości).
2. **Odcinki między węzłami** → punkty co ~10 m → `fit_trace_stretch()` / `ease_corners()`: proste, łuki, przejściówki i przechyłka **według prędkości** (z atrybutu, inaczej do wpisania). Odchyłka od danych (`rms`, `worst`) pokazana na odcinku: duża = dane albo dopasowanie do sprawdzenia.
3. **Rozjazdy** w węzłach: typ dobrany z kąta i promienia (z narzędzia zwrotnic), tory spięte z ich końcami. Gdzie się nie da dobrać, węzeł zostaje zaznaczony do ręcznego ułożenia, a nie zgadywany.
4. **Parametry toru** jak w pliku scenerii: rodzaj szyny, podsypka, przechyłka, prędkość, elektryfikacja (sieć trakcyjna osobno, poza tym etapem), nazwy/numery torów; domyślne z ustawień zakładki, nadpisywane atrybutami źródła.
5. **Połączenia z tym, co już leży**: koniec nowego toru w tolerancji od wolnego końca istniejącego = przejście tak jak przy zwykłym układaniu (ciągłość kierunku i krzywizny); inaczej koniec zostaje wolny i zaznaczony.

## 5. Wysokości, niweleta i teren

- Wysokość osi z NMT pod torem → **profil podłużny** (istniejący): niweleta ze spadkami i łukami pionowymi wg norm, formacja po gruncie.
- Teren pod torowiskiem: **modyfikator formacji i skarp wzdłuż osi** (istniejący) + „Teren podąża za torami”. Po ułożeniu sprawdzenie „tor zakopany / wystaje” (istniejące `nmt_check_buried`) i zestawienie miejsc do poprawki.
- Wszystko w **jednym kroku historii** (Ctrl+Z cofa całe ułożenie z terenem).

## 6. Obiekty: wiadukty, mosty, przepusty, przejścia

NMT to goła powierzchnia (heightmapa, bez nawisów): pod wiaduktem ma dolinę albo drogę. Formowanie ziemi pod tor „na ślepo” zasypałoby drogę albo rzekę.

- **Wykrywanie**: odcinek, na którym niweleta wychodzi wyraźnie nad NMT **i** dane mówią o skrzyżowaniu (OSM `bridge`/`culvert`, drogi i cieki z BDOT10k/OSM). Sama wysokość to tylko podejrzenie, nie obiekt.
- **Na obiekcie bez formowania**: tor idzie po niwelecie, teren zostaje, jak jest. Takie miejsca **żółte w podglądzie** do sprawdzenia przed „Połóż tory”.
- **Wycinanie tylko na żądanie**, i to **własnym znacznikiem dziury** (trzeci obok 254 pól i 255 ręcznych), żeby przebudowa obiektu nie ruszała dziur pól ani ręcznych. Krawędź dziury idzie po siatce co metr: do zasłonięcia przyczółkiem/skarpą.
- **Skarpy wewnętrzne przy przejściu** (lewa i prawa, od strony otworu): łagodna krzywa od poziomu dna przejścia (droga, ciek) do nasypu, lekko falowana (ten sam szum co naturalne skarpy), **każda wycięta jako osobna powierzchnia** z własnym materiałem i mapowaniem (domyślnie wzmocnienie: bruk, płyty, kamień w siatce), na liście jak pola. Niezależne od rodzaju obiektu: most, łuk czy przepust stawia się osobno.

## 7. Kolejność wdrażania (propozycja)

1. Dostawca IRT z lokalnego GeoJSON + zakładka Tory: obszar (chunk + promień), wycięcie linii z obszaru, **podgląd osi** na mapie i w widoku z atrybutami (nr linii, km, v, NT, EL). Nic nie kładzie.
2. Graf + dopasowanie odcinków (`fit_trace_stretch`) + podgląd projektu z odchyłkami; „Połóż tory” bez rozjazdów, wolne końce zaznaczone.
3. Rozjazdy w węzłach + łączenie z istniejącymi torami.
4. Niweleta z NMT + formacja i skarpy + sprawdzenie zakopania, jeden krok historii.
5. Obiekty: wykrywanie, brak formowania, żółte miejsca; potem wycinanie z własnym znacznikiem i skarpy wewnętrzne jako powierzchnie.
6. Kolejne źródła (BDOT10k, OSM) i województwa.

Każdy krok osobno do sprawdzenia w grze i osobno jako PR (zakładka GeoTools jest już planowana jako PR do mastera).

## 8. Do ustalenia z Tobą

- ~~Adres usługi IRT i skrypty~~: skrypty są (pkt 0); GeoJSON lokalny wystarcza na start, usługa na żywo później.
- ~~Oś linii czy osie torów~~: osie linii; tor 2 z reguły 4,00 m, **tory stacyjne** (README v4: „układy stacyjne trzeba dorysować”) nadal ręcznie albo z OSM, gdzie są tory osobno.
- Czy w edytorze trzymamy się wyniku v4/v5 jako wzorca (te same reguły = ten sam tor), czy dopuszczamy, że dopasowanie maj00ra da trochę inne łuki.
- Domyślne parametry toru dla Dolnego Śląska (szyna, podsypka, prędkość, gdy brak w danych).
- OSM: czy w ogóle wchodzimy w ODbL.
- Czy obiekty (most/przepust) stawiamy też automatycznie z biblioteki modeli, czy tylko przygotowujemy pod nie teren.

## Przegląd skryptów v4 (12.11.2026): co przenosić, a co poprawić przy przenoszeniu

Wynik v4 w liczbach (`podsumowanie.json`, `raport_ukladu.csv`): 113 łańcuchów, 3428 łuków, z tego 1190 z lokalnym ograniczeniem prędkości; oś po korekcji leży średnio 2,25 m od punktów IRT, mediana p95 4,8 m, w 18 łańcuchach ponad 20 m, najgorzej L275 (110 m); 24 miejsca niepołączone (17 dowiązań i 7 połączeń koniec-koniec pominiętych).

Do poprawienia, zanim to trafi do edytora:

1. **Przepusty i przejazdy pod torem giną.** `mark_env` daje `bridge` dopiero, gdy teren jest ponad 5 m pod niweletą, a profil idzie przez filtr medianowy 50 m. Wąskie obniżenie (droga pod wiaduktem, przepust, 2-5 m) nie jest mostem, więc formacja je zasypuje. Potrzebne wykrywanie wąskich obniżeń w poprzek osi i dane o obiektach (BDOT10k/OSM).
2. **Nasyp przy przyczółku kończy się ścianą.** `Formation.apply` ucina formację prostopadle na końcu odcinka przed mostem (`open_start/open_end`); za tą linią teren wraca od razu do NMT. Brak skarp wewnętrznych i stożków, czyli dokładnie tego, co ma być osobną powierzchnią.
3. **Formacja bierze tylko najbliższą oś.** Między torami na różnych wysokościach (dojazd do wiaduktu, węzeł) teren ma uskok w połowie odległości, bez przejścia.
4. **Tor 1 leży na osi linii, tor 2 zawsze 4,00 m w lewo.** Oś IRT to oś linii, więc para torów jest przesunięta o 2 m względem danych, a strona toru 2 nie wynika z niczego. Lepiej ±e/2 od osi, a na łukach R < 250 m poszerzenie międzytorza wg A6.
5. **Dopasowanie liczone dla całego łańcucha.** Łuki zależą od sąsiadów (scalanie, prostowanie, zmniejszanie R), więc liczenie osobno dla chunka z promieniem da inne tory na styku dwóch obszarów. W edytorze: projekt osi całej linii liczony raz i zapisany, a kładzione są tylko kawałki z obszaru.
6. **Promień wybierany jako największy w zapasie 1 m RMS** przy punktach co 50 m do 2 km: promienie są zawyżone i słabo określone. Do sprawdzenia z ortofotomapą (narzędzie maj00ra dopasowuje do punktów ze zdjęcia).
7. **Odcinki dwutorowe krótsze niż 200 m są pomijane**, a rozjazd na końcu dwutorowego jest przesuwany do 1 km w poszukiwaniu prostej; oba przypadki zmieniają układ bez śladu w scenerii (tylko w raporcie).
8. Drobne: `--cache` wczytuje `_cache.pkl` bez sprawdzenia, czy zmieniły się parametry albo dane; profil próbkowany tylko po osi toru 1; brak układów stacyjnych (338 stacji trafia tylko do `stacje.txt`).

Bez zastrzeżeń do przeniesienia: tabele A6 (przechyłka, niedomiar, przejściówki, proste), lokalne ograniczenie prędkości z wygładzaniem krótkich odcinków, katalog Rz i zapis rozjazdu, most przechyłkowy na łukach koszowych, wiadukty na skrzyżowaniach linii, podział na kawałki (proste do 1000 m, łuki do 100 m i 10°, przejściówki do 10 m).
