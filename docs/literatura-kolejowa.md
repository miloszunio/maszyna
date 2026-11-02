# Literatura kolejowa: wyciąg do edytora i konwertera

Notatki z czterech książek z listy literatury do „Projektowania linii kolejowych” (PUT). Są tu własnymi słowami spisane zasady i liczby, które przydają się w edytorze, w konwerterze IRT i w `tools/plk/plk_audit.py`. Obowiązujące są dziś ST-T1-A6 i rozporządzenie z 17.09.2026 (zob. `docs/plk-normy.md`). Książki dają tło, metody i zasady, których normy nie podają, a starsze przepisy cytowane w książkach oznaczono jako dawne.

| Książka | Wydanie | Co z niej bierzemy |
|---|---|---|
| H. Bałuch, M. Bałuch, *Układy geometryczne toru i ich deformacje* | PLK, 2010 | krzywe przejściowe, przesunięcia przy wydłużaniu krzywych, układy stacyjne, profil |
| A. Massel, *Projektowanie linii i stacji kolejowych* | PLK, 2010 | zakleszczenie zderzaków, nagła zmiana niedomiaru wg EN 13803-2, numeracja torów, stacje |
| S. J. Cieślakowski, *Stacje kolejowe* | WKiŁ, 1992 | układy małych stacji, rozmieszczenie semaforów |
| J. Sysak (red.), *Drogi kolejowe* | PWN, 1986 | przekroje podtorza (torowisko, ławy, skarpy), dawne zasady układania rozjazdów |

## 1. Plan: łuki, krzywe przejściowe, łuki odwrotne

**Postać krzywej przejściowej** (Bałuch, rozdz. 3)
- Najczęściej stosuje się parabolę 3. stopnia z rampą prostoliniową. Jest to pierwszy wyraz rozwinięcia klotoidy.
- W klotoidzie krzywizna rośnie liniowo z długością: L·R = A² w każdym punkcie.
- Krzywe z rampą krzywoliniową to parabola 4. stopnia, cosinusoida, sinusoida i krzywa Blossa. Najlepszą dynamikę daje sinusoida, a parabola 3. stopnia wypada pod tym względem najgorzej.
- Rampa paraboli 3. stopnia nazywa się rampą Blossa, rampa sinusoidalna rampą Kleina.
- ST-T1-A6 dopuszcza rampy krzywoliniowe tylko jako próg P2.

**Przesunięcie łuku do środka** przy wstawieniu krzywej przejściowej długości L to około n = L²/(24R).
- Ze względu na tyczenie n ma wynosić co najmniej 0,02 m (to samo jest we wzorze 9.14 ST-T1-A6).
- Przy dużych R wydłużenie krzywej kosztuje bardzo mało przesunięcia. Przykład: L = 36 m przy R = 3000 m to tylko około 2 cm.

**Wydłużanie krzywych przejściowych przy modernizacji** (Bałuch, rozdz. 6)
- W praktyce najczęściej trzeba wydłużać krzywe przejściowe. Rzadziej jednocześnie zwiększa się R, a sam wzrost R przy tych samych krzywych zdarza się najrzadziej.
- Przesunięcia toru liczy się w strefach. Strefy zależą od tego, czy nowa krzywa kończy się przed starą, za nią, czy na niej.
- Pole przesunięć, czyli powierzchnia między starą a nową osią, mówi, ile robót ziemnych trzeba i czy tor wyjdzie poza granice działki.
- Przykład z książki: łuk R 950 m z krzywymi 90 m przy 120 km/h przebudowany na 160 km/h dostaje R 1450 m i krzywe po 140 m.
- **Wniosek dla konwertera IRT.** Krótkie krzywe przejściowe na liniach 120–160 km/h, które teraz trzeba łatać obniżeniem prędkości, da się wydłużyć kosztem centymetrów do decymetrów przesunięcia. Mieści się to w tolerancji dopasowania do osi z IRT. Ograniczeniem jest miejsce między sąsiednimi łukami, a nie samo przesunięcie.

**Zakleszczenie zderzaków na łukach odwrotnych** (Massel 7.1.2, ST-T1-A6 pkt 9.2.9–10, EN 13803-2)
- Promień zastępczy to R_z = R1·R2/(R1 + R2).
- Jeśli R_z < 110 m, między łukami musi być prosta:

| R_z | Najkrótsza prosta |
|---|---|
| ≥ 110 m | 0 |
| 100 m | 6,0 m |
| 95 m | 7,0 m |
| 92,5 m | 7,5 m |
| 90 m | 8,0 m |
| 85 m | 9,0 m |
| 80 m | 10,2 m |
| 75 m | 11,5 m |

  W wierszach dla 95 i 92,5 m wartości odczytano z OCR; pasują do ciągu.
