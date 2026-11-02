# plk_audit.py: kontrola i poprawa torów scenerii wg norm PLK

Samodzielny skrypt (Python 3.8+, bez dodatkowych bibliotek). Czyta tory z plików `.scn` i `.inc` scenerii MaSzyny, razem z dołączanymi przez `include`. Sprawdza je według ST-T1-A6 i rozporządzenia z 17.09.2026. Granice norm są opisane w `docs/plk-normy.md`.

```
python plk_audit.py scenery/irt_tory.inc --raport raport.csv
python plk_audit.py scenery/irt_tory.inc --napraw scenery/irt_tory_plk.inc
python plk_audit.py scenery/moja.scn --typ-linii P --raport raport.csv
```

**Co sprawdza:**

| Kontrola | Co to jest |
|---|---|
| `R` | promień łuku |
| `D` | przechyłka |
| `I` | niedomiar przechyłki (także w rozjazdach) |
| `dD/ds`, `dD/dt` | rampy przechyłkowe |
| `dI/dt` | krzywe przejściowe (niedomiar próbkowany co metr) |
| `dI` | nagłe zmiany niedomiaru |
| `L` | najkrótsze proste i łuki |
| `i` | pochylenie podłużne |
| `Rv` | załomy niwelety |
| `rozjazd` | skosy zakazane w torach głównych, rozstaw torów przy połączeniach, odcinki pośrednie |
| `styk` | szczeliny, załamania i skoki przechyłki na stykach |

Wynik ponad P1 to „uwaga”, ponad P2 to „błąd”. Raport CSV podaje tor, wartość, granicę i współrzędne X i Z, więc miejsce łatwo znaleźć w edytorze.

**Co poprawia** (`--napraw`, próg `--prog-naprawy`, domyślnie P2):
- **Przechyłkę łuków z dwiema krzywymi przejściowymi**, gdy łuk nie spełnia norm. Dobiera ją od nowa tak, żeby jednocześnie spełnić tab. 9.6, 9.7, 9.12, 9.13 i 9.14. Rampy są wtedy prostoliniowe, na długości krzywych przejściowych.
- **Prędkość** tam, gdzie mimo to niedomiar lub jego zmiany przekraczają normę. `velocity` torów jest obniżane do największej wielokrotności 10 km/h, przy której warunki są spełnione, osobno dla każdego łuku.

Geometrii w planie ani niwelety skrypt nie zmienia. Za krótkie krzywe przejściowe, zbyt ciasne łuki, za duże pochylenia i załomy zostają w raporcie do poprawienia w edytorze albo w konwerterze.

Rodzaj toru jest ustalany automatycznie. Tor główny to tor z nazwą `_t1` lub `_t2` albo z prędkością od 80 km/h. Połączenie to krótki tor między dwoma rozjazdami. Wszystkie pozostałe tory są traktowane jako boczne. Można to zmienić opcjami `--rodzaj` i `--glowny-od`.
