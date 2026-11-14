# Changelog ficzery2

Numer wydania to numer budowania na GitHubie; numery pominięte to budowania anulowane, bo zastąpiło je następne (ich zmiany są w kolejnym wydaniu). Obsługa wszystkiego: [poradnik.md](poradnik.md). Szczegóły techniczne i starsza historia: [ficzery2.md](ficzery2.md).

## Co jest w PR-ach do mastera (MaSzyna-EU07/maszyna)

Oznaczenia przy pozycjach niżej: **[PR #n]** otwarty PR, **[master #n]** już wcielone, bez oznaczenia: tylko w ficzery2.

| PR | Stan | Co |
|---|---|---|
| #241 | otwarty | Wypełnienie obszaru: Enter kończy, Esc porzuca obrys, lista wypełnionych obszarów |
| #243 | otwarty | Położenie NMT z kamery; `//$g` szukane w dołączanych plikach |
| #311 | otwarty | Profil podłużny drogi w osobnym oknie, na węzłach |
| #312 | otwarty | ~ pokazuje/chowa ortofoto, Ctrl+~ pobiera najpierw widok kamery; prędkość kamery w menu Widok |
| #315 | otwarty | Pola w trybie Teren, falowanie terenu pod nimi, naturalne skarpy |
| #240 | wcielony | gl46: głębia kursora edytora |
| #302 | wcielony | Obrót modelu gizmem nie przeskakuje za 90° |
| #309 | wcielony | Kursor rysowany w obrazie (streamy, nagrania) |
| #310 | wcielony | Punkty dróg łapane na ekranie, także pod terenem; Alt+ciągnij: wysokość |
| #313 | wcielony | Kółko wciśnięte i ciągnięte przesuwa widok w perspektywie |
| #314 | wcielony | Punkty narzędzi terenu kładzione wzdłuż promienia przez kursor |
| #327 | wcielony | `Mouse_Hit()` nie mówi „tak”, gdy nie wie |

Do wystawienia później: drzewa (ZIELSKO) i działki, tłumaczenia `.po`. Tory z mapy, obiekty pod torem i mosty, drogi gruntowe, GeoTools: na razie tylko ficzery2.

## nr 173

- **Dwa tory i więcej**: skrzyżowania drogi z torami leżącymi obok siebie to jeden obiekt. Dźwigary i kratownice mostów z jazdą dołem stoją na zewnątrz skrajnych torów (wcześniej jeden stał między torami); w mostach z jazdą górą balustrady są tylko na krawędziach.
- **Zszywanie terenu całymi komórkami siatki** i pasem gruntu w modelu obiektu (te same trójkąty i wysokości co teren): koniec z piłą i szparami przy skrzydłach i czołach.
- **Przekop drogi do stopy nasypu**: przed licem nie zostaje wał ziemi; dno otworu biegnie jak przekop.
- **Płyta i rama wyglądają inaczej**: płyta ze stopkami belek od spodu, oczepami, łożyskami i balustradą; rama ze skosami, skrzydłami w linii ścian i betonową attyką.
- **Podgląd obiektu** obok listy rodzajów, obracany myszą.

## nr 172

- **Obiekty pod torem od nowa, osiem wyraźnie różnych rodzajów** wzorowanych na obiektach polskich linii: rura z blachy falistej (końce ścięte po skarpie, wieniec), przepust skrzynkowy (skosy, skrzydła ukośne), sklepienie murowane (archiwolta, gzyms, attyka), wiadukt ramowy (duże skosy, skrzydła w linii ścian), płyta z belek obetonowanych (wysunięta krawędź, przekładki łożysk), blachownica z jazdą górą, blachownica z jazdą dołem, kratownica z jazdą dołem (od 32 m ze stężeniami górą i portalami).
- **Wysokość pilnowana:** czoło tam, gdzie rzeczywisty teren nasypu schodzi do góry obiektu, skrzydła do końca skarpy. Gdy obiekt się nie mieści, **żółty napis** mówi, o ile obniżyć drogę (wcześniej „nic się nie działo”: odmowa szła do szarego statusu). Najmniej pod torowiskiem zabierają mosty z jazdą dołem (ok. 0,55 m).
- **Teren zszyty z obiektem:** każda krawędź dziury leży w murze; dno otworu sięga pod skrzydła; teren nad obiektem podniesiony, żeby go przykrył.
- **Nowe chunki zszywane z sąsiadami** od razu przy tworzeniu (krawędź = wysokość sąsiada, wygaszanie na 40 m), bez przycisku.
- Etykiety UTWÓRZ tylko dla brakujących chunków bliżej niż 100 m od kamery.
- Kliknięcie nad wyciętym terenem łapie punkt na widocznym dnie (wcześniej narzędzie dróg łapało punkt kawałek dalej).

## nr 164

- **Mosty z torem na konstrukcji:** most blachownicowy (tor na pomoście, dźwigary pod nim) i most kratownicowy (tor między kratownicami). Nasyp wycięty na całej szerokości między przyczółkami, ścianki żwirowe, pomost, łożyska.
- **Łożyska na filarach** w obiektach wieloprzęsłowych.
- Changelog i poradnik uzupełniony o budowanie dróg i drobne rzeczy edytora.

## nr 163

- **Kilka przęseł na filarach** w obiektach pod torem (1–6).
- **Przepust rurowy** z blachy falistej.
- **Tekstura stali** dla dźwigarów, balustrad i rury.
- **Ekran ładowania** z napisem „Przygotowanie grafiki…” zamiast czarnego obrazu przed pierwszą klatką.

## nr 160

- **Obiekt pod torem jako jeden model T3D z części:** przyczółki i skrzydła, sklepienie albo płyta, czoła z gzymsami, attyka albo balustrady, dźwigary na łożyskach.

## nr 159

- **Droga gruntowa / asfaltowa:** gruntowa bez pasów, 3 razy luźniejszy profil, nierówna albo równa jak stół.
- **Obrót tekstury** drogi i skrzyżowania (`texangle`).
- **Rondo:** zjazdy bliżej siebie, czyli więcej zjazdów.
- **Teren dociągany do drogi:** zostawia nasypy torów, równa szerzej niż droga, usuwa trawę spod nawierzchni.
- **Obiekty pod torem:** sklepienie półkoliste, przęsło belkowe, powtarzanie tekstury, skrajnia „jaka się zmieści”.

## nr 154

- **Okno „Opcje narzędzia”** wraca po wybraniu narzędzia; zamknięcie trafia do `log.txt`.
- **UTWÓRZ** nad brakującym chunkiem w GeoTools.
- **Esc:** w GeoTools czyści obrys i zaznaczenia, w Drogach wraca do zaznaczania.

## nr 153

- **GeoTools → Tory:** linie z mapy IRT projektowane według norm i układane jako tory wokół kamery (łuki, przejściówki, przechyłki, ograniczenia, dwa tory na liniach dwutorowych).
- Bramka Sonara ficzery2: poprawki zgłoszeń z „Idź do” i ZIELSKA.

## nr 145

- **Poprawki z recenzji PR-ów przeniesione** **[PR #315] [PR #311]**: dziura pola po wczytaniu sceny, „Obrysuj ponownie”, dziury nakładających się pól, „Wyrównaj” na wolnym końcu drogi.

## nr 112–139

- **GeoTools** zamiast Ficzery (F7): zakładki Pola, Drzewa, Torowisko i skarpy, Idź do.
- **Pola** **[PR #315]**: przeciąganie punktów obrysu, pierścień kierunku rzędów, ostrzeżenie i dodawanie chunków pod obrysem.
- **Drzewa przy torze** (ZIELSKO): osobna grupa, wymiana i zaznaczanie razem; ZIELSKO mówi, czemu nie sadzi.
- Koncepcja torów z danych, opis czarnego ekranu.

## nr 111 i wcześniej

- **Obrys z działki ewidencyjnej** (ULDK), edycja punktów obrysu, strzałka rzędów.
- nr 109: poprawki z recenzji #309–#315 i #327 **[master #309, #310, #313, #314, #327]**, liczniki pętli dla Sonara, `eu07_user.ini`.
- nr 80–85: Pola w trybie Teren **[PR #315]**, uprawy, kolory i ścieżki.
- Wcześniejsze: profil drogi **[PR #311]**, ortofoto pod ~ **[PR #312]**, lista wypełnień **[PR #241]**, ZIELSKO z kosmosu, obiekty pod torami zamiast tunelu. Pełna lista w [ficzery2.md](ficzery2.md).