- Przykład: dwa łuki odwrotne R 180 m dają R_z = 90 m, więc potrzeba 8 m prostej.
- **Wdrożone:** `plk::buffer_straight_min` używają Magic i `plk_audit.py` (kontrola „zderzaki”).

**Nagła zmiana niedomiaru** (Massel 7.1.9, EN 13803-2)
- Sprawdza się ją w każdym miejscu, gdzie krzywizna zmienia się skokiem: łuki koszowe bez krzywej przejściowej, łuki o małym kącie bez krzywych, rampa leżąca na prostej lub na łuku.
- Element przejściowy między dwoma skokami liczy się osobno, gdy ma co najmniej:

| V | Zalecane | Minimum |
|---|---|---|
| < 70 km/h | 0,20·V | 0,10·V |
| 70–100 km/h | 0,25·V | 0,15·V |
| 100–200 km/h | 0,30·V | 0,20·V |

  Wartości minimalne odpowiadają tab. 9.16 ST-T1-A6.

**Długości prostych i łuków** (Massel 7.1.8)
- Dawne warunki techniczne dla torów głównych linii magistralnych:
  - łuk ≥ V/2,5, nie mniej niż 30 m;
  - prosta między łukami ≥ V/1,8, w trudnych warunkach V/2,5.
- Norma EN 13803 jest łagodniejsza. ST-T1-A6 (tab. 9.1) idzie w tę stronę.

**Przechyłka** (Massel 7.1.3, Bałuch rozdz. 3)
- Przechyłkę dobiera się tak, żeby jak najlepiej zrównoważyć siły dla rzeczywistych prędkości wszystkich rodzajów pociągów.
- Najmniejsza zastosowana przechyłka to 20 mm.
- Przy peronach przechyłka wynosi najwyżej 110 mm (TSI).
- W łukach R < 300 m przechyłka D ≤ (R − 50)/1,5.

## 2. Profil

**Pochylenie miarodajne** (Bałuch 9.1) to pochylenie rzeczywiste powiększone o opór łuków na danym odcinku.
- Mierzy się je na długości co najmniej najcięższego pociągu towarowego.
- Dawne limity:
  - linie magistralne i pierwszorzędne: 6‰;
  - linie znaczenia miejscowego i bocznice: 20‰;
  - w tunelach dłuższych niż 250 m mniej.

**Dawne zasady łączenia pochyleń** (Bałuch 9.1)
- Różnica sąsiednich pochyleń na liniach magistralnych i pierwszorzędnych: najwyżej 5‰.
- Pochylenia przeciwne, większe niż 2,5‰, rozdziela się wstawką o długości liczonej od długości pociągu.
- Dla porównania na kolejach dużych prędkości stosuje się do 25–35‰.

**Zaokrąglenie załomu** (Bałuch 9.2)
- Załom trzeba zaokrąglić, gdy strzałka z między wierzchołkiem a łukiem przekracza 8 mm. Przy mniejszej ruszt torowy sam się wygnie.
- Dawne promienie łuków pionowych:

| Tory | Promień |
|---|---|
| nowe linie powyżej 160 km/h | 20 000 m |
| 140–160 km/h | 15 000 m |
| linie magistralne i pierwszorzędne | 10 000 m |
| linie drugorzędne | 5000 m |
| główne dodatkowe linii drugorzędnych | 2500 m |
| boczne | 2000 m |

- Dziś obowiązuje tab. 10.2 ST-T1-A6, zwykle co najmniej 5000 m.
- Załomu nie umieszcza się na rampie przechyłkowej. Łuk pionowy ma kończyć się co najmniej 6 m od rampy i od mostu bez podsypki. Przy modernizacji wyjątkowo można, gdy rampa ma do 2‰, a łuk pionowy co najmniej 5000 m.
- Na załomach nie układa się rozjazdów.
- **Wniosek dla konwertera.** Niweleta z NMT ma za dużo krótkich załomów. Raport pokazał 1185 kontroli „Rv” ponad P2. Trzeba ją wygładzić tak, żeby sąsiednie pochylenia różniły się mało, a każdy załom ponad 2‰ dostał łuk pionowy z tab. 10.2 i nie leżał na rampie ani na rozjeździe.

## 3. Przekrój podtorza (do korytarzy terenu w edytorze)

Dawne przekroje normalne, Sysak rozdz. 11:

**Torowisko**
- Na linii jednotorowej ma kształt trapezu z płaską górą o szerokości 2,60 m (2,50 m na liniach drugorzędnych i lokalnych). Krawędzie leżą 7 cm niżej, co daje spadek około 4,5–5,6%; na liniach lokalnych 5 cm.
- Na linii dwutorowej ma kształt daszku o spadku około 1:25, czyli 4%.
- **Ława** (pobocze między stopą podsypki a krawędzią torowiska) ma co najmniej:
  - 55 cm na liniach magistralnych;
  - 35 cm na pierwszo- i drugorzędnych;
  - 30 cm na lokalnych.

  W łuku od zewnątrz może być węższa.
