# ArduRower HR: własne pole Connect IQ

Wersja 0.1.6, nazwa na zegarku **ArduRower HR 016**, dla Garmin fēnix 6X Pro / Solar (`fenix6xpro`, sprawdzony numer 006-B3291-00, firmware 28.02). Wymaga ESP32 ArduRower CYD 0.09.23 lub nowszego z kanałem zwrotnym HR.

Pole odbiera Rowing Data z WR S4BL3 i odsyła `Activity.Info.currentHeartRate` zegarka przez to samo połączenie BLE. Nie wymaga nadawania tętna jako osobny czujnik 180D. Odczyt HR jest bieżącym tętnem aktywności; jeśli zegarek używa zewnętrznego czujnika HR, będzie to źródło wybrane przez zegarek.

## Instalacja przez USB

1. Zbuduj `AROWERHR.PRG` dla `fenix6xpro`.
2. Podłącz zegarek w trybie MTP. Skopiuj plik do `Primary/Garmin/Apps`. Zweryfikuj odczyt zwrotny pliku przed odłączeniem.
3. Odłącz USB. W aktywności wioślarskiej usuń wcześniejsze pole BLE z ekranów tej aktywności; może pozostać zainstalowane na zegarku.
4. Dodaj **ArduRower HR 016**. Można użyć małego pola: układ dopasowuje liczbę wierszy do jego wymiarów. Dwa różne pola zarządzające tym samym BLE mogą zajmować zasoby zegarka; usuń wcześniejsze pole BLE z ekranów tej aktywności.
5. Otwórz ten ekran przy włączonym komputerku. Przy braku pomiaru HR wysyłany jest jawny brak wartości, nie poprzednie BPM.

Tło jest pobierane z `DataField.getBackgroundColor()`, tekst pozostaje w `FONT_XTINY` i jest mierzony przed narysowaniem. Liczba wierszy (1–6) zależy od **rzeczywistej** wysokości pola. W większym polu widać dystans, tempo /500 m, prędkość w km/h, własny licznik ruchów, czas aktywny ergometru, kadencję i puls. W małych polach w pierwszej kolejności zostają dystans i tempo. Prędkość na zegarku jest wyliczona z chwilowego tempa FTMS, nie z GPS ani z tempa aktywności Garmina.

### Dodatkowe metryki Garmin Connect (0.1.6)

Pole zapisuje przez FitContributor **osobne metryki Connect IQ**: dystans [m], prędkość [m/s], kadencję [spm], opcjonalną moc równoważną [W], ważność BLE, liczbę ruchów [strokes], czas aktywnego ruchu [s], średnie tempo [s/500m] i średnią kadencję [spm]. Ostatnie cztery odczytuje z **istniejących** bajtów FTMS 0.09.36 (bez zmian w firmware ESP32). Prędkość w km/h jest dodatkowo wyświetlana w polu, a do FIT trafia w m/s.

**Ograniczenie Garmina:** te pola stanowią dane deweloperskie w FIT, a `nativeNum` nie zastępuje natywnego dystansu, prędkości i podsumowania aktywności Garmin Connect. Jeżeli natywny dystans w aktywności „Wioślarstwo w pomieszczeniu” nadal pokazuje 0 m, mimo poprawnego wykresu „Rower distance” w Connect IQ, nie jest to awaria komunikacji CYD. Natywny zapis wymaga innej ścieżki (np. zgodnego czujnika/protokołu ANT+ akceptowanego przez dany zegarek lub późniejszej świadomej obróbki FIT). Sam ESP32 nie nadaje ANT+ bez odpowiedniego dodatkowego sprzętu.

Tętno jest **źródłem Garmina**, odsyłanym do komputerka po tym samym BLE; nie jest sztucznie zastępowane przez dane z czujnika CYD. Moc jest zapisywana tylko, gdy firmware ją naprawdę dołącza (w 0.09.36 jest domyślnie wyłączona). Czas „Rower active time” pochodzi z komputerka, więc może różnić się od czasu aktywności mierzonego przez zegarek. Własna liczba ruchów może różnić się od liczby wyznaczanej przez akcelerometr Garmina.

