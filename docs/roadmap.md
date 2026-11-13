# Mapa drogowa: edytor ficzery2 → 3.0

Stan na 12.11.2026, po nr 129. Kierunek: szybsze robienie realistycznych scenerii z danych (GUGiK, OSM) zamiast ręcznego
klikania; później fikcyjne, z poprawkami istniejących. Każda zmiana, która idzie upstream, idzie sprawdzona w grze, nie tylko
z kompilacją i Sonarem.

Bieżący stan PR-ów i testów: arkusz „MaSzyna ficzery2: PR-y i testy”.

## 1. PR-y upstream (MaSzyna-EU07)

| PR | Co | Stan |
|---|---|---|
| #309 | kursor rysowany w obrazie (streamy) | **w masterze** |
| #310 | punkty dróg pod terenem, Alt+przeciąganie | **w masterze** |
| #313 | przesuwanie widoku kółkiem w perspektywie | **w masterze** |
| #314 | promień do terenu dla narzędzi | **w masterze** |
| #327 | `Mouse_Hit()` bez optymizmu | **w masterze** |
| #311 | okno profilu drogi | rebase na master 12.11, Sonar zielony, czeka na review |
| #312 | ~ / Ctrl+~ ortofoto, prędkość kamery | rebase na master 12.11 (po #334 maj00ra), Sonar zielony, czeka |
| #315 | Pola w trybie Teren (gl46) | czysto na gl46, Sonar zielony, czeka; do przeniesienia na master pod GeoTools (3.0) |
| #241 | wypełnianie obszaru, lista obszarów (gl46) | poprawki z review wypchnięte 12.11 |
| #243 | NMT z kamery, `//$g` z include (switch-blades) | poprawki z review wypchnięte 12.11, baza naprawiona |
| #335 | issue: nagrywanie ekranu GL46 na pełnym ekranie | zgłoszone, z alternatywą (zastrzeżenie) |

## 2. U nas, w testach (ficzery2)

- **GeoTools (F7)** zamiast Ficzery: zakładki Pola, Drzewa, Torowisko i skarpy, Idź do (nr 128).
- **ZIELSKO** działa; drzewa do 30 m od osi toru jako `zielsko_tor_N`, wymiana na modele 3D, zaznaczanie (nr 128).
- **Obrys z działki (ULDK)**, przeciąganie punktów, **pierścień kierunku rzędów** na ziemi (nr 129), puste chunki pod obrysem (nr 128).
- **Uprawy na polach**, fala „Pole uprawne” (nr 83–84); korekcja obrazu i wyostrzanie (nr 84), `eu07_user.ini` (nr 85).

## 3. Teraz

1. **Czarny ekran przy pierwszym starcie nowego exe** (~35 s kompilacji shaderów po zniknięciu ekranu ładowania): ekran
   ładowania z napisem do końca pierwszej klatki (PR upstream), stała nazwa exe w naszych buildach; potem kompilacja w trakcie
   wczytywania z paskiem postępu. Do akceptu.
