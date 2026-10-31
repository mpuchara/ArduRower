# Garmin: działające FTMS i HR oraz układ pola

## Potwierdzenie fizyczne komunikacji

ESP32 CYD 0.09.23 na COM6 oraz pole Connect IQ 0.1.4 na fēnixie 6X Pro Solar (006-B3291-00, firmware 28.02) zostały sprawdzone razem. Użytkownik potwierdził zarówno odbiór danych wioślarskich na zegarku, jak i aktualny puls na ESP32, a następnie płynną aktualizację obu kierunków podczas próby z wiosłowaniem.

Log `device-watch-bridge-014.log` zawiera prawdziwe zapisy HR od adresu zegarka na ACL=0 i postęp FTMS na tym samym ACL. Kolejne widoczne wartości obejmują 81, 83, 85, 93 i 92 BPM, przy rosnących licznikach odebranych próbek i wysłanych FTMS. Nie są to wartości READ z cache ani symulowane pomiary. Jedyny laptopowy test charakterystyki zwrotnej wysłał jawny brak pomiaru (BPM=0), co w logu ma osobny adres laptopa i nie stanowi dowodu tętna z zegarka.

Ostateczna ścieżka nie wymaga osobnego nadawania HR przez Garmin 180D. Pole pobiera aktualny HR aktywności i zapisuje prywatną charakterystykę na istniejącym połączeniu FTMS. W normalnym starcie ESP32 nie uruchamia osobnego klienta/skanera HR.

Walidacja implementacji: 10 testów natywnych C++, 6 sprawdzeń Python i 5 testów Monkey C w symulatorze. Testy obejmują m.in. format FTMS, odrzucanie starych i niepoprawnych pakietów HR, przepełnienie sekwencji, porównanie nazwy urządzenia oraz ujemny licznik czasu systemowego. Testy hosta i symulatora są odrębne od powyższego potwierdzenia fizycznego.

## Układ pola 0.1.5, 31.10.2026

- Motyw przez `getBackgroundColor()` i kontrastowy kolor tekstu.
- Wyłącznie mała czcionka `FONT_XTINY`, bez nagłówka i długich statusów.
- Jeden, dwa lub trzy wiersze wybrane według rzeczywistej wysokości czcionki i pola. Tekst jest mierzony przed rysowaniem.
- Natywny podgląd symulatora dla fēnixa 6X: pola szerokości 120 px i wysokości 70, 48 oraz 26 px, osobno czarne i białe tło. Oględziny potwierdziły rozdzielenie wierszy i brak nakładania tekstu.
- Build SDK 9.3.0: `BUILD SUCCESSFUL`. Wgrano przez MTP i sprawdzono odczyt zwrotny. SHA-256 pliku PRG: `8E3311780C012C727F28BE481EFAB1FE658F2ACB6D6AB9AAAB3ABE2B0E30CAD0`.
- Zmiana 0.1.5 dotyczy prezentacji; działający kod komunikacji z wersji 0.1.4 pozostał bez zmian. Nie flashowano ponownie ESP32 na potrzeby poprawki wyglądu.
- Oględziny po odłączeniu USB na fizycznym zegarku są osobnym krokiem użytkownika; wynik podglądu nie zastępuje tego potwierdzenia.

Dotyk został wcześniej skalibrowany i przyjęty do wyboru 500 m. Dalsze fizyczne testy pełnego wyścigu z duchem i jego wykresów pozostają odłożone na wyraźną prośbę użytkownika.

Starsze eksperymenty z zachowaniem poprzedniego pola: `BLUETOOTH_COEXISTENCE_PL.md` i `CYD_0.09.17_VALIDATION.md`.
