# Koncepcja: pola i uprawy (przegląd i plan poprawy)

Stan: **E1–E2 w kodzie** (gałąź `feat/uprawy`, 17.11.2026; opis wdrożenia na końcu). Reszta to nadal koncepcja. Pisane 17.11.2026 na podstawie `ficzery2` (build z 15.11): uprawy z nr 83–85,
trawa 3D (`rendering/common/grassfield`, `shaders/{vk14,ogl46}/grass.vert/.frag`), presety `crop_presets()` w
`application/editormodefeatures.cpp` i tekstury z `tools/zielsko/uprawy.py`. Do tego paleta kolorów: `pola-uprawy-kolory.png`.

Cel: żeby pole z kabiny dało się rozpoznać. Pszenica ma wyglądać jak pszenica, a owies jak owies. Zboże ma być gęste i mieć
wysokość: widać ścianę łanu przy miedzy i torze. Po żniwach ma zostać prawdziwe ściernisko, a kukurydza ma przejść cały
sezon: od rzędów na ziemi, przez dwa i pół metra zieleni i suchą wiechę, do ścierniska przez zimę.

---

## 0. Diagnoza w skrócie: dlaczego dziś jest słabo

Za złym wyglądem stoi sześć konkretnych przyczyn w kodzie, nie jedna. Kolory są dopiero piąte.

| # | Objaw | Przyczyna w kodzie |
|---|---|---|
| 1 | „Ni to owies, ni pszenica” | `grass.vert` rysuje **każde** zboże tak samo: jeden płaski pasek zwężający się do zera (`halfwidth = width·(1 − t^1.5)`). Nie ma kłosa, wiechy, ości ani liści. Pszenica, jęczmień, żyto i owies różnią się tylko kolorem, wysokością i `bend`, więc wszystkie wyglądają jak wysoka trawa. |
| 2 | „Zboże cienkie” od kilkunastu metrów | Rzednięcie zaczyna się w `thinning = max(3, 0.2·distance)`, czyli **od 12 m** przy domyślnych 60/70 m. Liczba źdźbeł spada jak `(12/d)^1.5`, a szerokość rośnie tylko o `1/√share`, więc pokrycie maleje jak `√share`: **ok. 50% przy 30 m i ok. 30% przy 50–60 m**. Od 60 m wszystko znika do zera przy 70 m. |
| 3 | Rzadki, „przejrzysty” łan | Preset ma 150–180 źdźbeł/m² po 1 cm szerokości na całej wysokości. Prawdziwa pszenica ma **500–650 kłosów/m²** (źdźbeł z liśćmi jeszcze więcej): łodyga 3–4 mm, kłos 10–14 mm, do tego liście. Gęstości nie da się po prostu podnieść 3×, bo potrzebne są kępy i kłos (pkt 2). |
| 4 | Pole płaskie jak dywan | Trawy nie ma w depth prepassie (nie łapie GTAO, `docs/opengl46-renderer.md`). Nie rzuca cieni ani na siebie, ani na ziemię, a jej normalna to w 80% normalna gruntu. Całe pole jest więc oświetlone jednakowo. Jedyne cieniowanie to `albedo *= mix(0.7, 1, t)`. Pod słońce zboże nie prześwituje, choć to najbardziej charakterystyczny widok łanu. |
| 5 | Za 70 m tylko „kolorowy papier” | Dalej jest już tylko tekstura terenu: `farm01` przekolorowana w `uprawy.py`, czyli **rzędy widziane z góry** z ciemnymi bruzdami. Pod kątem z kabiny dojrzałe zboże wygląda przez to jak żółte paski na ziemi. Pole nie ma też wysokości: nie widać ściany łanu na krawędzi pola, a to z pociągu najważniejszy znak, że to zboże, a nie ugór. |
| 6 | Kolory | Zielenie są za nasycone (S ≈ 72%, C\* ≈ 51, prawie zero niebieskiego). Owies ≈ jęczmień (**ΔE 6,8**, w zamgleniu nie do odróżnienia). Pszenica jest za jasna jak na łan (liniowe R 0,55). Kwitnący rzepak ma trawę 3D oliwkową (nasada mieszana z żółtą górą), a teksturę cytrynową, więc pole zmienia kolor na granicy trawy. Ściernisko ≈ pszenica (ΔE ≈ 10) i nie widać w nim ziemi. |

Braki: **kukurydza** (w poradniku stoi wprost „Kukurydzy nie ma”), **ściernisko po kukurydzy**, ściernisko po rzepaku, słoma
leżąca, wały słomy, bele, podorywka, wyległe placki i kalendarz upraw (jest w `roadmap.md` jako propozycja).
Limit **4 rodzajów trawy na chunk** (`grass_chunk::kinds`) nie pozwala złożyć ścierniska z kilku warstw (pkt 5).

---

## 1. Na czym to stoi (`ficzery2`)

