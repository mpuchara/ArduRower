# Duch, nagroda i automatyczna pauza: 0.09.24

> Od 0.09.27 trening korzysta z [nowego pomiaru dystansu](PULSE_DISTANCE_0.09.27.md). Rekordy, przebiegi tętna i medale sprzed tej wersji pozostają w NVS, a nowe wyniki tworzą oddzielny zestaw.

## Obsługa dotykiem

- Normalny start nadal otwiera zwykły trening z polem TEMPO /500m.
- Dotknięcie pulpitu otwiera wybór 500/1000/2000 m. Duży dolny przycisk „Zwykły trening — wyjdź” wraca na zwykły pulpit.
- Dotknięcie pulpitu aktywnego ducha otwiera trzy duże przyciski: kontynuuj, zmień dystans, zwykły trening. Otwarcie opcji samo nie anuluje przejazdu; zmiana dystansu i wyjście anulują niedokończoną próbę. Wyjście zachowuje bieżące metryki treningu i przywraca pole tempa.
- Ekran wyniku ma osobne przyciski nowego wyścigu/powtórki oraz powrotu do zwykłego treningu. Obsługa nie wymaga dostępu do BOOT.

## Nagroda

Pobicie istniejącego ducha wyświetla złoty puchar, numer medalu oraz przewagę w sekundach. Licznik zwycięstw jest zapisywany osobno dla każdego dystansu i wczytywany po restarcie. Remis, przegrana, anulowanie, przepełnienie próby i pierwszy przejazd bez przeciwnika nie przyznają medalu. Przy błędzie zapisu nowego rekordu medal nie jest przyznawany. Błąd zapisu samego licznika jest raportowany; ekran nie udaje, że numer medalu został utrwalony.

## Pauza

Po 5 sekundach bez impulsów kończy się okres wybiegu. Pojawia się AUTO PAUZA, prędkość i kadencja wynoszą zero, czas aktywny oraz dystans pozostają stałe. Pierwszy prawidłowy impuls automatycznie wznawia trening, bez doliczania czasu przerwy. W czasie pauzy nadal działa pętla BLE, odbiór HR i obsługa dotyku.

Duch, różnica dystansu, próbkowanie rekordu i wynik wyścigu korzystają z tego samego zegara aktywnego co licznik treningu. Wykresy bieżące nie dopisują próbek podczas pauzy. Opóźniony znacznik czasu z kolejki ISR nie jest interpretowany jako wielominutowy skok. Telemetria z modelu ruchu nie może nadpisać zerowej prędkości/kadencji w stanie pauzy.

To zmienia zasady porównania rekordów. Format wyścigu ma wersję 3, a nowe klucze NVS to g3_500/g3_1000/g3_2000 i odpowiednie ślady HR oraz liczniki w3_*. Starsze rekordy pozostają pod wcześniejszymi kluczami, ale nie są przeciwnikami w wyścigach czasu aktywnego. Pierwszy przejazd w nowych zasadach ustanawia nowy punkt odniesienia.

## Sprawdzenie

- 11 testów C++ przeszło: dodatkowy test łączy rzeczywiste klasy modelu ruchu, metryk i ducha. Sprawdza wybieg, 30-sekundową pauzę, stałość czasu/dystansu/pozycji ducha, wznowienie, opóźnione próbki i blokowanie niezerowej telemetrii podczas pauzy.
- Testy wyników obejmują pierwszy rekord, zwycięstwo, remis, przegraną, anulowanie, przepełnienie oraz odrzucenie rekordu wersji 2.
- 6 sprawdzeń Python przeszło, w tym niezmieniony format FTMS, buforowanie renderera, nawigacja i zgodność wersji instalatora.
- Build ST7789/GPIO21 zakończony sukcesem. Na COM6 zapisano tylko aplikację od 0x10000; esptool potwierdził hash zapisu. Przed zmianą zachowano prywatną kopię obszaru NVS 0x9000..0xDFFF. Bez kasowania pamięci.
- Boot urządzenia potwierdził 0.09.24, wczytaną kalibrację dotyku i działającą reklamę FTMS. Log fizycznego urządzenia zarejestrował pauzę przy activeMs=5000, wznowienie nadal przy activeMs=5000, a następnie kolejną pauzę przy activeMs=11752 i dystansie 0,80 m. Kolejne odczyty pozostały stałe.
- 31.10.2026 użytkownik potwierdził fizyczną próbę: wyjście z menu do zwykłego treningu, wybór 500 m i trzy przyciski opcji ducha oraz automatyczną pauzę po przerwie i poprawne wznowienie. Odpowiedź: „tak, wszystko działa według założeń”.
- Ta próba nie obejmowała zgłoszonego ukończenia wyścigu i pokonania istniejącego ducha. Warunki przyznania medalu są sprawdzone testami logiki; ekran pucharu i utrwalenie medalu po realnym zwycięstwie pozostają do fizycznego potwierdzenia.
