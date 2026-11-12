# ArduRower 0.09.35 — Reed Signal Filter and Adaptive Stroke Cycle Detection

Bazą jest commit `83937efb531fd748eb5a2ddd2eaa6a2517715a65` (0.09.34). Zdalny main i lokalny kod wskazywały tę samą wersję przed rozpoczęciem prac.

## Przyczyna i zakres zmiany

W 0.09.34 dystans otrzymywał zbocza FALLING co najmniej co 10 ms, a detektor ruchów co 15 ms. W szybkich próbach wcześniejszy próg 15 ms pomijał prawidłowe impulsy. Detektor zmieniał fazę po pojedynczym długim odstępie albo krótkim spowolnieniu potwierdzonym następnym odstępem. Potrafił przełączyć fazę nawet po odrzuceniu naliczenia ruchu. Porównywał też odstęp ze sprzed czterech przyjętych impulsów jako odpowiadający temu samemu magnesowi, choć po nawrocie i zgubionym zboczu takie przypisanie jest niepewne.

Nowe zapisy obu zboczy ujawniły liczne przejścia i powtórne zgłoszenia GPIO oddalone o kilka–kilkadziesiąt mikrosekund. To potwierdza potrzebę filtracji stanu styku. W oznaczonej serii 30 pełnych ruchów użytkownik odczytał 29 naliczeń na starym detektorze. Nowa implementacja odtwarza tę serię jako 30. Podwójne naliczanie było zgłoszone z innych treningów. Nowa warstwa kontaktu i potwierdzanie całego cyklu zabezpieczają oba kierunki błędu.

## Akwizycja i filtr kontaktu

ISR reaguje na CHANGE na GPIO22 i zapisuje timestamp oraz poziom wejścia do kolejki. Obliczenia i zapis logów odbywają się w głównej pętli. Kolejka ma 512 miejsc, z których 511 może być zajętych. Licznik przepełnień jest jawny. Po utracie ciągłości firmware odrzuca niedokończony kontakt i synchronizację faz, zachowując dotychczas naliczone ruchy i dystans.

Filtr wymaga stabilnego zamknięcia przez 300 µs oraz stabilnego otwarcia przez 300 µs. Potwierdzone otwarcie publikuje jeden impuls ze znacznikiem czasu początku zamknięcia. Krótkie drgania otwarcia w ramach tego samego zamknięcia nie uzbrajają ponownie czujnika. Samo pozostawanie kontaktronu w stanie zamkniętym nie tworzy kolejnych impulsów.

Parametry dobrano z pomiaru konkretnego CYD. W pierwszych dwóch seriach minimalne czasy potwierdzonych zamknięć wyniosły około 8,2 ms, a otwarć około 0,64 ms. Okresy 13–15 ms pozostają zachowane. Stabilne ponowne otwarcie i zamknięcie przy tym samym magnesie może zostać przyjęte również szybciej niż 10 ms. Detektor cyklu ocenia je w kontekście otaczającego ruchu.

Prędkość, dystans, detektor faz oraz potwierdzenie wznowienia dostają jeden strumień zweryfikowanych impulsów. Starsze wejście timestamp-only w `Engine::onPulse` pozostało do odtwarzania historycznych, już przerzedzonych zapisów.

## Detektor cyklu

`StrokeCycle.h` analizuje sumę czterech kolejnych odstępów jako krótkie lokalne okno prędkości. To okno nie przypisuje zdarzeń do konkretnych magnesów i nie oznacza gwarantowanego pełnego obrotu po nawrocie.

Detektor śledzi najszybszą część fazy. Wzrost okresu okna o co najmniej 15% uzbraja kandydata na spowolnienie. Następny spadek okresu względem szczytu potwierdza ponowne przyspieszenie. Przed uzbrojeniem wymagane jest co najmniej osiem impulsów i 160 ms fazy. Gdy dostępne są wcześniejsze fazy, minimalna liczba impulsów rośnie do 65% mediany ich długości. Ta histereza ogranicza wielokrotne uznawanie jednego nawrotu za kolejne fazy.