| Element | Gdzie | Co robi dziś |
|---|---|---|
| Rodzaj trawy | `heightmap::grass_def` (sRGB) → `gfx::grass_kind` (liniowe, `publish_grass`) | base/tip/dry, wysokość, rozrzut, szerokość, gęstość, bend, seasonal |
| Tabela rodzajów na GPU | VK: `grass_kinds` RGBA32F 4×5; GL: uniformy `grass_base[4]` … | 4 rodzaje na chunk |
| Źdźbło | `grass.vert` | pasek 2·seg+1 wierzchołków, seg 3/2/1 wg odległości; R2 + jitter (bez rzędów); wiatr: lean + podmuch + drganie |
| LOD | `grassfield.cpp::prepare`, `grass.vert::lod_share` | thinning 0,2·distance, fade, reach; kafle 16×16 na chunk, max 65 536 źdźbeł na kafel |
| Pora roku | `grass_season()` | dla zbóż `seasonal = 0`, czyli faza wynika z presetu, nie z daty |
| Uprawa w polu | `crop_presets()` (15 sztuk) | materiał `uprawy/*`, rodzaj trawy dopisany do palety, fala „Pole uprawne” |
| Pole | `surface_patch.outline`, `surface_patch.field` (`tram= angle= head= drift= soil=`) | obrys, kierunek rzędów, uwrocia, ścieżki technologiczne |
| Tekstury | `tools/zielsko/uprawy.py` | `farm00/01/03/05/08` przekolorowane do „średniego koloru trawy” `0,35·base + 0,65·tip` |

Wniosek: szkielet jest dobry. Mamy pole z obrysem, kierunkiem rzędów, uwrociami i ścieżkami, trawę GPU-driven z deterministycznym
rozkładem i spójność koloru tekstury z trawą. Brakuje **morfologii rośliny**, **objętości** i **reprezentacji pola z
daleka**. Na tym skupia się reszta dokumentu.

---

## 2. Roślina zamiast źdźbła: archetypy

### 2.1 Model danych

`grass_def` / `grass_kind` dostają **archetyp** i parametry głowy. Stare palety bez tych pól czytają się jako `blade`, czyli
tak jak dziś.

```yaml
grass:
  - name: Pszenica dojrzała
    archetype: cereal        # blade | cereal | awned | panicle | rape | stubble | straw | corn | residue
    base: [0.46, 0.38, 0.22] # sRGB, nasada łodygi
    tip:  [0.74, 0.62, 0.40] # górna część łodygi
    head: [0.80, 0.63, 0.36] # kłos / wiecha / kwiat / ścięcie
    height: 0.85             # do szczytu kłosa
    variation: 0.10
    width: 0.0035            # łodyga
    head_length: 0.09
    head_width: 0.012
    head_droop: 0.05         # 0 pionowo … 1 kłos zwisa w dół
    awns: 0.0                # długość ości, m
    leaves: 1                # liście (paski odchodzące z dolnej 1/3)
    density: 450             # pędów/m² (pkt 2.3)
    clump: 3                 # pędów z jednego krzewienia
    rows: 0.125              # rozstaw rzędów, m; 0: bez rzędów
    translucency: 0.35
    sheen: 0.2               # połysk przy kątach ślizgowych (nalot woskowy, ości)
    dry: [0.80, 0.63, 0.36]
    seasonal: 0
    reach: 140               # m, własny zasięg rodzaju (pkt 4)
```

Na GPU tabela rośnie z **4×5 do 4×8** RGBA32F. W GL to kilka nowych uniformów albo przejście na tę samą tabelę co w VK (lepiej:
jedna ścieżka danych). Push constanty zostają bez zmian. Liczba rodzajów na chunk rośnie z **4 do 8**: dwie tekstury
gęstości RGBA8, jak wagi warstw w `terrain_materials`. To potrzebne do ścierniska (kikuty, słoma, samosiewy, chwasty) i do
mieszanych brzegów pól.

### 2.2 Geometria archetypów (w `grass.vert`, bez assetów)

Jedna instancja to dalej jedna roślina z numeru instancji. Archetyp wybiera **liczbę wierzchołków** i ich rozkład. Kafel z
różnymi archetypami idzie w osobnych drawach, po jednym na archetyp: `tile_draw` dostaje pole `archetype`, a `prepare()`
rozdziela kafel po archetypach obecnych w chunku.

