# Ogrzewanie rozjazdów i zwrotnice trzymane przez śnieg (`physics.weather.switches`)

Pogoda działa na infrastrukturę: śnieg i lód przy iglicach potrafią zatrzymać zwrotnicę, a elektryczne ogrzewanie rozjazdów (EOR)
je topi. Zmiana jest w exe i w shaderach obu rendererów (OpenGL 4.6 i Vulkan 1.4); scenerie i pliki zwrotnic, które o niej nie
wiedzą, działają dokładnie jak dotąd.

## Co robi symulator

- **Śnieg przy iglicach.** Każda zwrotnica ma własny stan: ile śniegu leży między iglicą a opornicą. Przybywa go tyle, ile
  napadało i nawiało na tory (`snow_cover::brought()`), ubywa tyle, ile stopiły grzałki, i nigdy nie jest go więcej, niż leży
  wokół (odwilż zabiera go razem z resztą).
- **Grzałki.** Tempo topnienia wynika z bilansu energii: moc grzałek na metr szyny, z czego do śniegu trafia 60% (wiatr 10 m/s
  zmniejsza to o połowę), podzielona przez ciepło potrzebne na ogrzanie śniegu do 0 °C i stopienie go (zależne od jego gęstości
  i od mrozu). Przy domyślnych 330 W na metr szyny, lekkim mrozie i świeżym śniegu to około 4 cm na godzinę: zwykły opad
  grzałki topią na bieżąco, w najcięższej śnieżycy z wiatrem przestają nadążać.
- **Zatrzymana zwrotnica.** Rozkaz przełożenia (event `switch`) nie jest wykonywany, gdy przy iglicach leży co najmniej 4 cm
  śniegu albo gdy szyny są oblodzone, jest mróz i ogrzewanie nie działa. Zwrotnica zostaje w dotychczasowym położeniu, w logu
  pojawia się wpis, a sceneria dostaje event (niżej). Rozprucie przez pojazd i przełożenie w edytorze działają zawsze.
- **Tylko tam, gdzie sceneria steruje ogrzewaniem.** Zwrotnica bez komórki ogrzewania jest uznawana za ogrzewaną i nigdy nie jest
  zatrzymywana. Dlatego stare scenerie zachowują się jak dotąd.
- Na odcinku iglic ogrzewanej zwrotnicy pojazdy zastają tyle śniegu, ile zostawiły grzałki (opór śniegu i przyczepność).

## Co widać (oba renderery)

Na odcinku iglic ogrzewanej zwrotnicy, z metrem zapasu wzdłuż toru i w pasie około 2,3 m od osi (zamknięcia, pręty, napęd),
śnieg sięga tylko tak wysoko, ile zostawiły grzałki, aż do gołej podsypki i podkładów. Miejsce jest mokre od stopionego śniegu
i nie pokrywa się gołoledzią. Ostatnie 4 cm śniegu opadają do zera w miarę topnienia, więc zwrotnica wolna od śniegu jest goła,
a trzymana przez śnieg ma go po podkłady. Po wyłączeniu ogrzewania śnieg wraca w tempie opadu i nawiewania.

Przy iglicach każdej zwrotnicy, także nieogrzewanej, pas śniegu trzymanego poniżej główek szyn jest o metr szerszy z każdej
strony niż na szlaku (2,3 m od osi zamiast 1,3 m).

Ograniczenia: plama ma rozdzielczość map śniegu (`gfx.skyvisibility`, piksel 0,5 do 1 m) i jest rysowana tylko w ich zasięgu
wokół kamery, przy włączonym `gfx.snow`. Mapa jest odświeżana co sekundę. W edytorze plamy nie ma. Odbicia z ray tracingu nie
pokazują śniegu, więc i plamy.

## Co dopisać w scenerii

Nazwy są szukane po nazwie toru zwrotnicy, tak jak dotychczasowe `nazwa:forced+` i `nazwa:forced-`. Wszystko jest opcjonalne.

