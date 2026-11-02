# Normy PLK do edytora: ściąga

Zebrane 2 listopada 2026 na kolejne poprawki edytora gałęzi `gse`. Punktem wyjścia był sylabus „Projektowanie linii kolejowych” Politechniki Poznańskiej (dr inż. M. Pawłowski) i jego lista literatury. Liczby niżej pochodzą z dokumentów źródłowych i są przepisane do tabel. Przy rozbieżności rozstrzyga oryginał.

Progi z ST-T1-A6: **P0** to wartość zalecana, **P1** normalna dopuszczalna, **P2** graniczna, która wymaga zgody Zakładu Linii Kolejowych i jest przewidziana głównie przy modernizacji.

Wyciąg z podręczników (Bałuch, Massel, Cieślakowski, Sysak) jest w `docs/literatura-kolejowa.md`.

## Źródła i gdzie są

| Dokument | Status | Dostęp |
|---|---|---|
| **ST-T1-A6 Układy geometryczne torów** (załącznik do Standardów Technicznych PLK, tom I) | obowiązuje od 17.04.2025 | mamy PDF; główne źródło liczb poniżej |
| Standardy Techniczne PLK, tom I „Droga szynowa” (Vmax ≤ 200/250 km/h) | wersja 2019 i późniejsze | strona PLK (akty prawne → standardy) |
| **Rozporządzenie MI z 17.09.2026 w sprawie warunków technicznych budowli kolejowych** (Dz.U. 2026 poz. 1225) | **obowiązuje od 20.09.2026**, zastępuje rozporządzenie z 1998 r. (Dz.U. 1998 nr 151 poz. 987) | https://eli.gov.pl/api/acts/DU/2026/1225/text/O/D20261225.pdf |
| Id-1 (D-1) Warunki techniczne utrzymania nawierzchni | obowiązuje, tekst ujednolicony | https://www.plk-sa.pl/files/public/user_upload/pdf/Akty_prawne_i_przepisy/Instrukcje/Wydruk/Id/Warunki_techniczne_Id-1_ujednolic..pdf |
| Id-3 (D-4) Warunki techniczne utrzymania podtorza | obowiązuje | https://www.plk-sa.pl/files/user_upload/Id-3.pdf |
| Id-4 (D-6) Instrukcja o oględzinach, badaniach technicznych i utrzymaniu rozjazdów | obowiązuje | strona PLK, instrukcje Id |
| Standardy kolejowe CPK, tom I.1 „Układy geometryczne” (Vmax ≤ 350 km/h, wersja 3.1 z 18.12.2025) | publiczne | https://portpolska.pl/wp-content/uploads/TOM-I.1-DROGA-SZYNOWA-–-UKLADY-GEOMETRYCZNE-1.pdf |
| CPK tom I.4 „Skrajnia” (perony, rozstaw osi) | publiczne | https://portpolska.pl/wp-content/uploads/TOM-I.4-DROGA-SZYNOWA-–-SKRAJNIA-1.pdf |
| CPK tom I.2 „Konstrukcja obiektów budowlanych” (przekrój normalny, rozjazdy) | publiczne | portpolska.pl, tom I.2 |
| Szablowska, „Aspekty projektowania rozjazdów w łukach”, Autobusy 6/2019 | artykuł | baztech (yadda.icm.edu.pl); wzory na promień toru zwrotnego rozjazdu łukowanego wg ST |
| Bałuch H., Bałuch M., „Układy geometryczne toru i ich deformacje”, PLK 2010; Massel A., „Projektowanie linii i stacji kolejowych”, PLK 2010; Cieślakowski, Chełmecki, Węgierski (stacje) | książki | tylko papier lub biblioteki, w sieci ich nie ma |

## Plan: promienie, długości, przechyłka (ST-T1-A6, rozdz. 9)

**Promień R (tab. 9.2)**

| Tor | Rmin P0 | P1 | P2 |
|---|---|---|---|
| szlakowe i główne zasadnicze | 600 m | 300 m | 180 m |
| przy peronach | 500 m | 500 m | 300 m |
| główne dodatkowe i boczne | 450 m | 180 m | 180 m |

