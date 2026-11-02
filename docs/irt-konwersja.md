# Jak zasysamy i poprawiamy dane: IRT → MaSzyna

Opisuje, jak sceneria Dolnego Śląska przechodzi od surowych danych do gotowej scenerii, i co po drodze poprawiamy. Narzędzia są w `tools/irt/` (konwerter) i `tools/plk/` (audyt PLK). Normy, na które się powołujemy, są w `docs/plk-normy.md`, a literatura w `docs/literatura-kolejowa.md`.

Kolejne kroki:

1. Dane wejściowe (`irt2maszyna.py`)
2. Plan, czyli proste, przejściówki i łuki (`irt2maszyna.py`)
3. Tor 2, rozjazdy i dowiązania (`uklad.py`)
4. Profil podłużny z NMT (`profil.py`)
5. Teren z nasypami i przekopami (`teren.py`)
6. Kontrole (`check_*.py`)
7. Poprawki PLK na gotowych torach (`tools/plk/plk_audit.py`)
8. Kawałki terenu pod torowiskiem do teksturowania (`kawalki_terenu.py`)

Na końcu jest sekcja o tym, co w silniku i edytorze obsługuje te dane.

Układ współrzędnych:
- Dane wejściowe są w EPSG:2180 (E, N).
- MaSzyna ma X na zachód, Z na północ i Y w górę. Przeliczenie: X = −(E − 305000), Z = N − 353000.
- `roll` toru jest w stopniach; wartość dodatnia podnosi lewy tok.

## 1. Dane wejściowe

Dane wejściowe to osie linii kolejowych z warstwy IRT Geoportalu Dolnego Śląska (GeoJSON, EPSG:2180). Każda linia ma numer i kilometraż.

Surowe dane są poszarpane, dlatego najpierw je czyścimy:
- Łamaną upraszczamy algorytmem Douglasa–Peuckera.
- Z wyniku wyznaczamy wierzchołki wieloboku (PI).
- Kawałki z rozgałęzień scalamy po numerze linii.

## 2. Plan według ST-T1-A6 (rozdz. 9)

Dla każdego wierzchołka wieloboku powstaje łuk kołowy z dwiema krzywymi przejściowymi. Krzywa przejściowa to klotoida przybliżona parabolą 3. stopnia, z rampą prostoliniową.

**Promień łuku.** Dopasowujemy go do surowych punktów, wybierając jak największy (9.2 pkt 1). Nigdy nie schodzi poniżej Rmin P2 = 180 m (tab. 9.2).

**Przechyłka D.**
- Mieści się w przedziale ⟨Dreg; Deq⟩ (9.3 pkt 12b).
- Dmax bierzemy z tab. 9.6 i wzoru 9.8, a minimum to 20 mm.
- Niedomiar spełnia I ≤ Idop P1 (tab. 9.7).

**Prędkość.** Jeśli łuk nie pozwala na prędkość linii, wprowadzamy lokalne ograniczenie V = √((D + Idop)·R / 11,8) (wzór 9.1), zaokrąglone w dół do 10 km/h.

**Długość krzywej przejściowej.** Bierzemy ją z tab. 9.10, najpierw dla P0, potem P1 i P2. Ograniczają ją: dD/ds (tab. 9.12), dD/dt (9.13), dI/dt (9.14), minimum 30 m oraz √(qR·R).

**Proste między łukami.** Zachowujemy minimalne proste z tab. 9.1 (P1, a gdy brakuje miejsca, P2). Łuki zgodnie skierowane, które się nie mieszczą, scalamy w jeden.

**Raport.** `raport_lukow.csv` zawiera każdy łuk z jego R, D, I, długościami przejściówek i prędkością.

## 3. Tor 2, rozjazdy, dowiązania (`uklad.py`)

**Tor 2.** Leży po lewej w kierunku kilometrażu, w odstępie e = 4,00 m. Odstęp jest wielokrotnością 0,05 m (9.13). Na łukach tor 2 dostaje własny promień. Tor 1 leży na osi z danych.

**Rozjazdy.** Bierzemy je z katalogu Rz, tak jak `editor/editorSwitchDesign.cpp` (R, 1:n, a, b):
- Tor zwrotny to łuk R od początku rozjazdu i prosta do końca.
- Całość zapisujemy jedną krzywą Béziera, z uchwytami na 2/3 odległości do przecięcia stycznych (jak tryb „whole” w edytorze).
- Wpis to `track switch`, z ujemną prędkością, która ogranicza tylko jazdę w bok.
- Obok jest rekord `//$r switch …` w formacie `editor/editorSwitchStore.cpp`, dzięki któremu edytor rozpoznaje rozjazd jako katalogowy.

