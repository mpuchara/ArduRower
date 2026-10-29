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

**Jedyny potwierdzony profil sprzętowy:** `esp32-2432s028-st7789-gpio21` (ST7789, podświetlenie GPIO21).
Usunięto inne środowiska PlatformIO i ich obrazy. Na oryginalnym CYD użytkownik potwierdził działający obraz z tym sterownikiem.

### Płynne odświeżanie — poprawka migania

Poprzednia implementacja uruchamiała `fillScreen()` co 500 ms i rysowała ekran od góry na nowo, co powodowało widoczny czarny błysk. Nowy renderer utrzymuje statyczną makietę ekranu; aktualizuje tylko zmienione liczby oraz wykresy. Do renderowania używa jednego bufora `TFT_eSprite` 160×55 px, z 16-bitową paletą i awaryjną 8-bitową. Bufor jest przekazywany do LCD przez `pushSprite(..., crop)`. Widok jest czyszczony w całości wyłącznie przy zmianie strony lub po uruchomieniu. Wykresy mają kadencję 1 Hz, liczby 2 Hz i nie migają w trakcie aktualizacji. **Zmianę trzeba potwierdzić na fizycznej płytce**.

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


## Tryb GHOST RACE (v0.08)

**Swipe poziomo w lewo lub w prawo** na ekranie ST7789/GPIO21 przełącza pomiędzy obecnym dashboardem (z dwoma widokami) i **GHOST RACE**. Wejście do Ghost Race otwiera menu 500 m / 1000 m / 2000 m. Wybierz dystans dotykając całego wiersza. Jeśli nie ma kalibracji dotyku, przy pierwszym wejściu pojawią się trzy krzyżyki do dotknięcia; działająca kalibracja UBox CYD zostanie użyta automatycznie, jeśli zachowała się w NVS. Alternatywnie **krótki BOOT** przeskakuje między pozycjami menu, a **BOOT >1,5 s** wybiera dystans.

Po wyborze urządzenie czeka na pierwszy impuls kontaktronu, którym rozpoczyna się zegar wyścigu. Czas wyścigu biegnie także podczas przerwy, niezależnie od dotychczasowego treningowego czasu aktywnego i od protokołu BLE. Równolegle widać dwa paski dystansu (TY i DUCH), przewagę w metrach i sekundach, moc oraz prędkość z wykresami. Jeśli nie ma rekordu, pierwszy ukończony przejazd zostaje wzorcem; kolejne przejazdy ścigają się z najlepszym ukończonym rekordem. Przesunięcie z powrotem do dashboardu nie przerywa aktywnego wyścigu. Długie przytrzymanie BOOT podczas wyścigu anuluje próbę (rekord pozostaje). Po ukończeniu dotknięcie ekranu pokazuje menu; po wyjściu i powrocie do zakończonego trybu również pojawi się menu.

Wzorce są próbkowane co 2 sekundy (dystans w decymetrach), odtwarzane z interpolacją, zapisane w ESP32 NVS **wyłącznie po poprawie rekordu**. Każdy rekord jest walidowany sumą kontrolną i skojarzony z dystansem, wersją zapisu oraz aktualnym współczynnikiem skali 0,65. Limit pojedynczego wyścigu do zapisu wynosi około 34 minuty; po przekroczeniu limitu wynik nie jest zapisywany. Przed zmianą skali dystansu należy świadomie zarchiwizować/stworzyć na nowo rekordy; starsze zapisy o innej kalibracji zostaną odrzucone.

**UWAGA przy instalowaniu nowego firmware**: Nie wybieraj pełnego kasowania pamięci flash, jeżeli chcesz zachować rekordy w NVS. Zwykłe wgranie aplikacji i bootloadera nie musi kasować NVS, ale funkcja „erase all flash” usuwa rekordy. Starsza wersja firmware nie posiadała funkcji wyścigów, więc nie ma wcześniejszych rekordów do zachowania.

Współpraca Garmin Connect IQ i format BLE FTMS pozostają niezmienione. Testy w CI obejmują odtwarzanie ducha, kontrolę uszkodzonych rekordów, przerwy, przewagę, przepełnienie czasu i reset liczników, ale nie zastępują testu na fizycznym ekranie i zegarku.

## Ghost Race v0.08.1 — poprawka samoczynnego restartu