Ruch nalicza się przy potwierdzonym zakończeniu DRIVE. Kolejny ruch wymaga wcześniej potwierdzonego RETURN i ponownego DRIVE. Zbyt wczesny kandydat nie przełącza fazy. Minimalny odstęp pełnego cyklu uwzględnia 48% mediany ostatnich pięciu cykli, z dolną granicą 650 ms. Kadencja pochodzi z mediany okresów rzeczywiście zatwierdzonych ruchów.

Diagnostyczna prędkość mechaniczna ma jednostkę przejść magnesów na sekundę. Jej filtr 80 ms i przyspieszenie służą obserwacji. Detektor analizuje okresy kontaktu, niezależnie od wolniejszego filtra prędkości wirtualnej łodzi.

## Wyniki odtwarzania

Zanonimizowane surowe CSV są w `tests/fixtures/*.csv.gz`, a ręczne oznaczenia w `reed_sessions.json`. Zapisy diagnostyki początkowej miały wtrącone zwykłe komunikaty statusu pomiędzy fragmentami CSV. Usunięto te komunikaty przed wyodrębnieniem kompletnych rekordów R. Obie sesje zgłosiły zero utraconych zdarzeń akwizycji i zero przepełnień bufora logów. Docelowy logger wysyła całe wiersze.

| Oznaczona seria | Rzeczywiste ruchy | Detektor 0.09.34 | Nowy detektor | Dystans z okna, oba modele |
|---|---:|---:|---:|---:|
| Spokojna | 10 | 10 | 10 | 69,069 m |
| Szybsza | 10 | 10 | 10 | 72,289 m |
| Zmienne tempo | 30 | 29 | 30 | 207,045 m |

Różnica szczytowej prędkości w porównaniu na tych samych surowych zdarzeniach jest poniżej 0,05 km/h. Dystans na każdym oknie zgadza się w granicy 0,002 m. Oznacza to także zgodność średniej prędkości i równoważnego tempa /500 m dla tego samego czasu okna. To porównanie zapisów, bez dodatkowej próby fizycznych 500 m.

Zachowano starszy zapis 10 ruchów z `cyd_motion_test.cpp`. Nowy detektor nadal daje 10/10. Testy trajektorii zwijacza dają 30/30 przy 20, 30, 50 i 75 SPM oraz różnych pozycjach nawrotu względem nierówno umieszczonych magnesów. Przejście 20→30→45→25 SPM daje 60/60. Wstrzyknięte drgania nie zwiększają liczby ruchów ani dystansu. Testy obejmują też brakujące zbocze, niepełny ruch, zatrzymany zamknięty kontaktron, szybkie ponowne przejście magnesu, przepełnienie micros(), opóźnienia przetwarzania i przepełnienie kolejki.

## Próby na ergometrze

Na kompilacji testowej z nowym filtrem i detektorem użytkownik wykonał 30 spokojnych pełnych ruchów i potwierdził 30 na ekranie. Następnie potwierdził prawidłowy wynik szybszej próby oraz działanie urządzenia. Podczas szybszej próby ergometr przesunął się i przewody zostały rozłączone. Log USB urwał się przy 54 ruchach łącznie, czyli po 24 zarejestrowanych ruchach tej serii, rozpoczętej z 30. Ocenę końcowego wyniku tej próby oparto na odczycie użytkownika.

Log wcześniejszych pomiarów i nowej wersji testowej potwierdzał równoczesne aktualizacje FTMS i HR z Garmina. W kompilacji testowej naprawiono następnie błąd znaku w obliczeniu diagnostycznego filtra przyspieszenia, który wypisywał `nan` lub `inf`. Nie był on wejściem decyzji o ruchu. Testy końcowe sprawdzają skończoność tych wartości.

## Zgodność i skala

Zachowano `kMetersPerPulse=0.23`, domyślne `calibration=0.70`, filtr prędkości 3,5 s i dobieg. Dystans nadal ma domyślny krok 0,161 m na przyjęty odstęp. Opóźnienie potwierdzania otwarcia jest uwzględnione w filtrze prędkości.