| Archetyp | Kształt | Trójkąty (blisko / daleko) |
|---|---|---|
| `blade` | dzisiejsze źdźbło | 5 / 1 |
| `cereal` (pszenica) | łodyga do `height − head_length`, potem **kłos**: wyraźnie szerszy, zaokrąglony, z jasnym brzegiem; skrzyżowane 2 paski, żeby z boku nie znikał. Liść: krótki opadający pasek z 1/3 wysokości, tylko w fazach zielonych | 9 / 3 |
| `awned` (jęczmień, żyto) | jak `cereal` + **ości**: wąski pasek przedłużający kłos. W jęczmieniu kłos z ośćmi **zwisa** (`head_droop` 0,5–0,8), żyto jest wyższe, kłos smukły i lekko pochylony | 11 / 3 |
| `panicle` (owies) | łodyga + **wiecha**: 4–6 krótkich wiszących kłosków (po 2 trójkąty) na gałązkach rozchodzących się z osi; luźna, ażurowa sylwetka | 17 / 3 |
| `rape` | łodyga z 2–3 rozgałęzieniami, na górze **czapa kwiatów** (3–4 małe karty skierowane do kamery). Dojrzały: łuszczyny zamiast kwiatów, ciemniejsze | 14 / 4 |
| `stubble` | **kikut ścięty płasko**: stała szerokość, bez zwężenia, jaśniejszy pierścień ścięcia, lekki losowy przechył | 4 / 2 |
| `straw` | **leżąca słoma**: paski 10–40 cm położone na ziemi (oś pozioma, losowy kierunek, lekko uniesione końce) | 2 / 2 |
| `corn` | pkt 6 | ~90 / 8 |
| `residue` | resztki kukurydzy: szerokie krótkie płaty leżące, poskręcane | 4 / 2 |

Oświetlenie części: kłos i wiecha mają **własny kolor** (`head`), więc pole z daleka ma kolor kłosów, a z bliska widać
zielonkawe lub żółte łodygi pod kłosami. To dokładnie różnica między „owsem a pszenicą”. Wiecha owsa jest jasnozielonkawo-biała
i ażurowa, a kłos pszenicy gęsty, złoto-ochrowy i pionowy.

### 2.3 Gęstość, kępy i rzędy

- **Gęstość realna**: pszenica 500–650 kłosów/m², jęczmień 600–800, żyto 450–550, owies 400–500, rzepak 40–60 roślin/m²,
  kukurydza 7–9 roślin/m². Rysujemy **gęstość realną** (pkt 9: koszt nie ogranicza). Pędy idą w **kępach** po 2–4 z jednego
  punktu, z rozrzutem kąta (krzewienie), bo tak rośnie zboże i wygląda to naturalniej niż równy deszcz R2.
- **Gęstość jako parametr pola**: w panelu Pól przy „Obsiane” jest suwak **„Obsada, pędów/m²”** z wartością domyślną z uprawy
  (pszenica 600, jęczmień 700, żyto 500, owies 450, rzepak 50, kukurydza 8 roślin/m²). Zapis w `surface_patch.field` jako
  `dens=600`, więc przemalowanie i Ctrl+Z ją odtwarzają. Nie zmienia to formatu danych trawy. `density` rodzaju to **górna
  granica** (np. 800 dla zbóż), a pole maluje w teksturze gęstości chunka wartość `obsada / density` (0..1, RGBA8 jak dziś).
  Jeden rodzaj „Pszenica dojrzała” obsłuży więc pola rzadsze (słaba gleba, brzeg) i gęstsze bez dokładania nowych rodzajów
  do palety. Globalny `gfx.grass.density` mnoży to dalej, jako suwak wydajności po stronie gracza. Ten sam mechanizm
  przyda się do rzadszego zboża w uwrociach i na brzegu pola (np. −20%).
- **Rzędy**: współrzędna w poprzek rzędów przyciągana do siatki `rows` z jitterem ±15% rozstawu. Kierunek pochodzi z pola:
  `angle` i `drift` z `surface_patch.field`, a na uwrociach rzędy idą dookoła, jak dziś w fali „Pole uprawne”. Na GPU w chunku
  jest nowa tekstura **RG8 kierunku rzędów** (cos, sin) w rozdzielczości próbek trawy, wypełniana przez streamer z pól. Różne
  pola w jednym chunku mają wtedy różne kierunki. Zboże (12,5 cm) pokazuje rzędy tylko z bliska, wzdłuż nich: zbiegające się
  linie, które przy torze idącym wzdłuż pola są bardzo widoczne. Kukurydza (75 cm) ma rzędy zawsze i pod każdym kątem.
- Ścieżki technologiczne już są (trawa przerzedzona w koleinach). Zostają bez zmian.

### 2.4 Pole żyje: zmienność w obrębie pola

- **Dojrzewanie nierówne**: szum o skali 20–60 m przesuwa fazę (kolor kłosa między `tip` a `head`). **Brzegi pola** dojrzewają
  wcześniej (pas 5–10 m), a obniżenia terenu później.
- **Wyległe placki** po burzy: plamy 3–15 m, gdzie `bend → 0,9–1` w jednym kierunku. Bardzo realistyczne i tanie (szum + próg).
  Edytor: suwak „wyleganie” na polu.
