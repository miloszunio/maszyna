# Koncepcja: tory z danych (GeoTools → Tory)

Stan: **koncepcja do omówienia**, nic z tego nie jest jeszcze w kodzie. Pisane 12.11.2026.

Cel: zaznaczyć kawałek scenerii (chunk + promień), pobrać dla niego przebieg torów z geoportalu, ułożyć je w edytorze jako **połączone tory MaSzyny z pełnymi parametrami**, wyprostowane do przepisów (ST-T1-A6), posadzone na danych wysokościowych (NMT), z terenem dociągniętym do torowiska. To, co robiły nasze skrypty w Pythonie, tylko w edytorze i kawałkami.

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

1. **IRT: „Mapa sieci kolejowej” na Geoportalu Dolny Śląsk** (Instytut Rozwoju Terytorialnego; linie PLK i wojewódzkie; licencja: bezpłatnie z podaniem źródła). Katalog podaje tylko przeglądarkę (`geoportal.dolnyslask.pl/imap/#gpmap=gp107`), **adresu usługi nie ma**: weźmiemy go z naszych skryptów w Pythonie. Do ustalenia: czy to oś linii (jedna linia na wielotorową), czy oś każdego toru, i jakie atrybuty (nr linii, liczba torów, elektryfikacja, prędkość).
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

1. Dostawca IRT (adres z naszych skryptów) + zakładka Tory: obszar, pobranie, cache, **podgląd linii** na mapie i w widoku. Nic nie kładzie.
2. Graf + dopasowanie odcinków (`fit_trace_stretch`) + podgląd projektu z odchyłkami; „Połóż tory” bez rozjazdów, wolne końce zaznaczone.
3. Rozjazdy w węzłach + łączenie z istniejącymi torami.
4. Niweleta z NMT + formacja i skarpy + sprawdzenie zakopania, jeden krok historii.
5. Obiekty: wykrywanie, brak formowania, żółte miejsca; potem wycinanie z własnym znacznikiem i skarpy wewnętrzne jako powierzchnie.
6. Kolejne źródła (BDOT10k, OSM) i województwa.

Każdy krok osobno do sprawdzenia w grze i osobno jako PR (zakładka GeoTools jest już planowana jako PR do mastera).

## 8. Do ustalenia z Tobą

- Adres usługi IRT i format (z naszych skryptów w Pythonie); same skrypty, żeby przenieść reguły, które tam działały (tolerancje, rozstawy, wyjątki).
- Czy IRT daje oś linii czy osie torów; skąd brać tory stacyjne.
- Domyślne parametry toru dla Dolnego Śląska (szyna, podsypka, prędkość, gdy brak w danych).
- OSM: czy w ogóle wchodzimy w ODbL.
- Czy obiekty (most/przepust) stawiamy też automatycznie z biblioteki modeli, czy tylko przygotowujemy pod nie teren.
