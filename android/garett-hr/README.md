# ArduRower HR — Garett Essa 2 AI 4G (0.1.4)
Pierwsza wersja diagnostyczna, tylko fizyczny pomiar HR i wysyłanie do CYD 0.09.38 przez istniejący WatchHrBridge. Nie modyfikuje firmware ani Garmin IQ.

Android 8.1: min API 26, target 27, compile 35, pojedynczy DEX, Java, bez JNI i Internetu, podpis APK v1. Wzorzec: Espen/android/ala-robot/watch. Budowa: Android SDK platform 35, build-tools 35.0.0, JDK 17, bash android/garett-hr/build.sh. Domyślny klucz testowy generowany lokalnie; kolejne aktualizacje wymagają tego samego klucza (ustaw HR_KEYSTORE i HR_KEYSTORE_PASSWORD). Klucza nie publikujemy.

Instalacja: pobierz ZIP artefaktu Actions, wypakuj APK i prześlij na zegarek zwykłą metodą używaną z Alą. Nie potrzebujesz ADB. Włącz Bluetooth i lokalizację systemową (wymóg skanowania Android 8.1; aplikacja nie odczytuje lokalizacji), przyznaj uprawnienia. Załóż zegarek, otwórz aplikację, sprawdź BPM i listę czujników. Włącz CYD, wybierz użytkownika i trening, wyłącz pole Garmin na czas próby. Naciśnij Połącz. FTMS i HR ACK powinny rosnąć. Potwierdź BPM na ekranie CYD; ACK oznacza potwierdzony zapis GATT, nie niezależny dowód przyjęcia przez aplikację CYD. Przy braku ważnej próbki wysyłany jest jawny brak HR. Zegarek ma pozostać na ekranie aplikacji; opuszczenie ekranu zamyka BLE, a CYD wygasza HR po ok. 6,5 s.

Brak TYPE_HEART_RATE lub brak świeżych próbek: otwórz Czujniki / diagnostyka i sfotografuj listę. Nie ma obejścia uprawnień ani sztucznego BPM. Dostęp do fizycznego czujnika nie został potwierdzony. Brak zapisu historii, serwera i aplikacji treningowej — zakres celowo ograniczony. 

## 0.1.3 — pomiar HR
Obsługa wszystkich standardowych wariantów czujnika HR, także wake-up; ręczny restart i wybór, ponowna próba po 60 s bez ważnego pomiaru. Zdarzenia on-change ważne maksymalnie 45 s, ciągłe 6,5 s; brak kontaktu lub niewiarygodna dokładność unieważnia odczyt natychmiast. Surowe BPM, accuracy, timestamp, rejestracja i liczniki widoczne na żywo w diagnostyce. Nie wysyłamy próbek oznaczonych jako niewiarygodne. Zegar HAL z oczywistym błędem korzysta z czasu odbioru nowego zdarzenia, stare poprawnie datowane zdarzenia odrzucane. Bez publicznego dostępu do fizycznego HR potrzebny będzie udokumentowany interfejs producenta. Test sprzętowy konieczny.

## Zmiany po próbie 131/131 odrzuconych
Wersja 0.1.3 zachowuje ostatni surowy odczyt i osiem zdarzeń, zlicza osobno brak kontaktu, accuracy=0, BPM poza zakresem oraz stary timestamp. Nie usuwa diagnostyki przy ponownej rejestracji. Standardowy interwał rejestracji 1 s, przełącznik trybu szybkiego, bez automatycznej ponownej rejestracji gdy nadal przychodzą zdarzenia. Obsługa starego HAL używającego uptime zamiast elapsedRealtime tylko przy zgodnym świeżym timestampie. Nie wysyła danych accuracy=0 lub -1 jako ważnego pulsu. Surowe wartości widoczne na ekranie niezależnie od transmisji. Test na fizycznym zegarku konieczny. 

## Garett 0.1.3
Observed floats 9.753404E20 and 2.4268218E20 represent bits 62537E45 / 61527E45, bytes 98/83/126/69 and 97/82/126/69. Second byte is the candidate BPM. The interpretation is experimental and not vendor-confirmed. Main screen shows candidate and bytes; standard BPM still works. Compare several readings with the official app before enabling the explicit test-mode button. Confirmation is session-only; no packed samples are transmitted before confirmation. Accuracy <=0, out-of-signature packets, stale timestamps are still rejected. No medical use. No CYD changes.

## 0.1.4 — szybki pomiar domyślny
Po uruchomieniu aplikacja rejestruje czujnik z SENSOR_DELAY_FASTEST. Nadal można przełączyć na interwał 1 s. Na fizycznym Garett użytkownik uzyskał w trybie szybkim kandydatów BPM zbliżonych do odczytu Garmin. Dekoder eksperymentalny nadal wymaga potwierdzenia, transmisja BLE i firmware bez zmian.