- **Chwasty w brzegu**: maki i chabry jako rzadkie kropki w pasie 2–3 m od miedzy (rodzaj `blade` z czerwoną i niebieską głową).
  Opcjonalnie, na „konwencjonalnym” polu brak.

---

## 3. Objętość i światło łanu

Wszystko w `grass.vert/.frag` (oba renderery). Bez nowych passów.

1. **AO w głębi łanu**: `ao = exp(−k·LAI·(1 − t))` zamiast liniowego `mix(0.7, 1, t)`. `LAI` (indeks liści) pochodzi z gęstości
   i archetypu: łan zbóż 3–5, ściernisko ~0,3. Nasada łodyg w gęstym zbożu jest naprawdę ciemna i to robi wrażenie gęstości.
2. **Cień słońca w łanie** (bez shadow mapy): roślina na wysokości `h` jest zacieniona przez łan nad nią w kierunku słońca,
   `shade = exp(−k·LAI·(H − h)/(H·max(sin(elev), 0.1)))`. Przy niskim słońcu kłosy są jasne, a dół ciemny. Do tego zostają cienie
   obiektów i terenu (`shadow_tone`).
3. **Przeświecanie**: człon wsteczny `transmission · pow(max(dot(−V, L), 0), 4) · (1 − ao)`. Zielone zboże i kukurydza świecą
   zielonożółto, dojrzałe zboże świeci złoto pod zachodzące słońce. To najbardziej rozpoznawalny widok łanu z kabiny i dziś go nie ma.
4. **Połysk ślizgowy** (`sheen`): nalot woskowy zielonej pszenicy i ości jęczmienia dają przy kątach ślizgowych jasny, lekko
   niebieskawy połysk. Gdy wiatr kładzie łodygi, połysk zmienia się falami: to są „fale na zbożu”.
5. **Fale wiatru**: dziś podmuch to jedna sinusoida po polu. Proponuję dwie składowe (długość 8–15 m i 30–60 m), prędkość z
   wiatru i amplitudę zależną od archetypu (zboża dojrzałe najmocniej, kukurydza i ściernisko prawie nic). **Ta sama funkcja**
   idzie do shadera łanu dalekiego (pkt 4), żeby fale przechodziły przez granicę trawy bez skoku.
6. **Normalne**: łodyga i kłos z normalną zaokrągloną w poprzek (jak walec), a nie w 80% normalna gruntu. To daje relief
   pojedynczych kłosów z bliska. Dla `blade` (łąka) zostaje jak jest, bo tam to celowo uspokaja migotanie.

---

## 4. Pole z daleka: łan jako bryła

To punkt, który z pociągu daje najwięcej. Pociąg widzi pola od 20 m do 1–2 km, a trawa 3D kończy się na 70 m.

### 4.1 Szybka poprawka istniejącego LOD (osobno, od razu)

- Szerokość kompensuje **całe** przerzedzenie, a nie jego pierwiastek: `width /= pow(lod_thinning, 0.9)` z limitem szerokości,
  np. 6× bazowej. Pokrycie zostaje wtedy stałe do początku zanikania.
- `thinning` zależny od rodzaju, nie 0,2·distance: dla zbóż ok. 60 m (pkt 9).
- **Zasięg per rodzaj** (`reach`): zboża i kukurydza do 200–250 m, łąki zostają przy 60–70 m. Kafle zbóż daleko mają 1 segment
  i kłos jako poszerzony koniec, czyli 1–3 trójkąty na roślinę.

### 4.2 Łan daleki: siatka na obrys pola

Dla każdego pola z uprawą wyższą niż ~0,3 m streamer buduje w czasie działania (nic nie zapisujemy) **powłokę łanu** z
`surface_patch.outline`:

- **Wierzch** na wysokości łanu (`height · faza`) nad terenem, w siatce 2–4 m podążającej za terenem. Uwrocia idą tak samo, a
  ścieżki technologiczne to wąskie obniżenia (ciemniejsze pasy, jak dziś w teksturze).
- **Ściana** wzdłuż obrysu, od ziemi do wierzchu, z teksturą boku łanu: łodygi, kłosy i postrzępiona górna krawędź z alpha to
  coverage (już mamy A2C). **Ściana zboża przy miedzy, drodze i torze** to element, którego dziś brakuje najbardziej.
- **Shader łanu**: widok zależny od kąta. Pod kątem ślizgowym kolor **kłosów** (bez ziemi), stromo z góry więcej głębi i ziemi.
  Tani parallax 2–3 warstw szumu daje „kłosy” z ruchem fal z pkt 3.5, a przeświecanie i sheen to te same wzory co w trawie.
  Kolor z tego samego modelu łanu co trawa (pkt 7.1), żeby granica nie była widoczna.
- **Przejście**: od `reach − 20 m` do `reach` trawa zanika ditherem, a powłoka pojawia się tym samym ditherem (komplementarna
  maska). Powłokę tniemy też blisko kamery (np. < 40 m), żeby nie wchodziła w kabinę przy polu tuż przy torze.
