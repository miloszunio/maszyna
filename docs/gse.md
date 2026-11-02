# Gałąź `gse`: co w niej jest

`gse` to nasza gałąź edytora (fork `miloszunio/maszyna`). Zbiera to, co dobre z gałęzi Hirka (`feat/gl46-migration`), z `master` MaSzyna-EU07 i z kilku gałęzi tematycznych, a do tego nasze własne poprawki i narzędzia edytora.

Każdy push na `gse` kompiluje się automatycznie na GitHubie. Paczki można pobrać bez konta na GitHubie:

- Windows: https://github.com/miloszunio/maszyna/releases/download/build-gse/maszyna-windows-x64.zip
- Linux: https://github.com/miloszunio/maszyna/releases/download/build-gse/maszyna-linux-x64.tar.gz

Paczkę rozpakowuje się na istniejącą instalację MaSzyny. Danych gry w niej nie ma.

## Skąd jest kod

Stan na 2 listopada 2026: w `gse` są scalone wszystkie bieżące gałęzie Hirka i maj00ra z repozytorium MaSzyna-EU07.

| Źródło | Co daje |
|---|---|
| `feat/gl46-migration` (Hirek) | renderer OpenGL 4.6 i edytor w ImGui; `gse` stoi na tej gałęzi. Od 1.11 także małe raporty z awarii (crash dump), sprawdzanie procesora przy starcie i `eu07_starter` |
| `master` (MaSzyna-EU07) | bieżące poprawki symulatora, m.in. radio FM, pompa paliwa i budzenie fizyki pojazdów przez przewody |
| `feat/vhg` (Hirek) | pojazdy opisane grafem (`.vhg`): bloki logiki i kabiny, `-graph-run`, `-graph-catalog`, kurki przewodów na sprzęgach |
| `feat/switch-blades` (maj00r) | iglice rozjazdów oraz całe późniejsze prace maj00ra nad torami i edytorem |
| `fix/imgui-panels-no-auto-detach` | panele ImGui nie odklejają się same od głównego okna |
| `feat/track-uneven` (maj00r) | nierówności toru i opis drgań kabiny |
| `fix/cab-lighting` (maj00r), część | walidator materiałów (`tools/materials`) i krzywa tonów AgX, która nie zaczernia ciemnych części kabiny |

Scalone, ale bez zmian w kodzie, bo ich zawartość już tu jest w późniejszej postaci:
- `feat/track-join` i `feat/rail-crossings` maj00ra weszły wcześniej przez `switch-blades`;
- `feat/editor-filling`, `feat/improve-loading-times`, `feat/mini-generation` i `feat/dynobj-seasonal-models` Hirka weszły wcześniej przez `gl46-migration`.

Celowo pominięte:
- z `fix/cab-lighting`: ambient occlusion (GTAO), parallax i zmiany oświetlenia. Są napisane dla starego renderera OpenGL 3.3, którego w gl46 już nie ma, i trzeba by je przenieść ręcznie;
- refaktor `track-uneven` pod Sonara, który nie pasuje do segmentów z gl46 i nie zmienia działania;
- `feat/avx2-support`, `fix/default-shader-reflections`, `fix/reflections-and-burned-colors` i `staging`: stara linia sprzed gl46.

## Nasze zmiany w edytorze

### Kursor na dużych współrzędnych (PR #240, zgłoszenie #238)
Na scenerii daleko od punktu 0, np. na naszych wrocławskich współrzędnych, klik trafiał kawałek obok wskazanego miejsca. Przyczyny były dwie:
- głębia pod kursorem była odczytywana z przesunięciem,
- kursor był liczony z opóźnieniem o klatkę, gdy kamera się ruszała.

Obie są poprawione. Teraz punkt ląduje tam, gdzie się kliknie, także przy przesuwaniu kamery w trakcie rysowania długiej drogi.