**Dowiązania między liniami.** W węzłach końce linii są doprowadzane do toru, do którego się dowiązują. Łączniki bez przechyłki mają prędkość z warunku I ≤ 130 mm.

**Skrzyżowania bezkolizyjne.** Gdy jedna linia przechodzi nad drugą o co najmniej 2,5 m, górny tor dostaje środowisko `bridge`. Obejmuje ono całą długość, na której jego nasyp zasypałby dolną linię. Takie miejsca trafiają do `raport_ukladu.csv` jako `wiadukt_skrzyzowanie`.

## 4. Profil podłużny (`profil.py`, A6 rozdz. 10)

**Źródło wysokości.** Wysokości pochodzą z NMT GUGiK przez usługę WCS, tak jak w `editor/editorNmt.cpp`. Pobieramy kwadraty 500 m z rozdzielczością 2 m i trzymamy je na dysku. Zamiast usługi można podać katalog z arkuszami `.asc` lub `.tif`.

Niweleta to uproszczona wersja `profile::fit_ground` z edytora:
- Teren próbkujemy wzdłuż osi toru 1 co 10 m.
- **Mosty, wiadukty i tunele.** Próbki odstające od linii o więcej niż `--bridge-tol` odrzucamy. Takie miejsca dostają środowisko `bridge` lub `tunnel`, a teren pod nimi zostaje naturalny.
- Załamania niwelety (PVI) wyznaczamy Douglasem–Peuckerem z tolerancją `--grade-tol`.
- Pochylenie wynosi najwyżej 17,5 ‰ (tab. 10.1, P1); większe dają ostrzeżenie. Najkrótszy odcinek stały to 0,4·Vmax, ale nie mniej niż 20 m.
- Łuki pionowe stawiamy, gdy zmiana pochylenia przekracza 2 ‰. Promień R ≥ max(0,35·V²; 5000 m), a gdy się nie mieści, P2: max(0,25·V²; 2000 m).
- Końce linii w węzłach dostają wysokość linii, do której się dowiązują.

## 5. Teren (`teren.py`)

Teren to heightmapa MaSzyny: folder `terrain/irt/` z `terrain.yaml` i paczkami `.tch` (chunki 250 m, paczki 16×16 chunków). Format jest taki jak w `scene/heightmapterrain.*`.

**Źródło i wygładzanie.**
- NMT 10 m w pasie około 230 m z każdej strony torów.
- Braki uzupełniamy najbliższą wartością.
- Wygładzamy filtrem Gaussa (σ = 0,8 oczka) i próbkujemy splajnem 3. stopnia.
- Siatka ma 2 m przy torach i 5 m dalej.

**Torowisko w terenie.** Działa jak modyfikator `formation` z edytora (`modifier_shape::evaluate`). Każda linia ma w `terrain.yaml` modyfikator `tor_NNNNN` z punktami `[x, z, formacja, półszerokość]`, więc edytor przelicza go sam:
- Formacja leży 0,2 m pod główką szyny (spód podsypki).
- Półszerokość 3,52 m = stopa podsypki 2,32 m (0,5·1,435 + 0,5 + 1,1) plus zapas na siatkę 2 m.
- Skarpy 1:1,5, zasięg 60 m, zaokrąglenie krawędzi 1,5 m.
- Na mostach i w tunelach terenu nie ruszamy.

**Wycinki w paczkach.**
- `HGHT` to teren z modyfikatorami.
- `HBAS` to sam NMT. W v4 go nie ma, a `kawalki_terenu.py` radzi sobie bez niego.
- `GRAS` to trawa 3D (gałąź Hirka): `meadow` od 5 m od osi i `weeds` w pasie 3,2–6,5 m. Na podsypce trawy nie ma.

**Malowanie.** Warstwa materiału to `grass`, czyli pierwsza warstwa palety edytora.

**Podsypka.** Każdy tor ma `vis rail_screw_used1 6 1435mm/tpbps-new2 0.2 0.5 1.1`, czyli domyślny styl toru z edytora.

## 6. Kontrole

| Narzędzie | Co sprawdza |
|---|---|
| `check_inc.py` | Składnię jak `TTrack::Load` oraz ciągłość położenia, kierunku i przechyłki między kolejnymi torami. |
| `check_a6.py` | Niezależną kontrolę A6 tylko na podstawie `.inc`: R, D, dD/ds, niedomiar, proste między łukami. |
| `check_teren.py` | Czyta paczki `.tch` jak `heightmap::pack_file::read`: nagłówek, indeks, CRC32, wysokości. Sprawdza też, czy teren leży 0,2 m pod torem. |

## 7. Poprawki PLK na gotowych torach (`tools/plk/plk_audit.py`)

