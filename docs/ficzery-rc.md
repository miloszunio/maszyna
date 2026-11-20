Gałąź `ficzery-rc` to **wydanie kandydujące do testów**: jedna wersja ze wszystkim naraz, żeby nie testować po kawałku.

## Co jest w środku

- **Cały upstream z 20.11**: `master` (`6c94f974`) i `staging` (`2f127688`). Oba są wcielone w całości, 0 commitów za każdym z nich.
  `master` i `staging` rozeszły się 13.11 i każdy ma rzeczy, których nie ma drugi, więc wzięte są oba.
- **Renderery OpenGL 4.6 i Vulkan** z `feat/gl46-migration`, razem z latarniami oświetlającymi otoczenie (#371).
- **Całe `ficzery2`**: lista jest w `docs/ficzery2.md`.
- **Pogoda** (`FullWeatherModel`, PR #431): śnieg leżący i zaspy, presety klimatyczne, pogoda na żywo z Open-Meteo, pogoda w fizyce
  pojazdów, oblodzenie i wiatr na sieci trakcyjnej.
- **Nowy wygląd edytora** (`feat/editor-ui-refresh`, PR #430): jasna i ciemna paleta, okna jako wyspy, opisy narzędzi.

## Co doszło względem `all-to-master` z 20.11

- **Pogoda na żywo działa.** Odpowiedź Open-Meteo była odrzucana jako „nie-JSON”, przez co tryb `live` nie dostawał danych, a build
  na Linuksie nie przechodził testów.
- **Śnieg i mokre nawierzchnie w rendererze Vulkan**, tak jak w OpenGL 4.6.
- **Poprawki z naszych PR-ów, których nie było w linii `ficzery2`** (powstały na gałęziach PR-ów po uwagach z review):
  - profil podłużny drogi (#311): okno zapisuje tylko zmienione odcinki, węzły przesuwane razem i końce trzymają spadki, szew pętli,
    styk z końcem ciągu;
  - nazwy torów spoza ASCII (#376): lista zamiast automatycznej zmiany nazw, pola rozjazdu, obrotnicy i kodu stacji SRK nie
    przyjmują liter spoza ASCII;
  - miejsce modelu terenu z kamery (#243): „Przypisz z kamery” dopiero po wybraniu miejsca, pliki `include` znajdowane bez względu
    na wielkość liter;
  - ortofotomapa (#417): czeka tylko na te chunki terenu, które przy tej odległości faktycznie przyjdą;
  - pędzel modeli (#375): punkt linii podany dwa razy nie stawia modelu za jej końcem;
  - pola (#315): pola przebudowywane tylko wtedy, gdy któreś dochodzi albo odchodzi, `patches_file` jako sama nazwa pliku, dziury
    wycinane ręcznie zostawiają dziury pól;
  - wypełnianie obszarów (#241): zmieniany obszar zachowuje swój obrót i warstwę.
- **Nowe ze `staging` (9 PR-ów jerrrrycho z 20.11)**, m.in. pamięć podręczna regionu zapisywana przez plik tymczasowy, selektor
  programów świateł, przełącznik oświetlenia przedziałów, start próbek dźwięku.

## Czego tu nie ma

- Wersje pól (#315) i listy wypełnionych obszarów (#241) w kształcie, w jakim weszły do `feat/gl46-migration`. Tu są nasze, nowsze
  wersje tych narzędzi z poprawkami z review wymienionymi wyżej; uporządkowania kodu z tamtych PR-ów nie są przeniesione.
- Odbicia śledzone promieniami w Vulkanie (`gfx.raytracing`) pokazują scenerię bez śniegu.

## Na co patrzeć przy testach

- Śnieg w Vulkanie nie był uruchamiany na karcie graficznej przed tym wydaniem: porównać z OpenGL 4.6 na tej samej scenerii i pogodzie.
- Śnieg, pogoda w fizyce i mokre nawierzchnie są domyślnie włączone. Wyłączniki w `eu07.ini`: `gfx.snow no`, `physics.snow no`,
  `physics.weather no`.