- Koszt: pole 300×200 m to ok. 15–30 tys. trójkątów wierzchu (siatka 2–4 m) + kilkaset na ścianę. Pola dalekie dostają rzadszą
  siatkę. Cieni z shadow mapy: tak dla ściany i wierzchu, bo pole rzuca cień na drogę przy niskim słońcu.

Alternatywą jest raymarching wysokości łanu w `mat_terrain` bez nowej geometrii. Odrzucam ją: nie daje sylwetki ściany
na krawędzi pola, a to właśnie ją widać z pociągu.

---

## 5. Ścierniska i stany po zbiorach

Każde ściernisko to **kilka rodzajów naraz** (dlatego 8 rodzajów na chunk) + tekstura gleby z resztkami.

### 5.1 Ściernisko zbożowe

- **Kikuty** (`stubble`): 12–18 cm, ok. 450/m² w rzędach co 12,5 cm, przechył 5–15°, jasne ścięcie. Kolor bladej słomy, nie
  złota pszenicy (pkt 7).
- **Słoma leżąca** (`straw`): po kombajnie z sieczkarnią równo rozrzucona, a bez sieczkarni **wały słomy** (pokosy) co szerokość
  hedera 6–9 m, szerokie na 1–1,5 m i wysokie na 0,3–0,4 m, wzdłuż rzędów. Wały to pasy dużej gęstości `straw` (z daleka
  powłoka niska jak w pkt 4) liczone jak ścieżki technologiczne z `angle`/`drift` (nowy klucz `swath=7.5` w `surface_patch.field`).
- **Bele** okrągłe Ø 1,2–1,5 m na liniach wałów: modele z istniejącego rozsiewania (`editorScatter`), opcja „zbelowane”, kiedy
  wałów już nie ma.
- **Fazy**: świeże (sierpień, jasne, słoma złotawa) → po 2–4 tygodniach **szarzeje** i pojawiają się **samosiewy i chwasty**
  (krótkie zielone `blade`, gęstość rośnie z czasem) → **podorywka/talerzowanie** (60% gleby, pocięta słoma; nowy preset) →
  orka (istnieje).
- Tekstura: nowa z rzędami 12,5 cm (48 rzędów na kafel 6 m) zamiast `farm00` z młodymi rzędami zielonego.

### 5.2 Ściernisko po kukurydzy

Najczęstszy widok **od listopada do marca**, czyli teraz.

- Rzędy **75 cm**, kikuty 20–35 cm, Ø 2–2,5 cm, 7–9/m², ścięte skośnie, z poszarpanym końcem (`stubble` z grubością i 6-kątnym
  przekrojem z bliska).
- Resztki (`residue`): pocięte liście i łodygi, które przykrywają 50–80% gleby, szarobeżowe. Przez zimę szarzeją i ciemnieją
  (faza z daty).
- Tekstura: nowa, gleba z resztkami i rzędami 75 cm (8 rzędów na kafel 6 m), kierunek z pola.

### 5.3 Ściernisko po rzepaku

- Kikuty zdrewniałe 25–35 cm, rzadkie, rozgałęzione na końcu, szarobrązowe, z ciemną sieczką łuszczyn.
- W sierpniu i wrześniu **zielony kobierzec samosiewu rzepaku**, bardzo charakterystyczny. To faza z daty lub preset.

---

## 6. Kukurydza

### 6.1 Roślina

Rzędy 75 cm, w rzędzie co 16–19 cm (7–9 roślin/m²). Wysokość do **2,5–3,2 m**, 12–15 liści po 70–100 × 8–10 cm, łukowato
wygiętych, naprzemiennie (180° ± 30°). Na górze wiecha, a kolba w liściach okrywowych na 1–1,3 m.

### 6.2 Rysowanie: archetyp `corn` w tym samym potoku trawy

Proceduralnie w `grass.vert`, bez modelu z zewnątrz. Roślina to łodyga (walec 6 trójkątów) + N liści. Liść to pasek 4 segmentów
wygięty parabolą z opadającym końcem, z szerokością od nasady do czubka, a od blisko do daleko N spada z 12 do 3. Paski łączymy
zdegenerowanymi trójkątami w jeden strip na instancję, jak dziś źdźbło. **Ok. 90 trójkątów** na roślinę × 8/m² ≈ 720
trójkątów/m², czyli tyle co dzisiejsze zboże (180 × 5). Rzędy są obowiązkowe (pkt 2.3). Przeświecanie liści kukurydzy pod
słońce jest bardzo silne i trzeba je mieć od początku.

Alternatywa: model `.e3d` z LOD-ami od modelarzy, instancjonowany. Ładniejszy z bardzo bliska, ale wymaga assetu i osobnej
ścieżki instancji. Proponuję zacząć od proceduralnego, a model ewentualnie później jako LOD0.

