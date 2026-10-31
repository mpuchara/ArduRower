# ArduRower HR: własne pole Connect IQ

Wersja 0.1.5, nazwa na zegarku **ArduRower HR 015**, dla Garmin fēnix 6X Pro / Solar (`fenix6xpro`, sprawdzony numer 006-B3291-00, firmware 28.02). Wymaga ESP32 ArduRower CYD 0.09.23 lub nowszego z kanałem zwrotnym HR.

Pole odbiera Rowing Data z WR S4BL3 i odsyła `Activity.Info.currentHeartRate` zegarka przez to samo połączenie BLE. Nie wymaga nadawania tętna jako osobny czujnik 180D. Odczyt HR jest bieżącym tętnem aktywności; jeśli zegarek używa zewnętrznego czujnika HR, będzie to źródło wybrane przez zegarek.

## Instalacja przez USB

1. Zbuduj `AROWERHR.PRG` dla `fenix6xpro`.
2. Podłącz zegarek w trybie MTP. Skopiuj plik do `Primary/Garmin/Apps`. Zweryfikuj odczyt zwrotny pliku przed odłączeniem.
3. Odłącz USB. W aktywności wioślarskiej usuń wcześniejsze pole BLE z ekranów tej aktywności; może pozostać zainstalowane na zegarku.
4. Dodaj **ArduRower HR 015**. Można użyć małego pola: układ dopasowuje liczbę wierszy do jego wymiarów. Dwa różne pola zarządzające tym samym BLE mogą zajmować zasoby zegarka; usuń wcześniejsze pole BLE z ekranów tej aktywności.
5. Otwórz ten ekran przy włączonym komputerku. Przy braku pomiaru HR wysyłany jest jawny brak wartości, nie poprzednie BPM.

Tło jest pobierane z `DataField.getBackgroundColor()` przy każdym rysowaniu, a kolor tekstu jest kontrastowy. Wszystkie liczby i jednostki używają `FONT_XTINY`. Zniknęły nagłówek wersji i długie statusy techniczne. Wysokie pole ma trzy wiersze, niskie dwa, a najniższe jeden z priorytetem dystansu i tempa. Szerokość tekstu jest mierzona przed rysowaniem; gdy brakuje miejsca, pomijane są dodatkowe etykiety lub metryki zamiast nakładania wierszy. Przy rozłączeniu widoczne są kreski i krótki komunikat.

Pole pokazuje dystans, tempo /500m, kadencję i tętno. Zapisuje własne pola FIT: dystans, prędkość, kadencję, opcjonalną moc równoważną i ważność połączenia. Metadane `nativeNum` wskazują odpowiedniki FIT; nie oznacza to zastąpienia wszystkich natywnych podsumowań aktywności przez system Garmina. Moc jest zapisywana wyłącznie wtedy, gdy firmware rzeczywiście dołącza ją do FTMS; obecnie jest domyślnie wyłączona.

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