2. **Tory z danych**: etapy 1-2 wdrożone (GeoTools → Tory: mapa IRT, projekt według norm, układanie torów szlakowych w chunkach wokół kamery); do sprawdzenia w grze. Dalej: teren pod torami z przepustami i skarpami wewnętrznymi, rozjazdy, stacje (`docs/koncepcje/tory-z-danych.md`).
3. **Kawałki ortofoto jadące za kamerą**: czekamy na screen.
4. **Drogi, zgłoszone 13.11** (kolejne wersje, po kolei):
   - ~~obrót mapowania tekstury drogi o kąt~~ (zrobione 13.11: „Obrót tekstury [°]”);
   - ~~dwa rodzaje drogi: asfaltowa i gruntowa~~ (zrobione 13.11; do sprawdzenia w grze, zwłaszcza uskok na styku gruntowej ze skrzyżowaniem);
   - ~~rondo: więcej zjazdów~~ (13.11: ramiona zjazdów na rondzie mogą być krótsze; do sprawdzenia w grze, ile realnie wchodzi);
   - ~~wycinanie pod torem na wiadukt~~ (poprawione 13.11, do sprawdzenia w grze): było zepsute (screen 13.11: przekop pod torem wychodzi jako klin, skarpy wchodzą na jezdnię, nad drogą zostaje trójkąt terenu);
   - ~~dociąganie terenu do drogi niweletą szerszą niż sama droga~~ (pole „Płaski teren poza drogą”), żeby pobocza nie odstawały;
   - ~~trawa prześwituje przez asfalt~~ („Usuń trawę spod drogi”): wyrzucić trawę (pokrycie terenu) spod nawierzchni drogi;
   - **generator wiaduktów i mostów w przekopach** (częściowo: 5 kształtów, powtarzanie tekstury, dopasowanie skrajni; z przeglądu źródeł: na kolei przeważają przęsła stalowe ok. 42 % i betonowe ok. 37 %, starsze obiekty to sklepienia ceglane i kamienne, przepusty płytowe, ramowe i rurowe). Zrobione 13.11: jeden model T3D z podmodelami, dźwigary, łożyska, balustrady, attyka. Doszły też: przęsła z filarami, rura z blachy falistej, tekstura stali. Zrobione w nr 164: mosty z torem na konstrukcji (blachownicowy i kratownicowy, nasyp wycięty między przyczółkami), łożyska na filarach. Do sprawdzenia w grze. Pierwotne założenia: najczęstsze kształty (do ustalenia z przeglądu źródeł), model 3D generowany po wybraniu z listy, szerokość między przyczółkami wskazana albo obliczona, na pierwsze wdrożenie jedna tekstura z opcją zagęszczania/powtarzania.

## 4. Gałąź 3.0 (pod RC)

Czysta gałąź od **aktualnego mastera** (bez wciągania gl46: tamtą integrację robią Hirek i maj00r). Każdy nasz ficzer to osobna
gałąź od mastera i osobny PR; 3.0 = master + te gałęzie po kolei, składane od nowa skryptem, gdy master się ruszy. Kolejność:
otwarte PR-y → Pola na master pod GeoTools → zaawansowane obrysy → działki z pierścieniem rzędów i pustymi chunkami → uprawy →
ZIELSKO. Grade/wyostrzanie siedzą w rendererach GL46/Vulkan, których master nie ma: osobno do gl46. `ficzery2` zostaje
poligonem, dopóki 3.0 nie ma wszystkiego.

## 5. Dalej: propozycje

Z danych:
- **Tory z danych** (GeoTools → Tory): obszar z chunka + promień, przebieg z IRT / BDOT10k / OSM, dopasowanie do ST-T1-A6
  narzędziem układania maj00ra, NMT, formacja, obiekty z osobnymi skarpami. Szczegóły: `docs/koncepcje/tory-z-danych.md`.
- **NMPT − NMT:** wysokości koron i dachów; drzewa tam, gdzie nie ma modeli 3D, i kontrola ZIELSKO.
- **BDOT10k jako szkic scenerii:** lasy, zadrzewienia, łąki, uprawy, sady i wody jako pola z presetem; osie dróg do narzędzia
  dróg z klasą do profilu; rowy i stawy jako wcięcia i powierzchnie wody.
- **Budynki z modeli 3D GUGiK** (LOD1/LOD2) jako zastępcze bryły w tle, przez tę samą maszynerię co ZIELSKO.
- **Zadrzewienia śródpolne i miedze** wzdłuż granic działek i rowów.

Obraz:
- **Rzędy w trawie 3D i kalendarz upraw** (faza uprawy z daty symulacji).
- **Impostory dalekich drzew** z modelu 3D, las do 2–3 km tanio.
- **Pędzel skraju lasu:** krzaki, młodniki, wyższa trawa na granicy las–łąka.
- **Miedza przy brzegu pola, trawa zabrana z torowiska.**

Kolej i porządek:
- **Infrastruktura z OSM** (słupy trakcyjne, semafory, przejazdy, perony) jako punkty kontrolne przy torach.
- **Sprawdzanie scenerii** jednym przyciskiem: obiekty w powietrzu i zakopane, dziury bez powierzchni, drzewa na torowisku
  i drodze, nieciągłości dróg, z „Przejdź do”.

Formaty i zakres usług (ULDK, BDOT10k, NMPT, modele 3D budynków) sprawdzamy przed pisaniem kodu.
