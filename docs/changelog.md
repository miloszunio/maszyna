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

## nr 184

- Porządki pod Sonara z PR-ów do mastera (#375, #376): pętle i warunki pędzla oraz zamiany nazw torów uproszczone, ustawienia „Wiele modeli” wydzielone do osobnej funkcji, π z `std::numbers`. Działanie bez zmian.

## nr 181

- **Nazwy torów tylko w ASCII**: przy otwarciu scenerii w edytorze tory z nazwami ze znakami spoza ASCII dostają nazwy bez nich (ą→a, ł→l, ß→ss, Č→c itd., reszta znaków → `_`), małymi literami, jak czyta je parser. Nazwy w UTF-8 i w Windows-1250 rozpoznawane same.
  - Na środku ekranu przez **20 s** żółte ostrzeżenie z liczbą zmian i pierwszymi przykładami „stara → nowa”; pełna lista w `log.txt`.
  - Razem z torem zmieniają się komórki pamięci wskazujące tor i parametry includów, które nadają mu nazwę. Gdy nowa nazwa jest zajęta, dostaje `_2`, `_3`…
  - Eventy, składy i reszta dalej trafiają w tory po starych nazwach: wyszukiwanie toru po nazwie z polskimi znakami znajduje go też pod nazwą w ASCII, a eventy przypięte nazwą (`<tor>:event0`) znajdują się po nowej nazwie toru. Działa też w trybie jazdy.
  - Zmiana trafia do plików przy zapisie scenerii (definicje tych torów edytor pisze od nowa). Tory z plików, których edytor nie zapisuje, zostają w nich po staremu; ostrzeżenie podaje, ile ich jest.
- Sonar: oznaczone generatory losowe pędzla i dwa adresy http usług mapowych.

## nr 179

- **Pędzel w Otoczeniu od nowa**, dwie ścieżki:
  - **Pojedynczy model**: LPM stawia jeden model w miejscu krzyżyka na gruncie.
  - **Wiele modeli**: przy trzymanym LPM modele lądują w okręgu o zadanym **promieniu** wzdłuż drogi kursora (kolejne „pacnięcie” co pół promienia albo co odstęp). Wypełnienie do wyboru:
    - **losowe**: gęstość w sztukach na 100 m², nie bliżej niż odstęp;
    - **równomierne**: siatka trójkątów o kroku równym odstępowi, przypięta do świata, więc pociągnięcia na sobie nie dublują modeli;
    - **rozmyte do krawędzi**: coraz rzadziej ku brzegowi okręgu.
  - Cofanie: całe pociągnięcie to **jeden krok Ctrl+Z**.
- **Obrót** każdego modelu: stały (kąty x, y, z) albo losowy w zakresie ± stopni na każdej osi (x i z przechylają, y obraca wokół pionu). **Skala**: stała (procent x, y, z) albo losowa ± procent, jednakowa na wszystkich osiach albo osobna dla każdej.
- **Krzyżyk na gruncie** pod kursorem i **obrys okręgu** pędzla, położony na terenie.

## nr 178

- **Bank obiektów: grupy alfabetycznie** (bez względu na wielkość liter i spacje na początku nazwy; plik `nodebank.txt` zostaje w swojej kolejności).
- **„Dodaj model...”** w banku obiektów: model wpisany albo wybrany z listy modeli spod `models\`, których bank jeszcze nie ma (z filtrem), skóra, zasięg i grupa (istniejąca albo nowa). Wpis trafia do `nodebank.txt` na koniec swojej grupy, bank przeładowuje się sam, a miniatura robi się w tle (drugi proces symulatora z `-generate-nodebank-previews`).
- **„Zrób brakujące miniatury”** w „Więcej ustawień” banku: robi w tle miniatury wszystkich modeli, które ich nie mają.
- Podpowiedź przy modelu bez miniatury: pojawi się przy następnym uruchomieniu albo po przeładowaniu banku, kiedy już powstanie.

## nr 177

- **Obsypka wzdłuż obiektu** (pomysł użytkownika): od zewnętrznej górnej krawędzi każdego skrzydła, od tylnej krawędzi gzymsu i od wieńca rury wychodzi pas ziemi. Zaczyna się na koronie muru, robi lekki garb (ok. 18 cm) i opada parabolą w teren, kończąc się 8 cm pod nim. Jest szeroki na przekątną komórki siatki plus 0,6 m, więc przykrywa zęby wycięcia i szparę nad koroną skrzydła. Na narożnikach skrzydła obsypka zawija się w stronę toru i wzdłuż drogi. Tekstura do wyboru w polu „Tekstura obsypki” (wyszukiwarka: ziemia, żwir, kamień, grunt…); puste pole oznacza materiał terenu.
- **Edytor zawsze startuje w rzucie pionowym** (widok z góry), niezależnie od tego, w jakim widoku go zamknięto: zapamiętane ortofoto rysuje się dobrze tylko wtedy, gdy pierwszy obraz powstaje z góry.

## nr 176

- **Nasyp nad obiektem nie jest już przecinany przekopem drogi.** W nr 173–175 przekop drogi zatrzymywał się tylko tam, gdzie skarpa nasypu była wyżej niż „grunt rodzimy”. Za grunt rodzimy brałem wysokości sprzed modyfikatorów, a w terenie z NMT ten grunt zawiera już prawdziwy nasyp. Przekop szedł więc przez cały nasyp, także nad obiektem i między torami. Wyglądało to jak zasłony trawy wiszące z nasypu i strzępy terenu przy czołach, a pas gruntu przy murze dziedziczył te strzępy. Teraz o tym, co jest nasypem, decyduje wyłącznie modyfikator toru. Przekop dochodzi do otworu tylko z odcinka, który nie przechodzi pod koroną nasypu.

## nr 175

- **Pas gruntu przy obiekcie naprawdę się wczytuje**: w nr 173 część modelu nazywała się „grunt grass”, ze spacją. Format T3D rozdziela po spacjach, więc model nie wczytywał się od tej części i w komórkach przy murze zostawały białe zęby.
- **Przekop drogi dochodzi do otworu obiektu**: odcinek pod nasypem, który styka się z otworem tylko z jednej strony, jest wycinany jak przekop. Pod przęsłem przestaje zostawać wał ziemi w poprzek drogi. Między licami obiektu zakopanego nasyp zostaje nad nim.

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