- Zalecany zakres promieni to 600–20 000 m. Rmax wynosi 20 000 (P0) i 50 000 (P1).
- Wstępnie: Rmin = 11,8·V²/(D + Idop) oraz Rmax = 11,8·Vmin²/(D − Edop).
- Zalecane Rmin wg typu linii (tab. 9.3), w warunkach normalnych / trudnych:

| Typ linii | Rmin |
|---|---|
| P250 | 3100 / 2700 |
| P200 | 2000 / 1700 |
| M200 | 2300 / 2000 |
| P160 | 1300 / 1100 |
| M160 | 1500 / 1350 |
| P120 | 750 / 650 |
| M120, T120 | 850 / 750 |
| P80 | 600 / 300 |
| M80, T80 | 600 / 350 |
| T40 | 600 / 300 |

**Długość prostej i łuku L (tab. 9.1)**

| Vmax | P1 | P2 |
|---|---|---|
| ≤ 120 km/h | max(0,35·V; 30 m) | max(0,25·V; 20 m) |
| 120–200 km/h | 0,40·V | 0,30·V |
| 200–250 km/h | 0,65·V | 0,55·V |

**Przechyłka D (tab. 9.5–9.6; mm, V w km/h, R w m)**
- Wzory:
  - Deq = 11,8·Vmax²/R
  - Dreg = 6,5·Vmax²/R
  - Dmin ≥ 11,8·Vmax²/R − Idop
  - Dmax ≤ 11,8·Vmin²/R + Edop
  - dla R < 300 m dodatkowo D ≤ (R − 50)/1,5
- Wartości maksymalne:

| Tor | P0 | P1 | P2 |
|---|---|---|---|
| szlakowe i główne, podsypka | 150 | 150 | 150 |
| szlakowe i główne, bezpodsypkowa | 150 | 150 | 160 |
| przy peronach | 60 | 100 | 110 |
| boczne | 0 | 60 | 100 |
| 200 < R ≤ 300 | 0 | 100 | 150 |
| R ≤ 200 | 0 | 60 | 100 |
| połączenia z rozjazdów zwyczajnych | 0 | 100 | 110 |
| połączenia z rozjazdów łukowych | 60 | 100 | 110 |
| krzyżownice podwójne, dwukrotne, trzykrotne | 0 | 0 | 0 |

- Minimalna przechyłka, jeśli się ją stosuje, to 20 mm.
- Zalecenie dla jednolitej prędkości: tak dobrać D, żeby I wynosiło 10–20 mm. W innych przypadkach D z przedziału ⟨Dreg; Deq⟩.
- Łuk koszowy: jedna wspólna przechyłka, jeśli różnice Dreg łuków składowych nie przekraczają 25 mm. W przeciwnym razie między łukami składowymi muszą być krzywe przejściowe.
- Przechyłka odwrotna jest niedozwolona w torach szlakowych i głównych zasadniczych nowych linii.

**Niedomiar przechyłki I = 11,8·V²/R − D (tab. 9.7; mm)**

| Przypadek | P0 | P1 | P2 |
|---|---|---|---|
| szlakowe, główne zasadnicze i dodatkowe | 110 | 130 | 150 |
| boczne | 80 | 100 | 100 |
| 200 < R ≤ 250 | 80 | 100 | 100 |
| R ≤ 200 | 50 | 70 | 70 |
| rozjazdy ze stałą krzyżownicą, V ≤ 160 | 90 | 110 | 110 |
| rozjazdy ze stałą krzyżownicą, 160–200 | 60 | 90 | 90 |
| rozjazdy z ruchomym dziobem, V ≤ 230 | 100 | 130 | 130 |

**Rampy i krzywe przejściowe (9.6–9.10)**
- Rampa prostoliniowa o długości krzywej przejściowej. Krzywa przejściowa w postaci klotoidy albo paraboli 3. stopnia.
- Pochylenie rampy dD/ds dla V ≤ 250: 1,6 / 2,0 / 2,5 mm/m (P0/P1/P2).
- dD/dt: 35 / 50 / 60 mm/s.
- dI/dt:
  - V ≤ 200 km/h: 55 / 70 / 80 mm/s;
  - 200–250 km/h: P1 55, P2 70 mm/s.
