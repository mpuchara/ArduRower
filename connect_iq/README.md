# ArduRower HR 017 dla Garmin fēnix 6X Pro / Pro Solar

Pole Connect IQ **0.1.7** obsługuje ArduRower CYD 0.09.38 oraz starsze ramki 0.09.23–0.09.37. Komputerek nadaje jako „WR S4BL3”. Pole odsyła aktualne tętno aktywności wybrane przez Garmina przez to samo połączenie BLE.

## Instalacja przez USB

1. [Pobierz AROWERHR.PRG dla fenix6xpro](https://github.com/mpuchara/ArduRower/releases/download/v0.09.38/AROWERHR.PRG).
2. Podłącz zegarek w trybie MTP. Skopiuj PRG do **Primary/Garmin/Apps**, zastępując poprzedni plik o tej samej nazwie. Jeśli system udostępnia zegarek jako dysk, użyj folderu **GARMIN/APPS**.
3. Odłącz zegarek. W aktywności **Wioślarstwo w pomieszczeniu** sprawdź, czy na ekranie danych znajduje się **ArduRower HR 017**. Usuń z ekranów tej aktywności inne pola łączące się z tym samym komputerkiem.
4. Włącz CYD, wybierz użytkownika i tryb. Otwórz ekran pola i rozpocznij zapis na zegarku. Po zapisaniu aktywności zsynchronizuj ją z Garmin Connect.

Układ dopasowuje się do miejsca dostępnego na zegarku. Dystans, tempo i moc mają priorytet. Większe pole mieści też prędkość, kadencję, puls, liczbę ruchów i czas aktywny.

## Dane w FIT i Garmin Connect

| Pole deweloperskie w rekordzie FIT | Jednostka | Źródło |
| --- | --- | --- |
| Dystans | m | FTMS |
| Prędkość | m/s | Wyliczenie z tempa FTMS. Na ekranie zegarka km/h |
| Tempo i średnie tempo | s/500 m | FTMS |
| Moc i średnia moc szacunkowa | W | Dotychczasowy model Concept2 używany na CYD |
| Kadencja i średnia kadencja | SPM | FTMS, co 0,5 SPM |
| Liczba ruchów | szt. | FTMS |
| Czas aktywny | s | FTMS, według zegara CYD |
| Ważność telemetrii BLE | 0/1 | Pole Connect IQ |

Pole rejestruje 11 metryk, 31 bajtów na komunikat FIT Record przy limicie 32 bajtów na pole danych. Metadane wykresów są dołączone do aplikacji. Zegarek nadaje znaczniki czasu i decyduje o zapisie co sekundę albo w trybie Smart Recording. Pole przekazuje jedynie nową, świeżą próbkę. Rozłączenie od razu usuwa poprzednią próbkę z pola. Brak mocy jest pomijany, a prawidłowa wartość 0 W może być zapisana. Prędkość jest dostępna, gdy ramka zawiera tempo. Auto pauza zamraża czas CYD i nie ma osobnej flagi w ramce. Tętno zachowuje natywną ścieżkę zapisu Garmina.

Dane ArduRower w tym trybie pozostają Developer Fields. Metadane wykresów nie gwarantują, że Garmin Connect wyświetli wszystkie serie po instalacji USB. Potwierdzenie wymaga zapisania aktywności na fizycznym zegarku i jej synchronizacji. Garmin opisuje prezentację wykresów dla pól Connect IQ i dystrybucję aplikacji przez Connect IQ Store. [Wytyczne Garmin dla pól danych](https://developer.garmin.com/connect-iq/articles/user-experience-guidelines/Data_Fields.html) · [Publikacja aplikacji](https://developer.garmin.com/connect-iq/submit-an-app/)

### Natywne podsumowania

- **BLE FTMS**. fēnix 6X Pro Solar odbiera dane w naszym polu Connect IQ. Garmin dokumentuje natywne parowanie z wybranymi wioślarzami ANT+. Natywnego importu z BLE FTMS dla tego modelu nie potwierdzono. Skojarzenie nativeNum opisuje ekwiwalent pola, lecz system Garmina nadal zarządza standardowymi metrykami aktywności. [Pomoc Garmin](https://support.garmin.com/en-US/?faq=rlynVxyyoq9dcSZGw5YFo8) · [Specyfikacja FIT](https://developer.garmin.com/fit/protocol/)
- **ANT+**. Profil Fitness Equipment obsługuje dane wioślarza, takie jak liczba ruchów, kadencja i moc. ESP32 na CYD ma radio Wi‑Fi i Bluetooth, więc taka integracja wymaga osobnego sprzętu ANT+ i sprawdzenia zgodności z zegarkiem. [Profil ANT+](https://thisisant.developer.garmin.com/pages/developer/ant-plus/device-profiles/index.html) · [Dane ESP32](https://documentation.espressif.com/esp32_datasheet_en.html)
- **Eksport FIT po treningu**. Narzędzie zapisuje nową kopię pliku z uzupełnionymi standardowymi polami dystansu, prędkości, mocy, kadencji i liczby ruchów, jeśli odpowiednie dane istnieją. Zachowuje wartości zapisane przez Garmin, w tym tętno i czas zegarka. Opcja poniżej pozwala uzupełnić również zerowy dystans natywny. Eksport obsługuje pojedynczą sesję wioślarską i zachowuje pola deweloperskie rozpoznawane przez SDK. Sprawdzono poprawność pliku narzędziami FIT. Akceptację kopii przez Garmin Connect i sposób wyświetlenia podsumowania trzeba potwierdzić osobno.

~~~powershell
python -m pip install -r connect_iq/tools/requirements.txt
python connect_iq/tools/export_fit.py trening.fit trening-ardurower.fit --replace-zero-distance
~~~

Kopia może pomijać komunikaty i pola spoza publicznego profilu Garmin FIT SDK. Narzędzie podaje liczbę pominiętych komunikatów. Zachowaj oryginalny FIT. Przy instalacji pola przez USB fēnix 6X może wpisać zerowy identyfikator aplikacji. Eksporter rozpoznaje wtedy kompletny zestaw 11 pól ArduRower według ich nazw, jednostek i typów.

## Protokół i budowanie

- FTMS 1826, Rowing Data 2AD1. Dotychczasowa ramka bez mocy ma 15 bajtów. Z oboma polami mocy ma 19 bajtów i flagi 0x087E. Przy braku jednego pola mocy ramka ma 17 bajtów. Tempo jest kodowane w całych sekundach na 500 m, a moc w całych watach SINT16 little endian. [FTMS i GATT](https://www.bluetooth.com/specifications/specs/fitness-machine-service-1-0-1/)
- Charakterystyka zwrotna c9f10002-6f50-4fca-9b8d-3f8d90ae1b65 ma ten sam format z wersją, flagą ważności, licznikiem LE16 i BPM LE16. Około 1 Hz, z potwierdzeniem zapisu. Przy braku pomiaru zegarek wysyła jawny brak tętna.
- Użyj oficjalnego SDK Connect IQ, profilu fenix6xpro, Javy i dotychczasowego prywatnego klucza. Budowanie i testy symulatora nie zastępują próby na zegarku.

~~~powershell
./build.ps1 -SdkPath C:/narzedzia/connectiq-sdk -JavaPath C:/narzedzia/java/bin/java.exe -SigningKey C:/prywatne/developer.der
~~~

Uruchom symulator SDK i dodaj opcję -RunTests. Testy sprawdzają ramki bez mocy i z mocą, brakujące wartości, wygaszanie danych przy rozłączeniu oraz zapis wartości i schemat FIT. Plik do instalacji powstaje w bin/AROWERHR.PRG.