| Wpis | Rodzaj | Znaczenie |
| --- | --- | --- |
| `nazwa:eor` | komórka pamięci | ogrzewanie tej zwrotnicy: pierwsza wartość większa od 0 = grzeje; druga wartość = moc w W na metr szyny (0 = domyślne 330) |
| `:eor` | komórka pamięci | to samo dla wszystkich zwrotnic scenerii, które nie mają własnej komórki (nazwa z samym dwukropkiem, żeby nie trafić na istniejącą komórkę `eor`) |
| `nazwa:stuck+` / `nazwa:stuck-` | event | rozkaz przełożenia w plus / minus nie został wykonany, bo iglice trzyma śnieg albo lód |
| `nazwa:set+` / `nazwa:set-` | event | rozkaz przełożenia w plus / minus został wykonany |

"Plus" to stan 0 eventu `switch`, "minus" to stan 1. Ogrzewanie włącza i wyłącza się zwykłym eventem `updatevalues` na komórce,
więc może nim sterować dyżurny, scenariusz albo awaria.

### Przykład: `scenery/zwrl34r300l.inc`

Poprawka tylko dopisuje linie. Stare exe nowych eventów nigdy nie wywoła, więc plik działa w nim jak dotąd.

```
origin (p2) (p3) (p4)
rotate 0 (p5) 0
node 1000 0 (p1)_wz model 1.9 0.04 0.4 180 wza.t3d wzl endmodel
node 50 0 (p1)_dzwiek sound 0 0 0 zwr_naped-1.wav endsound
event (p1)_dzwiek sound 0.0 (p1)_dzwiek 1 endevent
event (p1)_wz1 animation 0.5 (p1)_wz rotate wz 0 0 0 100 endevent
event (p1)_wz2 animation 0.5 (p1)_wz rotate wz 0 0 90 100 endevent
event (p1)_0 switch 0.0 (p1) 0 endevent
event (p1)_1 switch 0.0 (p1) 1 endevent
event (p1)_Vmax trackvel 0.0 (p1) -1 endevent
event (p1)_V40 trackvel 0.0 (p1) 40.0 endevent
//---DOPISANE: kontrola polozenia (plus / minus / brak) i ogrzewanie (1 = grzeje)
node -1 0 (p1)_k memcell 0 0 0 plus 0 0 none endmemcell
node -1 0 (p1):eor memcell 0 0 0 eor 1 0 none endmemcell
event (p1)_kplus updatevalues 0.0 (p1)_k plus * * endevent
event (p1)_kminus updatevalues 0.0 (p1)_k minus * * endevent
event (p1)_kbrak updatevalues 0.0 (p1)_k brak * * endevent
event (p1)_eor1 updatevalues 0.0 (p1):eor * 1 * endevent
event (p1)_eor0 updatevalues 0.0 (p1):eor * 0 * endevent
//---ROZKAZY: na koncu dopisane tylko (p1)_kplus / (p1)_kminus
event (p1)+ multiple 1 none (p1)_wz1 (p1)_0 (p1)_Vmax (p1)_dzwiek (p1)_kplus endevent
event (p1)- multiple 1 none (p1)_wz2 (p1)_1 (p1)_V40 (p1)_dzwiek (p1)_kminus endevent
event (p1):+ multiple 1 none (p1)_wz1 (p1)_0 (p1)_Vmax (p1)_dzwiek (p1)_kplus endevent
event (p1):- multiple 1 none (p1)_wz2 (p1)_1 (p1)_V40 (p1)_dzwiek (p1)_kminus endevent
//---DOPISANE: odpowiedzi nowego exe
event (p1):stuck+ multiple 2 none (p1)_wz2 (p1)_V40 (p1)_kbrak endevent
event (p1):stuck- multiple 2 none (p1)_wz1 (p1)_Vmax (p1)_kbrak endevent
event (p1):set+ multiple 0 none (p1)_wz1 (p1)_Vmax (p1)_kplus endevent
event (p1):set- multiple 0 none (p1)_wz2 (p1)_V40 (p1)_kminus endevent
rotate 0 0 0
endorigin
```