W v0.08 pamięć bufora napisów `previousField[12]` była za mała dla pól `textPatch(12–20)` z trybu wyścigu. Nadpisywało to sąsiednie dane, powodując możliwy restart ESP32 (ekran powitalny, po nim domyślny dashboard). W v0.08.1 oba bufory korzystają z jednej stałej długości 24, w funkcjach rysowania są kontrole granic, a skrypt `tests/cyd_text_cache_test.py` weryfikuje wszystkie użyte identyfikatory pól. Kod nie zmienia Garmin BLE ani sposobu zapisywania rekordów.

Aby sprawdzić, czy wystąpił **rzeczywisty restart płytki**, podłącz micro-USB do komputera, uruchom monitor szeregowy 115200 (np. `pio device monitor -p COM4 -b 115200`, dobierz odpowiedni port COM) i obserwuj linię `ArduRower CYD Ghost Race v0.08.1 | reset reason=... | heap=...`. Jeśli linia pojawia się ponownie przy przejściu do wyścigu, skopiuj również komunikat bezpośrednio przed restartem oraz `rst:...`. Jeśli ekran tylko zmienia widoki bez tej linii, będzie to problem nawigacji/dotyku, nie restart ESP32.


## Wersja 0.09 — fazy taśmy, puls i 10 minut korelacji

**Budowa mechanizmu:** cztery magnesy na kole zwijającym taśmę; sprzęgło jednokierunkowe przekazuje moment na wirnik podczas pociągnięcia, ale pozwala mu kontynuować obroty podczas powrotu. Dwa lokalne spowolnienia na cykl służą jako *hipoteza* zmiany fazy. Jeden kontaktron nie mierzy kierunku — algorytm wykorzystuje liczbę impulsów, czas fazy oraz adaptacyjny próg przerwy. Gdy zmiana kierunku nie wywoła wyraźnej przerwy, detektor może źle policzyć pociągnięcia. Zobacz zakładkę analizy cyklu (dotknij zwykłego dashboardu), która pokazuje wykrytą fazę, impulsy, ruchy i liczbę przejść.

Nowy wirtualny model prędkości **nie zwiększa prędkości na impulsach fazy powrotu**, lecz podczas niej wygasza prędkość z czasem bezwładności. Odległość jest całką z prędkości, a nie stałą liczbą metrów na *każdy* impuls. Bazowy współczynnik `0.14 m/impuls napędowy` oraz skala `0.70` są **prowizoryczne** i nie stanowią kalibracji oryginalnego monitora WaterRower S4. Wartości z testu 30 pociągnięć (41 wykrytych, 249 m, ~13 km/h, 1:15) uzasadniają eksperyment, ale nie określają prawdziwego dystansu.

**Kalibracja:** w dashboardzie przytrzymaj palec na górnym pasku (około 1,2 sekundy), aby otworzyć ekran skali. Użyj `-0.05` lub `+0.05` i wybierz `ZAPISZ I WROC`. Skala zapisuje się w NVS i jest uwzględniona w identyfikacji rekordów Ghost Race. Przy zmianie współczynnika resetuje się bieżący trening. Wymagany będzie niezależny punkt odniesienia do kalibracji absolutnej.

**Puls:** Garmin fēnix 6X Pro Solar może nadawać HR w trakcie aktywności. Nowy opcjonalny BLE-client ESP32 próbuje subskrybować standardową charakterystykę Heart Rate Measurement (`0x2A37`). Gdy pomiary nie przychodzą przez 6,5 s, wskazanie zmienia się na `-- BPM` i wykres ma przerwę. Samo pole Connect IQ odbierające FTMS działa jak wcześniej (bez zmian w pakiecie). Równoczesne działanie odbioru HR i FTMS **wymaga sprawdzenia na zegarku**. Przy małej ilości RAM odbiornik HR pozostaje nieaktywny, aby priorytet zachować dla FTMS.

**Wykres:** oba przebiegi są rysowane w jednym szerokim polu bez legendy, 300 próbek co 2 s (10 min), stałe zakresy 60–190 BPM (czerwony) i 0–22 km/h (turkus). Pola pulsu i prędkości powyżej wykresu mają ten sam kolor co linie. Dwa dolne rzędy wyświetlają: czas, dystans, SPM, tempo, moc szacowaną w W, ruchy.

**Ghost Race:** rekordy poprzedniej wersji (wyliczane ze stałego impulsowego dystansu) mają inny identyfikator modelu i **nie mogą być porównywane z aktualnymi**; stare dane pozostają w NVS do nadpisania nowym ukończonym rekordem. Nie wybieraj opcji pełnego kasowania flash. 