### 6.3 Fazy

| Faza | Kiedy | Wygląd |
|---|---|---|
| Wschody (VE–V4) | V | kropki zielonych roślin 10–30 cm w rzędach na glebie (tekstura gleby + `corn` z małym N) |
| V8–V12 | VI | do kolan i pasa, rzędy się zamykają |
| Wiechowanie (VT/R1) | VII | 2,5–3 m, ciemna soczysta zieleń, jasne wiechy |
| Dojrzewanie (R3–R5) | VIII–IX | dolne liście żółkną, kolby odstają |
| Sucha (R6, na ziarno) | X–XI | beżowobrązowa, liście zwisają, sylwetka „szeleszcząca” |
| Zbiór na kiszonkę | początek IX | → ściernisko świeże z zielonymi resztkami |
| Ściernisko | XI–IV | pkt 5.2 |

Daleko: ta sama powłoka co dla zbóż (pkt 4), **ściana kukurydzy 2,5–3 m** z teksturą liści. Przy przejazdach kolejowych
kukurydza naprawdę zasłania widok, co jest miłym detalem do symulacji widoczności.

---

## 7. Kolory: przegląd i nowe wartości

Pełna paleta (obecnie i propozycja, z kolorami części): `pola-uprawy-kolory.png`.

### 7.1 Zasada: kolor części ≠ kolor łanu

Dziś `uprawy.py` liczy kolor tekstury jako `0,35·base + 0,65·tip`, a trawa ma te same `base`/`tip` jako albedo pojedynczego
źdźbła. Proponuję rozdzielić:

- **Albedo części** (nasada, szczyt łodygi, kłos) to wartości jak dla pojedynczej rośliny, w presetach.
- **Kolor łanu** (tekstura pod trawą i shader łanu dalekiego) liczy się z **modelu łanu**: udział widocznych części pod kątem z
  kabiny (kłosy ~45%, w kwitnącym rzepaku ~80%, wiechy kukurydzy ~8%), przyciemnienie z AO w łanie (pkt 3.1) i udział
  widocznej gleby (ozimina, ścierniska). Jedna funkcja `canopy_colour()` w Pythonie (`uprawy.py`) i jej odpowiednik w shaderze.
- Wiarygodność fizyczna: łan dojrzały ma w świetle widzialnym liniowe albedo ok. 0,25–0,35, a łan zielony G ok. 0,15–0,2.
  Pszenica ma dziś liniowe R 0,55, stąd „jarzenie się” pola w słońcu przy AgX. Wartości „widziany” w tabeli niżej i na palecie
  to mieszanka części **przed** przyciemnieniem w łanie. Z AO ok. 0,7 (pkt 3.1) pszenica schodzi do liniowego R ok. 0,33,
  czyli w ten przedział.

### 7.2 Uwagi do obecnych wartości

| Uprawa | Obecnie (widziany) | Problem | Propozycja (widziany) |
|---|---|---|---|
| Ozimina | `#487620` | limonka, S 73%; jesienią widać glebę | `#556639`, ciemniejsza, sinawa, 25% gleby |
| Zboże zielone | `#4B7822` | za nasycone, brak niebieskiego (nalot woskowy) | `#5C7A42`, C\* 51 → 35 |
| Kłoszenie | `#839040` | ok, lekko za żółte | `#79884B`, kłos dojrzałości mlecznej `#999E61` |
| Pszenica | `#C4A557` | za jasna jak na łan | `#B8955A`, kłos ochrowo-złoty `#CCA15C` |
| Jęczmień | `#D0BD80` | ≈ owies | `#C9B782`, ciepły, jedwabisty (ości + sheen) |
| Żyto | `#AD9D6E` | za żółte | `#9A9478`, szarobeżowe z zielonkawym odcieniem, najwyższe |
| Owies | `#BEB681` | ≈ jęczmień (ΔE 6,8) | `#B7B890`, zielonkawo-biały, najniższa chroma (ΔE do jęczmienia 10,8, do pszenicy 22) |
| Rzepak kwitnący | `#B8B122` (trawa) vs cytrynowa tekstura | trawa oliwkowa, inna niż tekstura | `#D0C125` z jednego modelu, kwiat `#F2D924` |
| Rzepak dojrzały | `#7A6C3A` | ok | `#796A43` |
| Ściernisko | `#C2AB6E` | ≈ pszenica, bez gleby | `#AE9F78`, blada szara słoma + gleba, ścięcia `#D6C799` |
| Kukurydza | brak | | VI `#4C732D`, VIII `#4B692C` (wiecha `#B8A36B`), X `#9F8D69`, ściernisko `#7D7059` |

Główna oś rozróżnienia zbóż: **pszenica** jest najcieplejsza i najbardziej nasycona (ochra), **jęczmień** jasny i ciepły,
**owies** zielonkawo-biały i najbledszy, **żyto** szare i najwyższe. Kształt kłosa (pkt 2) robi resztę.