- Minimalna długość krzywej przejściowej to maksimum z warunków dD/ds, dD/dt i dI/dt oraz z 30 m. Ze względu na tyczenie przesunięcie łuku ma wynosić co najmniej 0,02 m.
- Łuki odwrotne ze stykającymi się rampami: suma pochyleń obu ramp nie może przekroczyć dopuszczalnego dD/ds. Jeśli się nie da, stosuje się „nożyce” albo krzywe Blossa.
- Łuki odwrotne bez krzywych przejściowych wymagają wstawki prostej wg tab. 9.4, gdy |1/R1| + |1/R2| przekracza wartość ze wzoru 9.3.

**Rozstaw osi torów (9.13)**
- Rozstaw ma być wielokrotnością 0,05 m. Tory projektuje się jako równoległe na prostych i współśrodkowe w łukach.
- Zmiana rozstawu o Δe ≤ 1,5 m: cztery krzywe przejściowe albo dwa łuki bez przechyłki z prostą między nimi. Wstępnie R ≥ V².
- Zmiana rozstawu o Δe > 1,5 m: dwa łuki odwrotne z krzywymi przejściowymi.

## Profil (ST-T1-A6, rozdz. 10)

**Pochylenie maksymalne (tab. 10.1, mm/m)**

| Tor | P0 | P1 | P2 |
|---|---|---|---|
| szlakowe i główne, linie P | 6,0 | 17,5 | 35,0 |
| szlakowe i główne, linie M i T | 6,0 | 10,0 (17,5 na odcinkach < 500 m) | 20,0 (35,0) |
| na stacjach, na długości użytecznej | — | 2,0 | 2,5 |
| przy peronach | 2,5 | 6,0 | 10,0 |

- Najdłuższy odcinek o pochyleniu maksymalnym: 6000 m dla linii P, 3000 m dla linii M i T.
- Średnie pochylenie na 10 km: P 12,5 / 25; M i T 6,25 / 12,5.
- Na szlaku zalecane pochylenie wynosi co najmniej 1 mm/m (ze względu na odwodnienie).
- Minimalna długość odcinka o stałym pochyleniu: 0,4·Vmax, a przy regulacji i w połączeniach 20 m (P1).

**Łuki pionowe (10.2, tab. 10.2)**
- Załom niwelety trzeba wyokrąglić, gdy różnica pochyleń przekracza 2 mm/m.
- Promień łuku pionowego na liniach P, M i T:
  - P0: max(0,77·V²; 5000 m);
  - P1: max(0,35·V²; 5000 m);
  - P2: max(0,25·V²; 2000 m).
- Promień w połączeniach torów:
  - wypukłe: P1 0,35·V² / 5000 m, P2 0,25·V² / 3000 m;
  - wklęsłe: P1 0,35·V² / 3000 m, P2 0,25·V² / 2000 m.
- Długość łuku pionowego co najmniej max(0,4·Vmax; 20 m).
- Początek i koniec łuku pionowego mają być co najmniej 6 m od końców ramp, przyrządów wyrównawczych, przejazdów i mostów bez podsypki.
- Zakazy:
  - w nowych liniach nie ma załomów na rampach, w połączeniach ani na przejazdach;
  - w połączeniach nie ma załomów bez wyokrąglenia;
  - nie ma łuków pionowych na przyrządach wyrównawczych ani na przejazdach.
- Górki rozrządowe: łuk pionowy co najmniej 250 m (wypukły) i 300 m (wklęsły).

## Rozjazdy i połączenia (ST-T1-A6, rozdz. 11)

**Pojedyncze połączenie torów równoległych (tab. 11.2)**

| Rozjazd | Minimalny rozstaw | V na bok |
|---|---|---|
| 190-1:9 | 3,70 m | 40 km/h |
| 300-1:9,403 | 4,00 m | 50 km/h |
| 300-1:9 | 4,35 m | 50 km/h |
| 500-1:12 | 4,00 m | 60 km/h |
| 760-1:14 | 4,75 m | 80 km/h |
| 1200-1:18,5 | 4,35 m | 100 km/h |
| 2500-1:26,5 | 4,80 m | 130 km/h |