- Przejście do torowiska poszerzonego w łuku robi się na co najmniej 5 m.

**Podsypka i warstwa ochronna**
- Podsypka w torach głównych ma 30 cm na liniach magistralnych, 25 cm na pierwszorzędnych i 20 cm na drugorzędnych; w torach bocznych mniej. Na podkładach betonowych grubość rośnie o 5 cm.
- Pod podsypką na gruntach spoistych układa się warstwę ochronną z piasku: 30 cm na liniach magistralnych i pierwszorzędnych, 20 cm na drugorzędnych, 10 cm w torach bocznych.

**Nasypy**
- Skarpa ma pochylenie 1:1,5 do wysokości 6 m. Poniżej 6 m od krawędzi skarpa jest łagodniejsza, 1:1,75.
- Od wysokości 12 m stosuje się ławy szerokie na co najmniej 1 m, co 6 m wysokości.
- Najniższy nasyp ma 0,6 m. Tyle też torowisko powinno wystawać ponad najwyższą wodę.
- Nasyp wyższy niż 20–25 m (na słabym gruncie już od około 15 m) wychodzi drożej niż wiadukt.

**Przekopy**
- W piaskach i glinach skarpy mają 1:1,5, w skale od 1:1 do 1:0,1.
- Przekopów płytszych niż 1 m się unika, bo zasypuje je śnieg.
- Przekop głębszy niż około 20 m przegrywa z tunelem.
- Pod nasypem i wzdłuż rowów zostawia się ławę 1 m o spadku 4–5%. Dno ukopu ma spadek co najmniej 2%.

**Dawne rozstawy torów na szlaku**
- 4,0 m na liniach nowych, 3,5 m na istniejących.
- 4,75 m z sygnalizatorami na międzytorzu, 4,90 m ze słupami trakcyjnymi.
- Między torami dwóch różnych linii około 1,6 m więcej (4,0 + 1,6 = 5,6 m).
- Od osi skrajnego toru do słupa lub sygnalizatora co najmniej 2,50 m.

## 4. Stacje

**Długości torów** (Bałuch 7.1)
- Rozróżnia się cztery długości:
  - budowlaną: między rozjazdami albo do kozła;
  - ogólną: budowlana razem z rozjazdami;
  - użyteczną: na której mieści się pociąg, od semafora, tarczy lub ukresu;
  - rzeczywistą: użyteczna z dodatkami.
- Tory pasażerskie na nowych liniach sieci TEN projektuje się dla pociągu 300 lub 400 m, poza siecią TEN dla 200 m. Dodaje się do tego:
  - drogę ochronną 50 lub 100 m;
  - odcinek izolowany od ukresu 10 m;
  - zapas na niedokładne zatrzymanie 10–15 m;
  - lokomotywę 20 m.

  Razem dodatki to około 95 m. Zespoły podmiejskie potrzebują 200 m.
- Żeberko ochronne ma co najmniej 50 m.

**Rozstaw torów na stacji** (Bałuch tabl. 7.3, dawne warunki techniczne)

| Tory | Rozstaw |
|---|---|
| główne zasadnicze | ≥ 4,50 m (dopuszczalnie 4,00 m) |
| z semaforami lub tarczami na międzytorzu | ≥ 4,75 m |
| ze słupami trakcyjnymi na międzytorzu | ≥ 4,90 m (zalecane 5,00 m) |
| wyciągowy obok głównego zasadniczego / innego toru | 6,00 / 4,75 m |
| z drogą między torami | szerokość drogi + 2 × 3,0 m |
| do bezpośredniego przeładunku | ≥ 3,50 m |
| pozostałe | ≥ 4,50 m (zalecane 5,00 m) |
| z peronem jednokrawędziowym | ≥ 6,0 m (zalecane 9,0 m) |
| z peronem dwukrawędziowym, dojście w poziomie | ≥ 9,0 m (10–13 m) |
| z peronem dwukrawędziowym, kładka lub tunel | ≥ 10,7 m (12,5–16 m) |

**Numeracja torów i rozjazdów** (Massel 8.3.2)
- Na stacji linii jednotorowej tor główny zasadniczy ma nr 1.
- Na stacji linii dwutorowej:
  - tor 1 służy jeździe w kierunku rosnącego kilometrażu;
  - tor 2 służy jeździe w kierunku malejącego kilometrażu.