### Wypełnianie obszaru (PR #241)
- **Enter** kończy obszar i zaczyna następny, zamiast dokładać punkty do poprzedniego.
- **Esc** porzuca rysowany obrys.
- Wypełnione obszary są na liście pod narzędziem. Można je później wybrać, zmienić im model lub gęstość albo usunąć.
- W pliku scenerii obszary zapisują się jako linie `//$w`. Wcześniej było to `//$f`, ale tego oznaczenia używa już narzędzie profilu dla dopasowanych torów.

### Rozjazdy: opcja „Magic”
Jest w narzędziu rozjazdów, przy rozjeździe pojedynczym. Działa wtedy, gdy rozjazd ciągnięty do istniejącego toru nie daje się z nim połączyć tak, jak tor leży: jest za ciasno, koniec toru jest w łuku albo jest za krótki. Edytor wtedy sam:
1. skraca tor za końcem zwrotnym (nie za kierunkiem na wprost), od jego wolnego końca, tak daleko jak trzeba, maksymalnie 400 m;
2. układa go od nowa wg zasad PLK (ST-T1-A6), na jeden z trzech sposobów:
   - prostą,
   - łukiem rozjazdu,
   - łukiem odwrotnym o promieniu co najmniej 180 m z prostą wstawką między łukami.

Najpierw próbowany jest wybrany wzorzec rozjazdu, potem pozostałe dla tej samej szyny, od 1:9 w górę. Ctrl+Z cofa całość jednym krokiem. Na pasku stanu widać, który rozjazd wybrano i o ile skrócono tor.

### Teren: wycinanie osobnej powierzchni
Jest w panelu Teren, w sekcji Wypełnienie, jako przycisk **„Wytnij jako osobną powierzchnię”**. Służy do kawałków terenu, które mają mieć inną teksturę niż reszta wygenerowanego terenu, np. plac, peron, rampa albo pole. Jak to działa:
1. Obrys rysuje się tak samo jak przy wypełnianiu.
2. Edytor zapamiętuje wysokości terenu w obrysie, co 1 m, a przy dużych obszarach rzadziej.
3. W terenie powstaje w tym miejscu dziura.
4. W dziurę wchodzi osobny obiekt o tych samych wysokościach, z materiałem wybranym w palecie i z obrotem tekstury ustawionym suwakiem.

Wycięte powierzchnie są na liście pod przyciskiem. Dla każdej można:
- zmienić materiał, przeciągając teksturę albo biorąc ją z palety;
- zmienić powtarzanie, obrót tekstury i uniesienie nad teren;
- przywrócić jej obrys do narzędzia;
- „oddać terenowi”, czyli usunąć powierzchnię i zamknąć dziurę.

Powierzchnie zapisują się w `terrain.yaml` w sekcji `patches`. Ctrl+Z działa jak dla reszty terenu.

### Minimapa z wyszukiwarką miejscowości
Jest w menu **Mapa → Minimapa**.
- Pokazuje tory scenerii z góry, z północą u góry, i kamerę jako strzałkę w kierunku patrzenia. W tle jest siatka kilometrowa.
- Miasta widać zawsze, wsie po przybliżeniu.
- Kółko myszy przybliża, przeciąganie przesuwa mapę, a dwuklik przenosi tam kamerę. Opcja „Podążaj za kamerą” trzyma mapę na kamerze.
- Po najechaniu myszą widać współrzędne i najbliższą miejscowość.
- Wyszukiwarka zna 3337 miast i wsi z prostokąta opisującego sieć IRT Dolnego Śląska. To dolnośląskie oraz kawałki opolskiego, wielkopolskiego i lubuskiego. Polskie znaki nie mają znaczenia, więc „zabkowice” znajdzie Ząbkowice Śląskie. Klik pokazuje miejscowość na mapie, a dwuklik lub Enter przenosi tam kamerę.
- Miejscowości są rozmieszczane według początku scenerii w układzie PUWG 1992. Ten początek bierze się z ustawień ortofotomapy albo z linii `//$g` w scenerii. Jeśli go brak, okno proponuje początek sieci IRT (E 305000, N 353000).
- Dane miejscowości pochodzą z Państwowego Rejestru Nazw Geograficznych, przez pakiet `polskie-miejscowosci` (licencja CC0), według stanu z 2021 r.