## 0.09.1: jak sprawdzić, czy nowe firmware naprawdę się wgrało

W poprzednim instalatorze wersja plików nie była weryfikowana przed włączeniem przycisku, więc możliwe było wgranie starszych obrazów z cache GitHub Pages. Od v0.09.1 urządzenie wyświetla numer firmware przy uruchamianiu i w nagłówku dashboardu/wyścigu. Otwórz stronę instalatora z parametrem `?v=0.09.1`, sprawdź `Kompilacja: 0.09.1-installer-...` i wgraj tylko zatwierdzoną przez stronę wersję. Proces zachowuje pamięć NVS z rekordami. Kod nie zmienia identyfikatorów ani formatu BLE FTMS.


## 0.09.2: dlaczego HR pokazuje -- BPM mimo BLE OK?

Znacznik **BLE OK** dotyczy wyłącznie połączenia `WR S4BL3` → pole Connect IQ w Garminie. Odbiornik tętna na ESP32 działa jako **osobny klient BLE** i oczekuje, że zegarek/czujnik nadaje usługę Heart Rate `0x180D` oraz charakterystykę `0x2A37`.

W wersji 0.09.2 skanowanie HR jest **aktywne**, by odbierać również odpowiedź skanowania, i udostępnia stany w prawym górnym rogu: `HR INIT`, `HR SKAN` (szukanie), `HR BRAK` (brak reklamy usługi 180D), `HR POL` (próba połączenia), `HR DANE` (połączenie, ale brak aktualnych pakietów BPM), `HR OK` (puls dostępny), `HR ROZ` (rozłączono), `HR RAM` (brakuje pamięci na drugą rolę BLE), `HR BLAD` (nie uruchomiono skanera/klienta). Log przez port USB / 115200 pokazuje liczbę reklam BLE i trafień 180D po każdym 4-sekundowym skanie. Próbę powtarza automatycznie co około 22 sekundy, o ile pozwala pamięć. Obowiązujący kontrakt FTMS i format danych do Connect IQ nie zostały zmienione.

**Test zgodności zegarka:** włącz na Garminie udostępnianie tętna podczas aktywności, uruchom trening i sprawdź aplikacją **nRF Connect** na telefonie, czy zegarek reklamuje BLE usługę `180D`. Uwaga: instrukcja fēnix 6 Pro opisuje także wysyłanie tętna przez ANT+, a niektóre materiały Garmina wymieniają serię fēnix 6 wśród urządzeń z nadawaniem BLE. Dlatego brak 180D w eterze wymaga ustalenia faktycznego zachowania Twojego egzemplarza. Klasyczny ESP32 CYD nie ma stosu radiowego ANT+ i nie potrafi odebrać ANT+ samym oprogramowaniem BLE. Alternatywami są nadajnik/pasek HR z BLE albo zewnętrzny mostek ANT+ → BLE; Garmin może w dalszym ciągu odbierać FTMS niezależnie.


## CYD 0.09.3 — klient pulsu Garmin: diagnoza i bezpieczne ponowienia

Logi z wersji 0.09.2 potwierdziły wykrycie urządzenia BLE reklamującego usługę `0x180D` (RSSI około -66 dBm) oraz przejście do `HR POL`, ale nie potwierdziły `connect() RETURN`, odnalezienia `0x2A37` ani odbioru powiadomień. Wpisy `POWERON_RESET` dotyczyły ręcznych restartów użytkownika i nie są uznawane za usterkę.

Wersja 0.09.3 rejestruje w USB (115200) etap po etapie: `connect() ENTER/RETURN`, callback GATT, `getService(0x180D) ENTER/RETURN`, `getCharacteristic(0x2A37) ENTER/RETURN`, `registerForNotify(0x2A37) ENTER/RETURN`, początek oczekiwania na pomiary i liczbę odebranych notyfikacji. Na ekranie: `HR POL`, `HR GATT`, `HR USL`, `HR CHAR`, `HR SUB`, `HR DANE`, `HR OK`; po 16 sekundach w blokującym etapie — `HR ZAW`. Ta ostatnia informacja oznacza, że starsza biblioteka ESP32 BLE jest zajęta: **nie stanowi potwierdzenia automatycznego odblokowania**.