- Prędkość zjazdu w połączeniach węzłowych (tab. 11.3, P0/P1):
  - tor wolniejszy 40–50 km/h: 50/40;
  - 60–70: 60/50;
  - 80–90: 80/60;
  - 100–120: 100 (120)/80;
  - 130–160: 130/100;
  - powyżej 170: dobór indywidualny.
- Połączenia dojazdowe dodatkowe mają skos o stopień mniejszy niż zasadnicze (tab. 11.5): 1:18,5 → 1:14 (1:12), 1:14 → 1:12 (1:9), 1:12 → 1:9.
- Połączenie dwóch torów rozjazdem podstawowym z rozjazdem łukowanym z Rz 300-1:9 (tab. 11.7):
  - V 60: Rz 500-1:12 z Rłd 751/500-1:9;
  - V 80, e = 4,75 m: Rz 760-1:14 z Rłd 760/497-1:9;
  - V 100: Rz 1200-1:18,5 z Rłd 1200/401-1:9.
- Odcinek pośredni między rozjazdami:
  - długość ma być wielokrotnością 0,05 m;
  - rozjazdy zwrócone do siebie iglicami, z odgałęzieniami w tę samą stronę: dopuszczalne 0 m, zalecane co najmniej 6 m;
  - przy V ≥ 160 km/h w nowych torach głównych co najmniej 30 m.
- W torach głównych zasadniczych nie stosuje się Rz 190-1:9 ani skosów 1:4,8, 1:6,6 i 1:7,5. Według rozporządzenia z 2026 r. (§ 23) zakazane są tam też 1:7.

## Stacje (ST-T1-A6, rozdz. 12)

**Długość pociągu wg typu linii (tab. 12.8)**

| Długość pociągu | Typy linii |
|---|---|
| 750 m | M200, M160, M120, T120 |
| 500–750 m | M80, T80, T40 |
| 400 m | P250, P200, P160 |
| 300–400 m | P120, P80 |

- Długość rzeczywista toru = długość pociągu + 10 m (niecelne zatrzymanie) + 10 m (widoczność sygnalizatora) + 5 m (do miejsca niebezpiecznego) + droga ochronna Do. Przykład: 750 m daje 775 m + Do.
- Tory ochronne razem z kozłem i zasypką: zalecane do 30 m.
- Na stacjach co najwyżej co 30 km: tor główny dodatkowy z obustronnie połączonym torem bocznym na postój maszyn torowych.
- Linie dwutorowe o ruchu mieszanym (M): zalecane dwa tory do wyprzedzania, po obu stronach torów głównych. Linie pasażerskie (P): wystarczy jeden, na kolejnych stacjach na przemian po obu stronach.

## Rozporządzenie z 17.09.2026: co nowe i do czego się przyda

- **Rmin w torach bezstykowych (§ 20):**
  - tory główne: 500 m na podkładach drewnianych, 450 m na betonowych;
  - tory stacyjne: 300 m;
  - od 190 m wzwyż tylko z dodatkowymi warunkami.
- **Poszerzenie toru w łuku (§ 22):**

| R | Poszerzenie |
|---|---|
| ≥ 250 m | 0 mm |
| 200–250 m | 10 mm |
| 180–200 m | 15 mm |
| 160–180 m | 20 mm |
| < 160 m | 25 mm |

  Poszerzenie wprowadza się na krzywej przejściowej albo na prostej przed łukiem.
- **Rozjazdy (§ 23):**
  - na bocznicach dopuszczalne R 140 m, skosy 1:7 i 1:5;
  - przy rozjazdach z krzyżownicami łukowymi wstawka prosta co najmniej 6 m i co najmniej V/10;
  - w połączeniach rozjazdami przyrost przyspieszenia niezrównoważonego najwyżej 1 m/s³.
- **Prowadnice w łukach R ≤ 250 m (§ 24):**
  - żłobek 60 mm;
  - prowadnica wydłużona o co najmniej 2 m za łuk, końce odgięte na 0,30 m pod kątem 30°;
  - przy krótkim łuku (do 50 m) prowadnice tylko dla R < 190 m.
