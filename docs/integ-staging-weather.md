Gałąź `integ/staging-weather`: **build-staging** (`staging` z 18.11, commit `dd733861`, wydanie
[build-staging](https://github.com/MaSzyna-EU07/maszyna/releases/tag/build-staging)) **plus wszystko nasze**: renderery GL 4.6
i Vulkan z `feat/gl46-migration`, cała `ficzery2` i pogoda z porami roku z `feat/weather-seasons`.

To punkt odniesienia: wszystko, co jest w build-staging, jest tutaj (0 commitów za staging). Różnica to nasze funkcje.

## Jak powstała

1. `feat/weather-seasons` + `feat/gl46-migration` (master z 13.11 w środku) — commit `5608e7de`.
2. + `build-staging` (`dd733861`) — commit `5505778e`.

## Co warto wiedzieć

- **Zachowanie jak w masterze tam, gdzie master świadomie cofnął zmiany ze stosu maj00ra:** oś na rozjeździe jedzie po
  rozjeździe tak, jak jest ustawiony, a tor jest prosty według pierwszego wektora kontrolnego (zgodność z istniejącymi
  sceneriami).
- **Sygnalizacja w edytorze:** zakładki z mastera (Sygnały, Stacja, Sygnały stacji, Blokada liniowa) z łańcuchami SBL.
- **Samouczek edytora** (Pomoc → Samouczek) jest z powrotem, jak w masterze.
- **Hamulce:** powietrze w przewodach między pojazdami wymieniane po kroku wszystkich pojazdów (#387), także przy fizyce
  liczonej na wielu wątkach. Naprawiony (zregenerowany) wykolejony pojazd znów rusza po spadku.
- **Klucze `eu07.ini`** i komendy selftestu edytora — wszystkie z `ficzery2` i `feat/weather-seasons` są.
- Do budowania ze źródeł: nowy submoduł `ref/wamr` (`git submodule update --init ref/wamr`).

Testy jednostkowe: 890/890 (MSVC, Release).
