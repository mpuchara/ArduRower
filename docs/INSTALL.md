# Instalacja ArduRower v0.07

Firmware jest przeznaczony dla **Heltec WiFi Kit 32 z ESP32 (pierwotny model, nie V3)**, OLED 128×64, kontaktronu na GPIO2 i czterech magnesów. Pokazuje prędkość w km/h. Wersja v0.07 stosuje wstępny współczynnik dystansu i prędkości 0.65: poprzednie 20 km/h daje 13 km/h i około 132 W. To przybliżenie, nie potwierdzona kalibracja do Concept2. Wgranie zastąpi dotychczasowy program na płytce.

## Zalecana metoda: Arduino IDE

1. Rozpakuj `ArduRower-poprawiony.zip`.
2. Otwórz `ArduRower/ArduRower.ino`. Zachowaj wszystkie pliki w tym folderze, w tym `RowerMetrics.h` i lokalne biblioteki OLED.
3. W menedżerze płytek wybierz pakiet **esp32 by Espressif Systems**, wersję **3.3.2**, oraz płytkę **Heltec WiFi Kit 32**. Zostaw domyślne ustawienia tej płytki.
4. Podłącz płytkę przewodem USB obsługującym dane i wybierz jej port COM.
5. Kliknij **Wgraj**. Jeśli połączenie z bootloaderem nie nastąpi automatycznie, przytrzymaj przycisk BOOT podczas rozpoczynania wgrywania i puść po nawiązaniu połączenia.
6. Po uruchomieniu sprawdź impulsy kontaktronu, ekran i połączenie BLE z używanym odbiornikiem.

## Gotowy plik BIN bez kompilacji

`ArduRower-Heltec-WiFi-Kit-32-v0.07.bin` jest **pełnym obrazem flash 4 MB**: zawiera bootloader, tablicę partycji i program. Wgrywaj go pod adres **0x00000000**. Nie używaj adresu 0x10000 dla tego pliku.

Możesz użyć narzędzia esptool 5.x. W folderze z plikiem BIN, po zainstalowaniu esptool, wykonaj:

```powershell
python -m pip install "esptool>=5,<6"
python -m esptool --chip esp32 --port COM5 --baud 115200 write-flash 0x0 ArduRower-Heltec-WiFi-Kit-32-v0.07.bin
```

Zastąp `COM5` rzeczywistym portem płytki. Nie wykonuj osobnego kasowania flash — do tej instalacji nie jest potrzebne. Obraz zawiera także obszary wypełnione pustymi bajtami, więc zastąpi również dotychczasową zawartość tych obszarów.

## Co sprawdzono

Kompilacja dla `esp32:esp32:heltec_wifi_kit_32` z pakietem ESP32 3.3.2 przeszła. Testy modułu obliczeń i granic ekranu przeszły. Obraz BIN odpowiada wersji z km/h. Nie został jeszcze sprawdzony na fizycznej płytce ani z Garminem.

Wskaźnik BLE pokazuje połączenie dowolnego klienta; sam model zegarka nie jest identyfikowany. Moc pozostaje szacunkiem, a dystans i kadencja wymagają weryfikacji na ergometrze.