Przed instalacją 0.1.6 zbuduj `AROWERHR.PRG` za pomocą **dotychczasowego klucza DER** i nie instaluj równolegle starszego pola BLE w aktywności. Zapis FIT działa tylko podczas nagrywania aktywności z dodanym aktywnym polem Connect IQ. Weryfikacja fizyczna wersji 0.1.6 oraz próba podglądu w Garmin Connect wymagają jeszcze wykonania.

## Protokół i ważność

- Jedna rejestracja profilu BLE: standardowa usługa FTMS `1826`, Rowing Data `2AD1` + CCCD `2902` oraz prywatna charakterystyka zapisu `c9f10002-6f50-4fca-9b8d-3f8d90ae1b65`.
- Zapis HR z potwierdzeniem jest uruchamiany także przez odebrany pakiet FTMS, z ograniczeniem do około 1 Hz. Nie zależy więc od tego, czy zegarek już wywołuje `compute()` w danym stanie aktywności. Tylko jedno żądanie oczekuje naraz. Pakiet ma 6 bajtów: wersja `1`, flaga ważności `0/1`, licznik LE16, BPM LE16. Przy nieważnym HR: flaga=0, BPM=0.
- ESP32 sprawdza ACL aktywnego FTMS, format i zakres oraz rosnący licznik z obsługą przepełnienia. Powtórzony albo stary pakiet nie odświeża wskazania. Przy braku nowych pomiarów HR wygasa po 6,5 s. Rozłączenie usuwa HR od razu i resetuje sekwencję.
- W normalnym starcie firmware nie uruchamia już osobnego klienta/skanera HR. Starsze eksperymenty pozostały wyłącznie jako jawne diagnostyki.
- Dekoder jest przeznaczony dla WR S4BL3. Historyczny format ArduRower przesyła tempo jako **całe sekundy /500m**, podczas gdy ogólny FTMS definiuje jednostkę 0,1 s. Zachowano działający format ESP32 i uwzględniono go w polu. Nie używać tego dekodera do dowolnych maszyn FTMS.
- Nieprawidłowy lub ucięty pakiet Rowing Data nie zastępuje poprzedniego poprawnego stanu. Po 6,5 s brak danych jest widoczny na ekranie i w polu ważności FIT.

## Budowanie

Wymagane: oficjalny Connect IQ SDK, pobrany profil `fenix6xpro`, Java i własny klucz deweloperski DER. Zastosowano SDK 9.3.0 oraz Java 21.

```powershell
./build.ps1 -SdkPath C:/narzedzia/connectiq-sdk -JavaPath C:/narzedzia/java/bin/java.exe -SigningKey C:/prywatne/developer.der
```

Klucz podpisujący jest prywatny: nie umieszczaj go w repozytorium ani publicznej paczce źródeł. Dla dalszych aktualizacji zachowaj ten sam klucz i identyfikator aplikacji.

Testy dekodera kompiluje opcja `-RunTests`. Do wykonania testów musi działać symulator SDK. Testy obejmują rzeczywisty format 15-bajtowy, wariant z mocą, brak tempa i ucięte/niepoprawne ramki. Testy C++ `watch_hr_bridge_test.cpp` sprawdzają odbiornik pakietów HR, duplikaty i przepełnienie licznika. Testy symulatora nie zastępują próby na zegarku.

Źródła API: [BluetoothLowEnergy](https://developer.garmin.com/connect-iq/api-docs/Toybox/BluetoothLowEnergy.html), [Characteristic.requestWrite](https://developer.garmin.com/connect-iq/api-docs/Toybox/BluetoothLowEnergy/Characteristic.html), [Activity.Info](https://developer.garmin.com/connect-iq/api-docs/Toybox/Activity/Info.html).