Auto pauza następuje po ośmiu sekundach. Cztery kolejne zweryfikowane impulsy, z przerwami do 350 ms, potwierdzają wznowienie. W trakcie pauzy czas, postęp ducha i próbki wykresów są zatrzymane.

Format i identyfikatory FTMS oraz kanał HR pola ArduRower HR 015 pozostają zgodne z 0.09.34. Orientacja rotation 1, dotyk i zapis kalibracji są zachowane. `ArduRower/RowerMetrics.h` nie wymagał zmian, więc wariant Heltec zachowuje swoje obliczenia. Ghost Race nadal używa zapisów `g7_*`, `h7_*`, `w7_*`. Zapisane rekordy i medale pozostają w NVS.

Tempo i moc równoważna używają zależności `tempo=500/prędkość_m_s` oraz `W=2.8*prędkość_m_s³`, zgodnych z kalkulatorami Concept2. Czujnik mierzy zwijacz taśmy, więc wartości są estymacją wirtualnej łodzi. [Concept2 Pace Calculator](https://www.concept2.com/training/pace-calculator), [Watts Calculator](https://www.concept2.com/training/watts-calculator).

## Diagnostyka USB

Port działa z prędkością 115200 baud. `pulsetrace` zachowuje dotychczasowy format P. `reedtrace` uzbraja zapis obu zboczy. Zapis trwa 120 sekund od pierwszego potwierdzonego kontaktu, aby oczekiwanie na rozpoczęcie ćwiczenia nie zużywało czasu próby.

```text
R,timestamp_us,edge,level,low_us,high_us,raw_gap_us,emitted,short_low,short_high,decision
F,closure_timestamp_us,low_us,previous_high_us,verified_gap_us
M,timestamp_us,reel_crossings_s,filtered_crossings_s,acceleration,phase,turn_candidate,confidence,stroke_count,spm,rejected_turns
```

R oznacza zdarzenie przed filtracją, F potwierdzone zamknięcie, M decyzje detektora. `raw_gap_us` odnosi się do ostatniego surowego FALLING, a `verified_gap_us` do poprzedniego potwierdzonego impulsu. Decyzje filtra mają kody 0 oczekiwanie, 1 potwierdzenie, 2 krótki LOW, 3 krótki HIGH, 4 powtórzony poziom, 5 nieuporządkowany timestamp, 6 inicjalizacja. Końcowy wiersz REEDTRACE END podaje przepełnienia i liczniki filtra od ostatniego resetu.

Bufor diagnostyki jest ograniczony i wysyła wyłącznie całe wiersze, gdy UART ma miejsce. Przepełnienie diagnostyki zwiększa licznik utraconych wierszy. Przepełnienie kolejki akwizycji ma osobny licznik i wymusza ponowną synchronizację.

## Odtwarzanie CSV

Na Linux/WSL z katalogu repozytorium:

```sh
g++ -std=c++11 -O2 tools/replay_reed.cpp -o /tmp/replay_reed
python3 tools/replay_reed.py zapis.csv.gz --executable /tmp/replay_reed
python3 tests/reed_replay_regression.py /tmp/replay_reed
```

Tryb `--mode raw` odtwarza oba zbocza przez aktualny filtr i detektor. `filtered` przyjmuje rekordy F, a `legacy` odtwarza historyczne bramki 10/15 ms. Opcja `--baseline-executable` pozwala porównać z binarnym odtwarzaczem skompilowanym z nagłówkiem z bazowego commita.

[Instalator i zachowanie NVS](WEB_INSTALL_CYD.md)

## Weryfikacja wydania

Końcowy zestaw lokalny przeszedł z kodem wyjścia 0: 15 testów C++, 6 kontroli Python oraz trzy oznaczone serie CSV porównane z odtwarzaczem bazowego 0.09.34. Kompilacja profilu ST7789/GPIO21 zakończyła się powodzeniem.