Po udanym nawiązaniu połączenia, lecz braku **pierwszej** notyfikacji BPM przez 15 sekund, aplikacja rozłącza jedynie **klienta GATT HR** i ponawia próbę po przerwie. Po otrzymaniu poprawnego BPM zapamiętuje jego adres BLE w NVS, a przy kolejnych skanach preferuje go ponad pozostałe nadajniki tętna; jeśli urządzenie zmieni adres, bierze pod uwagę najsilniejszego kandydata. Zapamiętywanie dopiero po prawidłowym BPM chroni przed przypisaniem pierwszego przypadkowego urządzenia reklamującego `0x180D`.

**Nie włączono niebezpiecznego resetu Bluetooth ani wymuszonego rozłączenia fizycznego po adresie GAP**: takie zabiegi mogłyby jednocześnie zerwać działające połączenie Garmin → FTMS. Używana biblioteka ESP32 2.x ma blokujące `BLEClient::connect()` i nie udostępnia limitu oczekiwania przez publiczne API; po wykryciu `HR ZAW` potrzebujemy logu do rozstrzygnięcia, czy problemem jest `connect`, wyszukiwanie usług czy subskrypcja. Następny etap może wymagać kontrolowanej aktualizacji stosu Bluetooth, ale nie uzasadnia to na razie naruszania FTMS.

Instaluj ze strony `https://mpuchara.github.io/ArduRower/` po sprawdzeniu kompilacji zaczynającej się od `0.09.3-hr-link-` i napisu `FW 0.09.3`. Przy wgrywaniu zachowaj NVS i rekordy Ghost Race.


## v0.09.4 — wykryty problem: brak tętna pomimo połączenia oraz Ghost Race

Nowy log z 19:17–19:19 **potwierdza** `connect() RETURN result=OK elapsed=11ms`, `getService(0x180D) RETURN FOUND` i `getCharacteristic(0x2A37) RETURN FOUND, notify=yes`. Pierwsza rejestracja notyfikacji wróciła, ale 15 sekund później licznik pakietów nadal wynosił zero. Druga rejestracja zatrzymała się na etapie `HR SUB` >16 s. Nie jest to już problem skanowania ani samego `connect()`. Nie ma też wskazania na samoistne restarty (wcześniejsze resety były wykonywane ręcznie).

W Arduino-ESP32 Bluedroid 2.x funkcja `BLERemoteCharacteristic::registerForNotify()` pisze do deskryptora CCCD `0x2902` **przed** oczekiwaniem na zakończenie lokalnej rejestracji GATT. W wersji 0.09.4 rozdzielamy operacje: `registerForNotify(hrNotify,true,false)` (bez zapisu CCCD), następnie jawny zapis `0x01,0x00` do `0x2902`. Osobny, pasywny monitor zdarzeń `BLEDevice::setCustomGattcHandler` pokazuje faktyczne kody statusu `ESP_GATTC_REG_FOR_NOTIFY_EVT` i `ESP_GATTC_WRITE_DESCR_EVT`, również gdy metoda blokuje się podczas oczekiwania. `HR CCCD` na ekranie oznacza etap zapisu deskryptora. Nie zakładamy, że zarejestrowana subskrypcja oznacza odebranie BPM; `HR OK` wymaga prawidłowego pomiaru.

**Ghost Race:** do menu wejdź dotykając tytułu **ARDUROWER** w lewym górnym rogu dashboardu albo dwukrotnie naciskając fizyczny **BOOT** (odstęp do 480 ms). W menu BOOT: krótko = zmień dystans, przytrzymaj 1,5 s = wybierz/start. Przejście do menu nie wymusza już automatycznie blokującej kalibracji dotyku, a historyczny wykres w wyścigu ma właściwe odświeżanie. Log USB pokazuje `GHOST: armed`, `GHOST: started`, `GHOST: finished`. Starsze rekordy v1 (poprzedni model liczenia metrów) pozostają **niezgodne z v2**; pierwszy ukończony wyścig na nowym modelu jest punktem odniesienia dla kolejnego. Nie czyść pamięci NVS podczas instalacji.

**Ograniczenia:** oryginalne BLE FTMS i komunikacja Connect IQ nie były modyfikowane. Sukces kompilacji i testów nie stanowi potwierdzenia odbioru tętna na fizycznym Garminie; w szczególności brak odpowiedzi na CCCD lub brak danych z poprawnie włączonego CCCD nadal wymaga diagnostyki urządzeń. W razie `HR ZAW` prześlij linie `HR: GATT notify...`, `HR: CCCD WRITE...`, `HR: raw GATT notification...`.