### 7.3 Kalibracja

- Porównanie zrzutów z kabiny (stała kamera 3,5 m, słońce 15°, 35° i 55°, pochmurno) ze zdjęciami pól z kabiny przy podobnym
  świetle. Pipeta na zrzucie, `gfx.postfx.grade.*` neutralne (vibrance 0, saturation 1).
- Ortofoto (`editorOrthophoto`) przydaje się tylko do **tekstur z góry** (rzędy, wały, ścieżki). Kolor z kabiny jest inny
  (kąt, BRDF łanu), więc nie kalibrujemy nim koloru łanu.
- AgX celowo spłaszcza nasycenie świateł. Nie nadrabiamy tego nasyceniem albedo. Jeśli całość ma być żywsza, robimy to gradem.

---

## 8. Kalendarz upraw (faza z daty)

Pole przechowuje **uprawę**, a nie stan: `crop=pszenica_ozima` w `surface_patch.field`. Faza wynika z dnia roku
(`simulation::Time.year_day()`) + losowe przesunięcie pola ±5 dni + opcjonalne przesunięcie regionu. Dzisiejsze presety stają
się parą (uprawa, faza) z opcją „zamroź fazę” w edytorze, więc stare scenerie wyglądają jak dziś.

| Uprawa | Ozimina / wschody | Strzelanie w źdźbło | Kłoszenie | Dojrzała | Zbiór | Potem |
|---|---|---|---|---|---|---|
| Pszenica ozima | X–III | IV–V | VI | VII | 20.VII–15.VIII | ściernisko VIII–IX, podorywka/orka IX–X, siew X |
| Jęczmień ozimy | X–III | IV | V | VI | 25.VI–15.VII | jw. |
| Żyto | X–III | IV | V | VII | VII | jw. |
| Owies (jary) | IV | V | VI | VII–VIII | VIII | jw. |
| Rzepak ozimy | VIII–III (rozety) | IV | kwitnie 25.IV–20.V | VI–VII | 10–25.VII | ściernisko z samosiewem VIII–IX |
| Kukurydza | wschody V | VI | VII | VIII–X | kiszonka IX / ziarno X–XI | ściernisko XI–IV |

Przejścia między fazami są płynne. Kolory i wysokość interpolujemy przez ~2 tygodnie, z nierównym dojrzewaniem (pkt 2.4).
Zbiór jest skokowy, ale per pole w losowym dniu z okna, więc nie wszystkie pola znikają jednego dnia.

---

## 9. Wydajność: nie jest ograniczeniem

Pomiar (17.11): obecne zboże na **1 ha w pełnym oświetleniu kosztuje ok. 4 FPS**. Zapasu jest więc dużo i przy wyborze między
jakością a kosztem **wybieramy jakość**:

- **Gęstość realna od razu**: pszenica 550–650 pędów/m², jęczmień 700, żyto 500, owies 450, a nie 300–450 z kępowaniem dla oszczędności.
  Gdyby gdzieś było za ciężko, obsadę zmniejsza się per pole parametrem `dens=` (pkt 2.3) bez ruszania palety.
  Kępy zostają, ale dla wyglądu (krzewienie), nie dla kosztu.
- **Pełna geometria dalej**: kłos, ości i wiecha do ok. 40 m, uproszczony kłos do `reach`.
- **`reach` zbóż i kukurydzy 200–250 m** (nie 120–150). Powłoka łanu (pkt 4.2) przejmuje dopiero dalej, więc przejście jest
  daleko od toru i praktycznie niewidoczne.
- Rzednięcie LOD tylko tam, gdzie i tak nie widać różnicy (od ok. 60 m), z pełną kompensacją pokrycia (pkt 4.1).
- Nadal sensowne, bo nic nie kosztują: archetypy w osobnych drawach (stała liczba wierzchołków), hash liczony raz na kępę.
- `gfx.grass.density` i `gfx.grass.crops` (zasięg zbóż) zostają jako suwaki dla słabszych kart. Ustawienia domyślne celujemy w
  jakość, nie w oszczędność.
- Kontrola: licznik trójkątów trawy jest (`triangles()`), więc po E2 i E4 mierzymy ten sam hektar i scenę z selftestu.

---

## 10. Kolejność wdrażania

Każdy etap to osobny build `ficzery2`, a potem osobny PR do 3.0 według kolejności z `roadmap.md` (uprawy po działkach).
Zmiany w shaderach zawsze w parze `vk14` + `ogl46`. Kod pisany pod reguły Sonara z `CLAUDE.md` (S134, S3776 w `prepare()`
przy rozdzielaniu archetypów).