- **Pochylenie poprzeczne szyn (§ 21):** 1:40, a dla 49E1 na drewnie 1:20. Między krótkimi rozjazdami (do 30 m) nie stosuje się. Do 60 km/h można je pominąć.
- **Odbojnice na mostach (§ 39):** w łukach R < 350 m. Na mostownicach przechyłka do 50 mm, powyżej tego siodełka.
- **Perony (§ 69, § 87):**
  - krawędź 1,30 m od osi toru na prostej i w łukach R > 800 m, w ciaśniejszych łukach poszerzona wg skrajni;
  - wysokość 760 lub 550 mm nad główką szyny;
  - długość równa najdłuższemu pociągowi pasażerskiemu, co najmniej 50 m;
  - szerokość: jednokrawędziowy co najmniej 2,50 m, wyspowy co najmniej 3,30 m;
  - słupy i latarnie co najmniej 3 m od osi toru;
  - peron nie może leżeć na pochyleniu większym niż 6‰ (10‰ na przystankach).
- **Kozły i drogi ochronne (§ 27):** zasypka żwirowa 100 mm nad główką na długości 30 m (15 m przy semaforach wyjazdowych).
- **Przepis przejściowy (§ 113):** stare przepisy dalej obowiązują budowy, które przed 20.09.2026 miały złożony wniosek o pozwolenie, pozwolenie, zgłoszenie albo decyzję legalizacyjną. Pozostałe parametry geometrii (przechyłka, krzywe przejściowe) rozporządzenie zostawia TSI Infrastruktura i ustaleniom z zarządcą, czyli w praktyce ST-T1-A6.

## Co już jest wdrożone

- **Magic przy rozjazdach** (`application/editormodeswitchmagic.cpp`, granice w `editor/editorNorms.hpp`):
  - Rmin wg tab. 9.2 dla rodzaju toru;
  - I wg tab. 9.7;
  - ΔI wg tab. 9.15, z wstawką prostą wg tab. 9.16;
  - progi P0, P1 lub P2;
  - prosta między łukami odwrotnymi chroniąca przed zakleszczeniem zderzaków (pkt 9.2.9–10).
- **`tools/plk/plk_audit.py`**: kontrola i poprawa (przechyłka, prędkość) całej scenerii, opis w `tools/plk/README.md`.

## Co z tego wynika dla edytora (pomysły na poprawki)

- **Magic przy rozjazdach:**
  - łuk odwrotny z R ≥ 180 m odpowiada progowi P2 dla torów bocznych (tab. 9.2);
  - przy torach głównych zasadniczych warto podnieść minimum do P1 = 300 m albo dać wybór progu P0, P1 lub P2;
  - wstawka między łukami odwrotnymi wg tab. 9.4 i wzoru 9.3.
- **Sprawdzanie układu torowego (nowe ostrzeżenia):**
  - rozstaw przy łączeniu torów równoległych krótszy niż w tab. 11.2 dla danego rozjazdu;
  - odcinki pośrednie między rozjazdami, które nie są wielokrotnością 0,05 m albo są za krótkie (6 m, a przy V ≥ 160 km/h 30 m);
  - przechyłka ponad limit tab. 9.6 dla rodzaju toru, a w łukach R < 300 m warunek (R − 50)/1,5;
  - niedomiar przechyłki I ponad tab. 9.7, z prędkości toru.
- **Narzędzie profilu:**
  - limity pochyleń z tab. 10.1 według typu linii;
  - łuk pionowy liczony automatycznie z tab. 10.2, gdy różnica pochyleń przekracza 2 mm/m;
  - ostrzeżenie o załomie na rampie, w połączeniu albo na przejeździe;
  - zachowanie odległości 6 m od ramp i przejazdów.
- **Perony:** krawędź 1,30 m od osi toru (w łukach więcej), wysokość 550 lub 760 mm. Ostrzeżenie o peronie na łuku R < 500 m albo na pochyleniu powyżej 6‰.
- **Długości torów stacyjnych:** podpowiedź długości rzeczywistej z tab. 12.9 według typu linii.
- **Poszerzenie toru i prowadnice w ciasnych łukach** (§ 22 i § 24) przy rysowaniu szyn w łukach R ≤ 250 m.