Audyt czyta gotowy `.inc` i sprawdza go względem A6 oraz rozporządzenia Dz.U. 2026 poz. 1225. Z opcją `--napraw` sam poprawia błędy.

**Grupowanie.**
- Kolejne tory składa w elementy: prostą, przejściówkę albo łuk.
- Szum w grupowaniu wycina z tolerancją.
- Łuki koszowe traktuje jako jeden układ.

**Naprawy.**
- Przechyłkę przeprojektowuje w granicach ⟨Dreg; Deq⟩, z rampą w przejściówkach. Nie robi skoków na końcach grup.
- Gdy łuk albo rampa nie pozwala na prędkość, obniża ją w oknie obejmującym cały łuk z przejściówkami.

**Kozły oporowe.** Przed kozłem wymaga prostej co najmniej `plk::buffer_straight_min`. Ta sama reguła jest w Magic (`editor/editorNorms.hpp`).

**Wynik dla naszej scenerii.**
- Przechyłka przeprojektowana w 183 łukach.
- Prędkość obniżona na 531 z 3336 km toru.
- Czas przejazdu całej sieci wzrósł o około 9%.

## 8. Kawałki terenu pod torowiskiem (`tools/irt/kawalki_terenu.py`)

Teren pod torami i przy nich jest wycinany na osobne powierzchnie, każda nie dłuższa niż 100 m. Dzięki temu każdą można osobno oteksturować w edytorze (lista „Powierzchnie wycięte z terenu”).

**Torowisko.**
- Pas o półszerokości 3,52 m od osi, czyli tej samej, na której modyfikator formuje teren.
- Na wielu torach pas między osiami dzielimy w połowie odstępu. Dzięki temu każdy tor ma swoje kawałki, a kawałki sąsiednich torów nie nachodzą na siebie.

**Skarpy.**
- Osobno nasyp i przekop, osobno lewa (L) i prawa (P) strona.
- Skarpa sięga od krawędzi torowiska do miejsca, gdzie skarpa 1:1,5 spotyka teren naturalny.
- Rozpoznajemy ją po kształcie: wysokość ≈ formacja ∓ odległość/1,5, z tolerancją 0,15 m. Gdy paczka ma `HBAS`, skarpę rozpoznajemy po różnicy `HGHT − HBAS`.
- Na koniec wygładzamy: krótkie dziury łatamy, krótkie wyspy usuwamy, a koniec skarpy uśredniamy medianą.

**Nazwy.** Kawałki nazywamy tak: `L132_2 torowisko km 012.300-012.400`, `L132_2 nasyp L km …`, `… przekop P km …`.

**Nasypy.** Nasypy i przekopy kształtuje modyfikator edytora (pkt 5). Kawałki ich nie zmieniają, tylko oddzielają teksturowanie.

**Zapis.**
- Dziury w terenie trafiają do wycinka `HOLE` w paczkach: 1 m siatki, z zapasem 1 m, kopiowane do sąsiadów na granicach chunków.
- Powierzchnie trafiają do pliku `patches.txt` obok `terrain.yaml` (`patches_file: patches.txt`).

**Wynik dla naszej scenerii:** 78 827 kawałków, z tego 33 375 torowiska, 23 046 nasypu i 22 406 przekopu. Dziury są w 10 738 chunkach.

Uruchomienie:

```
python3 tools/irt/kawalki_terenu.py terrain/irt wynik --tory scenery/irt_tory.inc --dlugosc 100
```

## Co w silniku i edytorze obsługuje te dane

**Powierzchnie „follow”** (`scene/heightmapterrain.*`).
- Taka powierzchnia nie trzyma własnych wysokości, bierze je z terenu, podniesione o `lift`.
- W pliku YAML: `follow: true`, `spacing`.
- Narzędzie wycinania w edytorze tworzy teraz właśnie takie powierzchnie.

**Plik obok terenu.**
- Przy ponad 200 powierzchniach albo gdy jest ustawione `patches_file`, powierzchnie „follow” zapisują się do pliku tekstowego.
- Jedna linia na powierzchnię, pola oddzielone tabulatorami: nazwa, materiał, rozmiar, obrót, lift, spacing, obrys.

**Strumieniowanie.** Powierzchnie budują się tylko w promieniu 1,5 km od kamery i są przebudowywane, gdy kamera przesunie się o 200 m. Powierzchnia, pod którą teren jeszcze się nie wczytał, czeka na niego.

**Lista w edytorze.**
- Filtr po słowach w nazwie, np. `L132 nasyp`.
- „Materiał z palety dla wszystkich pokazanych”, z cofaniem przez Ctrl+Z.
- Przycisk „Idź do”.
