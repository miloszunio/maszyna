Gałąź `alfa-test_candidate` to **kandydat do testów alfa** z 22.11: całe `ficzery-rc`, aktualny `master` i wszystko, co powstało na
naszych gałęziach po 20.11. Jedna wersja ze wszystkim naraz.

## Co jest w środku

- **Całe `ficzery-rc`**: `ficzery2`, renderery OpenGL 4.6 i Vulkan, pogoda, nowy wygląd edytora, `staging` z 20.11. Lista jest w
  `docs/ficzery-rc.md`.
- **`master` z 24.11, 19:44** (`57148761`), 0 commitów za nim. Względem `ficzery-rc` doszło dziesięć merge z 21, 22 i 24.11:
  - `RandomInt()` dla losowania liczb całkowitych, `Random()` z całkowitymi granicami znów daje liczbę rzeczywistą (#416);
  - ortofotomapa tylko na chunkach terenu w pamięci (#417, w tej linii była już w ostatecznej postaci);
  - zamykanie edytora nie czeka na usługę geoportalu, która nie odpowiada (#432);
  - zmętnienie nieba jak przed #227 (#410);
  - AI przy ruszaniu pod górę trzyma moc, dopóki hamulec pociągu nie odpuści (#396);
  - rozmycie ruchu nie rozmazuje dołu ekranu w jasny pas przy szybkim ruchu kamery (#441): do tej linii weszła tylko część w
    shaderze, w OpenGL 4.6 i w Vulkanie. Część w rendererze OpenGL 3.3 (rozmycie od ruchu samej wolnej kamery) nie ma tu
    odpowiednika, bo tego renderera w tej linii nie ma.
  - AI rusza stojącym pociągiem dopiero z naładowanymi hamulcami, a na czas ładowania trzyma go hamulcem lokomotywy (#444).
    Maszynista dostaje to samo czekanie jako podpowiedź.
  - ekrany Pythona znajdują na Linuksie pliki, które same otwierają, także przy innej wielkości liter (#449);
  - niebo, światło otoczenia i mgła z 26.05 w starym rendererze (#415) oraz wygląd starych zasobów jak w 26.05 i szerszy snop
    reflektorów, `gfx.headlights.beamwidth` (#481): oba są w historii, ale nic z nich tu nie działa. Dotyczą renderera OpenGL 3.3 i
    jego shaderów w `shaders/`, których w tej linii nie ma; shadery OpenGL 4.6 i Vulkana (`shaders/ogl46`, `shaders/vk14`) zostały
    bez zmian, także tonemapping AgX. Klucz `gfx.headlights.beamwidth` nic tu nie robi.
- **Ogrzewanie rozjazdów (EOR)** z `feat/infrastructure-impact`: grzałki topią śnieg przy iglicach, a śnieg albo lód trzyma iglice
  rozjazdu, któremu sceneria wyłączyła ogrzewanie. Opis w `docs/switch-heating.md`.
- **Dźwięk wraca po utracie albo zmianie urządzenia wyjściowego** (PR #433), połączone z opcjami dźwięku ze `staging` (HRTF, tryb
  wyjścia, nazwa urządzenia z `eu07.ini` czytana też jako tekst w stronie kodowej 1250).
- **Poprawki pogody z 21.11**: pobieranie pogody na żywo na własnym wątku zamiast `std::async`, generator trzasków łuku i liczniki
  pętli w teście klimatu uporządkowane pod Sonara, `tools/weather/imgw_normals.py` trzyma ścieżki w swoim folderze.
- **`weather-schema.txt` obok gry**: klucze, wartości domyślne, zakresy i grupy `physics.weather.*` tej wersji, dla startera. Gra tego
  pliku nie czyta.

## Czego tu nie ma

- **Cofnięcia #408 ze `staging` z 22.11** (#443). `staging` z 20.11 jest tu w całości, razem z #408, więc log fizyki ma tu obsługę
  wolnego dysku, którą `staging` 22.11 zdjął do zrobienia od nowa na #385. To jedyna różnica względem dzisiejszego `staging`.
- **Pojazdów grafowych** (`feat/vhg`), które były w starej linii `ficzery`. Linia `ficzery2` ich nie ma.
- Wersji pól (#315) i listy wypełnionych obszarów (#241) w kształcie, w jakim weszły do `feat/gl46-migration`: są tu nasze, nowsze
  wersje tych narzędzi, jak w `ficzery-rc`.

## Co zostało połączone ręcznie

- **Dźwięk.** `staging` i #433 zmieniały to samo miejsce. Zostało jedno i drugie: urządzenie z `eu07.ini`, którego nie ma przy
  starcie, zostaje zapamiętane (wcześniej nazwa była czyszczona), gra idzie na domyślnym wyjściu i wraca na tamto, gdy się pojawi.
  Zmiana urządzenia w ustawieniach dźwięku w grze ustawia śledzenie wyjścia od nowa.
- **`RandomInt()`** losuje z lokalnego generatora, jak na `master`; `Random()` zostaje przy generatorze wątku z tej linii.
- **`physics.weather.switches`** ma w `weather-schema.txt` własną grupę `switches`.

## Na co patrzeć przy testach

- **Dźwięk**: wyjąć i włożyć słuchawki w trakcie jazdy, zmienić domyślne wyjście w systemie, zmienić urządzenie i HRTF w
  ustawieniach dźwięku; osobno start z urządzeniem wpisanym w `sound.openal.renderer`, którego nie ma.
- **Rozjazdy zimą**: sceneria ze śniegiem i mrozem, rozjazd z ogrzewaniem i bez. Wyłącznik: `physics.weather.switches no`.
- **Ortofotomapa w edytorze** po poprawce #417 i zamykanie edytora bez sieci (#432).
- **Niebo** po cofnięciu zmętnienia (#410): porównać z `ficzery-rc`.
- **AI pod górę** (#396): pociąg prowadzony przez AI, ruszający na wzniesieniu.
- **Ruszanie AI z zimnego składu** (#444): ciężki pociąg postawiony z pustym przewodem, także na pochyleniu. Ma stać, dopóki
  hamulce się nie naładują, i nie staczać się w tym czasie.
- **Rozmycie ruchu** z wolną kamerą nisko nad ziemią, w OpenGL i w Vulkanie: bez jasnego pasa u dołu ekranu.
- Wszystko z listy w `docs/ficzery-rc.md`, w tym śnieg w Vulkanie.