- Patrząc w stronę rosnącego kilometrażu, tory na prawo od toru 1 dostają kolejne numery nieparzyste (3, 5, …), a tory na lewo kolejne parzyste (2, 4, …).
- Na dużych stacjach numeruje się grupami, np. 101…, 201…, 301…, i zostawia wolne numery na rozbudowę.
- Rozjazdy numeruje się kolejno w kierunku rosnącego kilometrażu, ewentualnie grupami od pełnych dziesiątek lub setek. Skrzyżowania torów bez zwrotnic nie dostają numerów.
- Stacja powinna leżeć na prostej, jeśli tylko się da.

**Liczba torów i układy małych stacji** (Cieślakowski rozdz. 2)
- Stacja na linii jednotorowej ma co najmniej 2 tory główne; dwie–trzy stacje przed węzłem lepiej 3.
- Stacja na linii dwutorowej ma co najmniej 3 tory główne, lepiej 4 (po dwa na każdy kierunek).
- Dworzec i ładownia po tej samej stronie torów głównych są wygodne przy małym ruchu towarowym. Przy dużym ładownię umieszcza się po przeciwnej stronie, a tor dodatkowy towarowy po stronie ładowni.
- Tory dodatkowe na stacji linii dwutorowej układa się na dwa sposoby:
  - po jednej stronie (układ liniowy): łatwiej obsłużyć pociągi zbiorowe i dojść do peronów, ale pociągi towarowe z jednego kierunku przecinają tor główny;
  - po obu stronach (układ kierunkowy): bez przecinania przy przyjmowaniu pociągów, ale manewry przecinają oba tory główne.
- Tor wyciągowy ma spadek w stronę stacji. Dworzec stoi od strony obsługiwanej miejscowości.

**Semafory** (Cieślakowski rozdz. 3, dawne zasady)
- **Wjazdowy:**
  - stoi przed pierwszą zwrotnicą, po prawej stronie toru;
  - stoi 100 lub 200 m przed punktem niebezpiecznym (początek lub ukres rozjazdu, wskaźnik W5, początek peronu); przy średnim spadku na 1000 m przed nim ponad 5‰ odległość rośnie o 50%, do 150 lub 300 m;
  - do następnego semafora musi zostać co najmniej droga hamowania.
- **Wyjazdowe:**
  - stoją na międzytorzu szerokim co najmniej 4,5 m;
  - za nimi jest droga ochronna 100 albo 50 m, zależnie od prędkości.
- **Odstępy:** między kolejnymi semaforami co najmniej droga hamowania (na PKP 1000 m), najwyżej 1500 m, wyjątkowo 1800 m.

## 5. Rozjazdy w drogach zwrotnicowych (dawne zasady, Sysak 7.4)

Prosta między kolejnymi rozjazdami zależała od tego, jak są do siebie ustawione:
- **W torach głównych zasadniczych** w wielu układach była potrzebna prosta 15 m.
- **W pozostałych torach** prosta wynosiła 6 m albo 0, jeśli rozjazd nie miał poszerzenia toru przy iglicach albo miał iglice styczne.
- **Na przejazd zwrotny:** co najmniej V/6 m (V w km/h), nie mniej niż 6 m.

Dziś obowiązuje pkt 11.3 ST-T1-A6:
- odcinki pośrednie mają długość będącą wielokrotnością 0,05 m;
- rozjazdy ustawione iglicami do siebie mogą stać bez odcinka między nimi (0 m), choć zaleca się co najmniej 6 m;
- przy prędkości co najmniej 160 km/h odcinek ma co najmniej 30 m.

## 6. Do zrobienia na podstawie lektury

1. **Konwerter IRT, plan.** Wydłużyć krzywe przejściowe do długości z tab. 9.10 przy prędkości linii. Robi się to kosztem przesunięcia n = L²/24R w granicach tolerancji dopasowania, a tam, gdzie brakuje miejsca, przez zmianę R sąsiednich łuków (Bałuch rozdz. 6). Wtedy zniknie większość ograniczeń prędkości z `plk_audit.py --napraw`.
2. **Konwerter IRT, profil.** Wygładzić niweletę z NMT: ograniczyć różnicę sąsiednich pochyleń, dać łuki pionowe z tab. 10.2, nie stawiać załomów na rampach i rozjazdach, trzymać dopuszczalne pochylenia (tab. 10.1) poza zjazdami na mosty i w tunele.
3. **Edytor, korytarz terenu.** Domyślne przekroje podtorza wg rozdz. 3: torowisko, ławy, skarpy 1:1,5 i 1:1,75 poniżej 6 m, ławy co 6 m, rowy w przekopach.
4. **Edytor, stacje.** Podpowiadać rozstawy torów z tabeli w rozdz. 4 i numerację torów (nieparzyste na prawo, parzyste na lewo, w kierunku rosnącego kilometrażu). Pokazywać miejsce na semafor wjazdowy (100 lub 200 m przed punktem niebezpiecznym, +50% na spadku).
5. **Magic i `plk_audit.py`:** zakleszczenie zderzaków (wdrożone 2.11.2026).
