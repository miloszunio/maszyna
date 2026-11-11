# Mapa drogowa: edytor ficzery2 → 3.0

Stan na 11.11.2026, po nr 109. Kierunek: szybsze robienie realistycznych scenerii z danych (GUGiK, OSM) zamiast ręcznego
klikania; później fikcyjne, z poprawkami istniejących. Każda zmiana, która idzie do Hirka, idzie z testem skryptem edytora
(`EU07_EDITOR_SELFTEST`) na kopii scenerii, nie tylko z kompilacją i Sonarem.

Bieżący stan PR-ów i testów: arkusz „MaSzyna ficzery2: PR-y i testy”.

## 1. Wysłane do Hirka (PR-y)

| PR | Co | Stan |
|---|---|---|
| #309 | kursor rysowany w obrazie (streamy) | review: merge; poprawka wypchnięta |
| #310 | punkty dróg pod terenem, Alt+przeciąganie | review: needs work; poprawki wypchnięte |
| #311 | okno profilu drogi | review: needs work; poprawki wypchnięte |
| #312 | ~ / Ctrl+~ ortofoto, prędkość kamery | review: merge po 1–2; poprawki wypchnięte |
| #313 | przesuwanie widoku kółkiem w perspektywie | review: merge; poprawka wypchnięta |
| #314 | promień do terenu dla narzędzi | review: needs work; poprawki wypchnięte |
| #315 | Pola w trybie Teren, falowanie, naturalne skarpy | review: needs work; poprawki wypchnięte |
| #327 | `Mouse_Hit()` bez optymizmu | nowy |
| #241, #243 | wypełnianie obszaru, NMT z kamery (stare) | do decyzji: poprawić albo zamknąć |

Zostało przy nich: test skryptem w edytorze, komentarz pod każdym PR-em z opisem poprawek, rebase konfliktów (#309 z #312,
#310 z #311).

## 2. U nas, w testach (ficzery2)

- **Poprawki z review #309–#315** przeniesione do nas (nr 109).
- **Uprawy na polach** (nr 83–84): 15 upraw z teksturą, trawą 3D i rzeźbą; tekstury z `tools/zielsko/uprawy.py`; kolor pola
  zgodny z trawą, bez jesiennej orki pod zbożem.
- **Fala „Pole uprawne”** (nr 83): rzędy wzdłuż najdłuższego boku, falujące, zagony, uwrocia, ścieżki technologiczne.
- **Korekcja obrazu i wyostrzanie po TAA** (nr 84), **`eu07_user.ini`** (nr 85).
- **ZIELSKO z kosmosu** (nr 72 / 82): drzewa z modeli 3D drzew Geoportalu. **Nie działa sadzenie: szukamy przyczyny.**

## 3. Teraz

1. **Naprawić sadzenie drzew (ZIELSKO).** Log z próby, ustalenie: dane (serwer, arkusze, współrzędne) czy sito i sadzenie.
   Zapas: wysokości koron z NMPT − NMT tam, gdzie modeli 3D nie ma.
2. **(w nr 111, do testu)** **Obrys z działki ewidencyjnej** jako sposób obrysu: klik w teren albo numer działki (ULDK GUGiK), Shift+klik dokłada
   sąsiednią, potem zwykła edycja punktów; odpowiedzi trzymane na dysku.
3. **(w nr 111, do testu)** **Strzałka kierunku rzędów** przy fali „Pole uprawne”: wektor w środku obrysu, grot do przeciągania (Ctrl: co 5°),
   tekstura uprawy obraca się z rzędami.
4. **PR-y z naszych rzeczy**, każdy z testem w edytorze: uprawy + fala pola, obrys z działki + strzałka, korekcja obrazu,
   `eu07_user.ini`, zaawansowane obrysy (krzywe, Bézier, ręcznie, wzdłuż toru, różdżka) po merge #315.

## 4. Gałąź 3.0 (pod RC)

Nowa gałąź od aktualnego `feat/gl46-migration` Hirka i mastera, ze zmianami Jerrego i maj00ra, a nasze ficzery już
w docelowych miejscach (podział na pliki jak u Hirka, nie `editormodeficzery`). `ficzery2` zostaje, dopóki 3.0 nie
zadziała w grze.

## 5. Dalej: propozycje

Z danych:
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