| Etap | Zakres | Efekt |
|---|---|---|
| **E1** (szybki) | pkt 4.1 (pokrycie LOD, `thinning` i `reach` per rodzaj), pkt 3 (AO w łanie, cień w łanie, przeświecanie, sheen), paleta z pkt 7 + `canopy_colour()` w `uprawy.py` | pole przestaje być „cienkie” i płaskie bez nowej geometrii |
| **E2** | archetypy zbóż i rzepaku (pkt 2.1–2.2), tabela 4×8, 8 rodzajów na chunk, zapis/odczyt YAML, obsada jako parametr pola `dens=` (razem z realną gęstością) | owies odróżnia się od pszenicy, jęczmień od żyta |
| **E3** | rzędy i kępy (pkt 2.3), tekstura kierunku rzędów, nierówne dojrzewanie, wyległe placki, chwasty brzegu | pole wygląda na siane, a nie posypane |
| **E4** | powłoka łanu dalekiego + ściana (pkt 4.2) | zboże ma wysokość do 1–2 km, ściana łanu przy torze |
| **E5** | ścierniska: zbożowe, kukurydziane, rzepakowe; słoma, wały, bele, podorywka (pkt 5) | krajobraz po żniwach i zimą |
| **E6** | kukurydza: archetyp `corn`, fazy (pkt 6) | |
| **E7** | kalendarz upraw (pkt 8) | pole samo zmienia się z datą scenariusza |

Testy: w `editormodeselftestterrain` scena porównawcza. Kamera na wysokości kabiny, pola każdej uprawy w odległości 10, 50, 200 i
800 m, trzy wysokości słońca, zrzut każdego przypadku. Do tego testy jednostkowe na `canopy_colour()`, kalendarz (dzień → faza)
i rozkład archetypów w `prepare()`.

---

## 11. Do ustalenia z Tobą

1. ~~Budżet trójkątów~~: rozstrzygnięte, nie ogranicza (1 ha zboża ≈ 4 FPS), patrz pkt 9.
2. **Kukurydza**: proceduralnie (propozycja) czy model `.e3d` od modelarzy?
3. **Powłoka łanu** budowana w czasie działania z obrysów (propozycja, nic nie zapisujemy) czy zapisywana z polem?
4. **Kalendarz** domyślnie włączony dla nowych pól, czy tylko na życzenie?
5. **Zmiany w `grass.vert` a gałąź Hirka** z malowaniem trawy w shaderze: archetypy i nowa tabela dotykają tego samego pliku,
   więc trzeba to uzgodnić przed E2, żeby nie rozjechać się przy scalaniu.
6. Czy bele i wały robić już w E5, czy osobno razem z ZIELSKO/rozsiewaniem obiektów.

---

## Co jest wdrożone (17.11.2026, `feat/uprawy`)

- **Archetypy roślin** (`heightmap::grass_archetype`, `terrain.yaml`: `archetype`, `head`, `head_length`, `head_width`, `head_droop`, `awns`,
  `translucency`, `sheen`, `reach`; pisane tylko gdy różne od zera, więc paleta samej trawy zapisuje się jak dotąd). Geometria w
  `shaders/{ogl46,vk14}/grass_plant.glsl` (jeden plik dla obu rendererów): łodyga, kłos łukiem wg `head_droop`, ości, karty wiechy i
  kwiatów doklejane zdegenerowanymi trójkątami. Kafel rysuje tyle wierzchołków, ile potrzebuje najbogatsza roślina jego rodzajów
  (`grass_field::plant_vertices`, ta sama funkcja w shaderze), reszta roślin powtarza swój ostatni wierzchołek.
- **Łan** (`grass_canopy.glsl`): AO w głębi łanu (w wierzchołku), cień słońca w łanie zależny od wysokości słońca, przeświecanie
  pod słońce, połysk ślizgowy. Dla archetypu `blade` wszystko zero, więc trawa wygląda jak przedtem.
- **LOD per rodzaj** (`kind_lod` w `grassfield.cpp`): rodzaj z `reach` ma rzednięcie od 0,27·reach, zanikanie w ostatnich 10%
  i szerokość kompensowaną potęgą 0,9 (trawa: wartości klatki i 0,5 jak dotąd). Tabela rodzajów 4×9 (VK) / uniformy 24–39 (GL).
  `max_blades` 262 144. `gfx.grass.crops` (domyślnie 1) mnoży zasięg upraw.
- **Presety upraw** z kolorami z pkt 7, obsadą (`dens=`), przycisk odświeżenia palety. Kukurydza, rzędy, łan daleki,
  ściernisko kukurydziane i kalendarz: nadal etapy E3–E7.
- **Do zrobienia w SVN**: przegenerować `textures/uprawy/*.png` skryptem `tools/zielsko/uprawy.py` (nowe `canopy_colour()`:
  zboża, rzepak kwitnący, ściernisko). Bez tego kod działa, tylko pole za zasięgiem roślin ma kolor starej tekstury.