Co się zmienia w pliku:

1. Komórka `(p1)_k` trzyma wynik kontroli położenia: `plus`, `minus` albo `brak`. Semafory i scenariusze mogą ją sprawdzać
   warunkiem `memcompare`.
2. Komórka `(p1):eor` z wartością 1 włącza ogrzewanie. To ona oddaje zwrotnicę pod działanie śniegu: bez niej zwrotnica nigdy
   nie jest zatrzymywana. Eventy `(p1)_eor1` i `(p1)_eor0` są do użycia przez scenerię.
3. Rozkazy `(p1)+`, `(p1)-`, `(p1):+`, `(p1):-` dostają na końcu ustawienie kontroli. Reszta bez zmian.
4. `(p1):stuck+` po 2 sekundach (napęd próbuje i odpuszcza) cofa wskaźnik i prędkość do położenia, w którym zwrotnica została,
   i ustawia kontrolę na `brak`. `(p1):stuck-` odwrotnie.
5. `(p1):set+` i `(p1):set-` potwierdzają wykonane przełożenie. Powtarzają to, co rozkaz już ustawił, więc po odśnieżeniu
   i ponownym rozkazie kontrola wraca z `brak` do właściwego położenia.

W pozostałych plikach zwrotnic poprawka jest taka sama; różnią się tylko wskaźnik i prędkości wstawiane w `stuck` i `set`:
w `stuck+` to, co rozkaz minus ustawia jako wskaźnik i prędkość, w `stuck-` to, co ustawia rozkaz plus.

### Zgodność

| exe | plik zwrotnicy | wynik |
| --- | --- | --- |
| stare | stary | jak dotąd |
| stare | poprawiony | jak dotąd: dochodzą dwie komórki i kilka eventów, których nic nie wywołuje |
| nowe | stary | jak dotąd: zwrotnica uznana za ogrzewaną, nigdy nie zatrzymywana; w śniegu widać przy niej plamę topnienia |
| nowe | poprawiony | pełne działanie |

## Ustawienia (eu07.ini)

| Klucz | Domyślnie | Znaczenie |
| --- | --- | --- |
| `physics.weather.switches` | yes | ogrzewanie rozjazdów i zatrzymywanie zwrotnic; `no` = zwrotnice i śnieg przy nich jak przed zmianą |

Działa razem z `physics.snow` i `physics.weather`: przy wyłączonym którymkolwiek z nich nie robi nic.

## Liczby i ich źródła

- Moc: 330 W na metr szyny to przyjęta wartość domyślna dla grzejników oporowych mocowanych do stopki szyny. Przy obu szynach
  grzanych na około 14 m iglic daje to blisko 10 kW na rozjazd, czyli tyle, ile wychodzi średnio z danych o sieci PKP PLK (około
  18 000 rozjazdów i około 180 MW mocy zainstalowanej EOR). Moc konkretnej zwrotnicy ustawia druga wartość komórki `:eor`.
- Ciepło topnienia lodu 334 kJ/kg, ciepło właściwe lodu 2,1 kJ/(kg·K), gęstość śniegu z modelu pokrywy (`snow_cover::density()`).
- Progi: 4 cm śniegu między iglicą a opornicą; lód od połowy skali oblodzenia główek szyn (`railhead::ice`), tylko w mrozie.

## Czego nie ma (świadomie)

- Pary nad grzanymi iglicami.
- Osobnych stanów dla każdej iglicy i częściowego przełożenia: zwrotnica albo się przekłada, albo zostaje.
- Topnienia w środku toru osobno od pasa przy szynach: plama obejmuje cały pas, bo mapa śniegu ma piksel 0,5 do 1 m.
