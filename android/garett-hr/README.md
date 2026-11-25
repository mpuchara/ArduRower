# ArduRower HR — Garett Essa 2 AI 4G (0.1.5)
Aplikacja do przesyłania pulsu przez istniejący WatchHrBridge. Użytkownik potwierdził prawidłowy puls na fizycznym Garett i ekranie CYD w 0.1.4. Dekoder producenta nie jest oficjalnie udokumentowany; zachowano wszystkie filtry i walidację. Nie do zastosowań medycznych.

## Obsługa
1. Wypakuj APK z artefaktu Actions. Z uwagi na zmienny podpis testowy może być konieczne odinstalowanie starszej wersji.
2. Włącz Bluetooth i lokalizację systemową (wymagana dla BLE na Androidzie 8.1; nie odczytujemy lokalizacji). Przy pierwszym uruchomieniu przyznaj uprawnienia czujnika i lokalizacji.
3. Włącz ArduRower, wybierz profil i trening. Nie łącz równocześnie pola Garmin.
4. Otwórz ArduRower HR. Pomiar FASTEST, dekoder Garett i wyszukiwanie BLE startują automatycznie. Naciśnij Połącz ponownie tylko przy problemie.
5. Puls na ekranie, animacja serca zgodna z BPM, wykres ostatnich 60 sekund (brak danych = przerwa). Wykres próbkowany co sekundę z aktualnego ważnego odczytu, nie jest EKG.
6. Po 15 s bez dotyku aplikacja ustawia jasność 6% i zatrzymuje ciągłą animację. Dotyk przywraca systemową jasność; transmisja trwa podczas przygaszenia. Ekran pozostaje włączony, ponieważ opuszczenie Activity lub fizyczne wygaszenie może zatrzymać pomiar/BLE. Nie dodano usługi w tle. Zachowanie na Omnia OS i oszczędność energii wymagają testu.
7. Ustawienia i diagnostyka ukrywają tryby pomiaru, dekoder, liczniki i rozłączanie. Status wysyłania wymaga niedawnego potwierdzenia zapisu GATT z ważnym BPM. Nie jest to niezależne potwierdzenie odbioru na CYD.

## Budowa
Android 8.1: min API 26, target 27, compile 35, pojedynczy DEX, Java, bez JNI i Internetu, podpis v1. Android SDK 35, build-tools 35.0.0, JDK 17, bash android/garett-hr/build.sh. Podpis testowy generowany na runnerze; dla stabilnych aktualizacji HR_KEYSTORE i HR_KEYSTORE_PASSWORD. Nie publikować klucza.

## Regresja
Nie zmieniono firmware, Garmin IQ, pakietu BLE, dekodera ani zasad ważności pomiaru. Testy obejmują przykłady Garetta, zakres, dokładność, czas, pakiet HR i bufor wykresu. Test fizyczny nowej wersji nadal konieczny.

## Historia
## 0.1.3 — pomiar HR
Obsługa wszystkich standardowych wariantów czujnika HR, także wake-up; ręczny restart i wybór, ponowna próba po 60 s bez ważnego pomiaru. Zdarzenia on-change ważne maksymalnie 45 s, ciągłe 6,5 s; brak kontaktu lub niewiarygodna dokładność unieważnia odczyt natychmiast. Surowe BPM, accuracy, timestamp, rejestracja i liczniki widoczne na żywo w diagnostyce. Nie wysyłamy próbek oznaczonych jako niewiarygodne. Zegar HAL z oczywistym błędem korzysta z czasu odbioru nowego zdarzenia, stare poprawnie datowane zdarzenia odrzucane. Bez publicznego dostępu do fizycznego HR potrzebny będzie udokumentowany interfejs producenta. Test sprzętowy konieczny.

## Zmiany po próbie 131/131 odrzuconych
Wersja 0.1.3 zachowuje ostatni surowy odczyt i osiem zdarzeń, zlicza osobno brak kontaktu, accuracy=0, BPM poza zakresem oraz stary timestamp. Nie usuwa diagnostyki przy ponownej rejestracji. Standardowy interwał rejestracji 1 s, przełącznik trybu szybkiego, bez automatycznej ponownej rejestracji gdy nadal przychodzą zdarzenia. Obsługa starego HAL używającego uptime zamiast elapsedRealtime tylko przy zgodnym świeżym timestampie. Nie wysyła danych accuracy=0 lub -1 jako ważnego pulsu. Surowe wartości widoczne na ekranie niezależnie od transmisji. Test na fizycznym zegarku konieczny. 

## Garett 0.1.3
Observed floats 9.753404E20 and 2.4268218E20 represent bits 62537E45 / 61527E45, bytes 98/83/126/69 and 97/82/126/69. Second byte is the candidate BPM. The interpretation is experimental and not vendor-confirmed. Main screen shows candidate and bytes; standard BPM still works. Compare several readings with the official app before enabling the explicit test-mode button. Confirmation is session-only; no packed samples are transmitted before confirmation. Accuracy <=0, out-of-signature packets, stale timestamps are still rejected. No medical use. No CYD changes.

## 0.1.4 — szybki pomiar domyślny
Po uruchomieniu aplikacja rejestruje czujnik z SENSOR_DELAY_FASTEST. Nadal można przełączyć na interwał 1 s. Na fizycznym Garett użytkownik uzyskał w trybie szybkim kandydatów BPM zbliżonych do odczytu Garmin. Dekoder eksperymentalny nadal wymaga potwierdzenia, transmisja BLE i firmware bez zmian.
