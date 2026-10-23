# ArduRower na CYD ESP32-2432S028 (eksperymentalny port)

Ten katalog dodaje **nowy firmware dla dotykowego TFT 320x240**. Oryginalny szkic dla Heltec w ../ArduRower/ArduRower.ino pozostaje bez zmian. Obliczenia są bezpośrednio importowane z ../ArduRower/RowerMetrics.h (v0.07).

## Najważniejsze założenia

- **Nie zmieniamy współpracy z istniejącym polem Garmin Connect IQ**. Firmware reklamuje tę samą nazwę \`WR S4BL3\`, usługę FTMS \`0x1826\`, Rowing Machine Data \`0x2AD1\`, te same flagi, kolejność pól, jednostki oraz notyfikacje co 1 sekundę.
- Nie włączamy dodatkowej transmisji szacowanej mocy: \`SEND_EQUIVALENT_POWER = false\` (wartość z działającego ArduRower v0.07).
- Lokalny ekran pokazuje **szacowaną moc**, prędkość, dystans, czas, tempo /500 m, SPM, ruchy oraz dwa wykresy obejmujące **ostatnie 120 sekund**. Wartości mocy są przeliczeniem Concept2 z estymowanej prędkości, **nie** rzeczywistym pomiarem watów ergometru wodnego.
- Dotknięcie ekranu albo krótkie naciśnięcie BOOT przełącza widok; przytrzymanie BOOT przez 1,5 s resetuje trening. **BOOT podczas włączenia może wejść w tryb flashowania.**
- Brak połączenia z zegarkiem nie blokuje treningu. \`BLE OK\` oznacza dowolnego klienta BLE, nie potwierdza tożsamości Garmina.

## Połączenie kontaktronu

**Uwaga: starego GPIO2 nie wolno tu używać — GPIO2 jest zajęte przez TFT (DC).**

Podłącz **dwa przewody kontaktronu między GPIO22 a GND**. Zalecane złącze **P3 (JST 1.25mm 4-pin)**, gdzie standardowy pinout CYD to: GND, GPIO35, GPIO22, GPIO21. Należy sprawdzić opis ścieżek i kolejność pinów fizycznie na swojej rewizji płytki, **nie** według orientacji wtyczki ze zdjęcia. GPIO22 pracuje jako \`INPUT_PULLUP\`. Dla zwykłego pasywnego kontaktronu nie stosuj zewnętrznego 5 V.

Nie podłączaj czujnika do GPIO33 (CS panelu dotykowego), GPIO2 (DC LCD), GPIO21 (może sterować podświetleniem), GPIO27 (może sterować podświetleniem) ani GPIO34 (fotorezystor).

Dystans, prędkość i SPM pozostają heurystykami z v0.07; domyślny \`DISTANCE_SCALE=0.65\` w ../ArduRower/RowerMetrics.h. Przed użyciem porównaj kilka serii 30 pełnych ruchów z ręcznym liczeniem.

## Wyświetlacz — parametry sprawdzone w HA_UBOX_bridge

\`cyd_rower/platformio.ini\` celowo odtwarza profile i magistralę z
[HA_UBOX_bridge/firmware/cyd_monitor](https://github.com/mpuchara/HA_UBOX_bridge/tree/main/firmware/cyd_monitor).
Parametry: TFT SCK14/MOSI13/MISO12/CS15/DC2, dotyk XPT2046 na 25/39/32/33/36, obrót landscape \`3\`, jasność **100% przy starcie** (przed inicjalizacją BLE i dotyku). Brak Wi-Fi ani konfiguratora Home Assistant w tym firmware.

Dostępne profile:
- \`esp32-2432s028-st7789-gpio21\` — startowy dla wielu płytek dual-USB;
- \`esp32-2432s028-st7789-gpio27\` — drugi wariant ST7789;
- \`esp32-2432s028-ili9341\` — starszy/alternatywny ILI9341 z GPIO21.

**Wybierz profil, który działał na tym samym egzemplarzu płytki w HA_UBOX_bridge.** Jeśli ekran jest czarny, najpierw sprawdź pin podświetlenia; jeśli widać pasy, przekłamane kolory albo śmieci, zmień odpowiednio sterownik. Port nie został jeszcze sprawdzony na fizycznej płytce ani zegarku.

## Build i flash (PlatformIO)

Z głównego katalogu repozytorium:

\`\`\`bash
pio run -d cyd_rower -e esp32-2432s028-st7789-gpio21
pio run -d cyd_rower -e esp32-2432s028-st7789-gpio21 -t upload --upload-port COM4
pio device monitor -b 115200 -p COM4
\`\`\`

**COM4 jest jedynie przykładem** (numer COM trzeba sprawdzić w systemie). Do programowania użyj portu USB z działającym połączeniem UART; USB-C w niektórych rewizjach ma problemy. Przed instalacją miej możliwość przywrócenia poprzedniego firmware UBox.

## Bezpieczna sekwencja testów

1. Uruchom płytkę bez czujnika. Ekran powinien pokazać \`ArduRower CYD\`, wymiary 320x240, sterownik ST7789/ILI9341 oraz pin BL. Gdy ekran jest pusty, nie diagnozuj BLE, dopóki nie sprawdzisz profilu LCD.
2. Sprawdź, czy wyświetlają się dwa główne wskaźniki oraz wykresy i działa przełączanie widoku dotykiem/BOOT.
3. Podłącz kontaktron do GPIO22/GND. Przejedź magnesem kilkanaście razy; wartości powinny rosnąć. Test bez rezystora zewnętrznego.
4. Otwórz właściwe pole Connect IQ na zegarku i sprawdź, czy połączenie z \`WR S4BL3\` następuje tak samo jak ze starym komputerkiem. **Nie zmieniaj pola Garmin.**
5. Zweryfikuj licznik ruchów, dystans, SPM i prędkość na całym treningu. Kontroluj Serial Monitor i brak spadku impulsów (czerwony znak \`!\`).
6. Po przerwie ponad 5 sekund potwierdź pauzę, zero prędkości, brak narastania dystansu i prawidłowe wznowienie. Sprawdź reset BOOT oraz ponowne łączenie BLE.

### Ważna uwaga o baterii
W oryginalnym Heltec GPIO33 służyło do odczytu baterii. W CYD GPIO33 steruje dotykiem. Usługa Battery Service pozostaje wystawiona dla zgodności z klientem, ale zgłasza **100% jako wskaźnik stałego zasilania USB**, a nie rzeczywisty poziom akumulatora.

## Weryfikacja
Testy obliczeń z repo: \`g++ -std=c++11 -I ArduRower tests/metrics_test.cpp -o metrics_test && ./metrics_test\`.
Na CI/platformie programisty dodatkowo należy wykonać kompilację wszystkich trzech środowisk oraz test długiego obciążenia BLE/TFT. Samo istnienie źródeł w repo **nie potwierdza** jeszcze fizycznej kompatybilności danego LCD lub Garmina.
