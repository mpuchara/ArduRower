# Wgrywanie ArduRower CYD przez GitHub Pages

Instalator: **https://mpuchara.github.io/ArduRower/**

## Jednorazowa konfiguracja GitHub Pages

Repozytorium → **Settings → Pages → Build and deployment → Source**: ustaw **GitHub Actions** (zamiast „Deploy from a branch”). Strona jest w katalogu głównym repo (index.html), ale kompletne binaria powstają dopiero podczas kompilacji jednego profilu ST7789/GPIO21 w GitHub Actions. Publikowanie samego main / (root) nie udostępni manifestów ani firmware.

Workflow: .github/workflows/cyd-web-installer.yml („ArduRower CYD - Pages installer”). Uruchamia się na zmianach kodu i przez „Run workflow”. Dla gałęzi main kompiluje firmware i publikuje stronę razem z jednym manifestem oraz pełnymi obrazami ESP32. Na gałęzi feature tylko buduje i testuje; nie nadpisuje publicznej strony.

Bez działania workflow instalator celowo pokazuje informację o braku binariów zamiast niesprawnego przycisku.

## Co zostanie wgrane

GitHub Pages publikuje **cztery niezależne pliki BIN** i manifest ESP Web Tools:

- bootloader: `0x1000`
- tablica partycji: `0x8000`
- boot_app0: `0xE000`
- aplikacja: `0x10000`

**Dlaczego nie jeden scalony obraz firmware?** Scalenie plików z wypełnieniem 0xFF i wgranie go pod offset 0 mogłoby nadpisać obszar NVS `0x9000–0xDFFF`, kasując rekordy Ghost Race. Rozdzielone pliki omijają NVS. Przy kolejnych instalacjach NIE wybieraj pełnego erase flash. Spójność zakresów i obrazów jest sprawdzana w GitHub Actions.

**Jedyny profil:** ST7789 z podświetleniem GPIO21, potwierdzony na docelowej płytce.

## Procedura

1. Otwórz instalator w aktualnym **Chrome lub Edge na komputerze** (HTTPS i Web Serial). Nie używaj przeglądarki w telefonie.
2. Podłącz płytkę **kablem micro-USB umożliwiającym transmisję danych**. W przypadku tej rewizji USB-C bywa problematyczne.
3. Kliknij „Połącz i zainstaluj”; zezwól na port COM rozpoznanej płytki.
4. Jeśli instalator nie może połączyć się z ESP32, zamknij Serial Monitor/PlatformIO. W razie potrzeby zgodnie z komunikatem instalatora wejdź w bootloader za pomocą BOOT podczas resetu i ponów próbę.
5. Po flashowaniu odłącz/podłącz płytkę lub ją zresetuj. Sprawdź dashboard, reakcję na dotyk i odczyt kontaktronu (GPIO22 + GND).
6. Na Garmin fēnix 6X Pro Solar otwórz **to samo** pole Connect IQ co poprzednio. Współpraca BLE WR S4BL3 / FTMS została celowo zachowana; nie zmieniaj ustawień pola bez potrzeby.

**Uwaga:** flashowanie zastąpi dotychczasowy UBox/HA firmware. Nowy port nie był jeszcze sprawdzony end-to-end na fizycznej płytce i zegarku. Przed wgraniem upewnij się, że możesz przywrócić stare firmware. W instalatorze wybór kasowania pamięci może usunąć konfigurację NVS. Nie odłączaj USB w trakcie zapisu.

## Bezpieczny development

- Heltec OLED pozostaje w ArduRower/ bez zmian.
- Nowy port jest w cyd_rower/; wyświetlacz dziedziczy pinout i profil ST7789/GPIO21 z HA_UBOX_bridge.
- Regresja BLE sprawdzana skryptem tests/cyd_ble_contract_test.py.
- Workflow zachowuje także artefakt ardurower-cyd-web-installer z gotowym pakietem strony.
- W razie awarii GitHub Pages stan publikacji można sprawdzić w **Actions**.

## Odświeżanie LCD

Poprawiony firmware nie czyści całego ekranu co pół sekundy. Zachowuje nieruchome ramki i podpisy, aktualizuje tylko zmienione liczby (2 Hz) i wykresy (1 Hz), w mniejszych buforowanych prostokątach. Test fizyczny potwierdzający brak migania pozostaje do wykonania po instalacji aktualizacji.


## Ghost Race v0.08 — instalacja/aktualizacja

Nowy tryb zawiera wybór wyścigu 500, 1000 lub 2000 metrów i zapis najlepszego przejazdu. Przesuń palcem w lewo lub w prawo, aby przejść do menu wyścigów; ponowny swipe wraca do dotychczasowego dashboardu. Jeżeli ekran poprosi o kalibrację, dotknij po kolei trzech żółtych znaków +. Dotyk wybiera dystans; BOOT krótko wybiera kolejną pozycję, przytrzymanie >1,5 s uruchamia wyścig. Start czasu następuje na pierwszym impulsie kontaktronu. Zegar wyścigu nie zatrzymuje się przy odpoczynku. Wartości są estymowane jak dotychczas.

Na ekranie wyścigu dwie belki pokazują dystans aktualny i zapisany, przewagę w metrach i sekundach oraz moc/prędkość z miniwykresami. Rekord zapisuje się tylko po ukończeniu dystansu szybciej od poprzedniego wyniku. Podczas aktualizacji **nie zaznaczaj pełnego erase flash**, aby zachować rekordy Ghost Race w NVS. Interfejs BLE FTMS nie jest modyfikowany.


## Instalacja v0.09-motion-hr

Na stronie instalatora sprawdź oznaczenie `0.09-motion-hr` przed aktualizacją. Repozytorium publikuje cztery obrazy na oddzielnych offsetach (brak nadpisywania NVS pomiędzy `0x9000` a `0xDFFF`). Przy aktualizacji nie zaznaczaj erase flash.

Wersja 0.09 dodaje eksperymentalną detekcję faz pociągnięcie/powrót z jednego kontaktronu, model prędkości z wygaszaniem, niezależną skalę zapisaną w NVS, większe pola i wspólny 10-minutowy wykres pulsu/prędkości oraz opcjonalny odbiornik tętna BLE (`0x180D`). Jeżeli Garmin nie wysyła HR lub moduł nie zdoła połączyć się jako klient BLE, prędkość, licznik i FTMS nadal pracują, a puls ma wartość `--`. Jednoczesne połączenia z zegarkiem trzeba zweryfikować doświadczalnie.

Przed pełnym treningiem wykonaj 30 spokojnych pociągnięć i sprawdź liczbę ruchów w trybie diagnostycznym: zmiana fazy na podstawie zwolnień NIE jest bezpośrednim pomiarem kierunku. Nie traktuj nowego dystansu jako skalibrowanego względem WaterRower S4.


## v0.09.1: starszy dashboard mimo zakończenia instalacji?

Poprzednio strona sprawdzała jedynie obecność manifestu i mogła umożliwić flashowanie starszych, buforowanych obrazów nawet po scaleniu nowszego kodu do `main`. Ponadto publikacja do GitHub Pages trwa dłużej niż kompilacja w PR. **Wydanie 0.09 zostało wdrożone do Pages dopiero 29.10.2026 o 11:52 czasu polskiego** (workflow `40860610696`).

Od v0.09.1 strona **blokuje przycisk instalacji**, dopóki wersja `version.json` nie rozpoczyna się od `0.09.1-installer-`, wersja manifestu nie jest identyczna i wszystkie cztery obrazy BIN nie mają nazwy z dokładnie tym samym 12-znakowym identyfikatorem kompilacji. Adres manifestu zawiera parametr wersji, a obrazy mają unikatowe nazwy (brak zderzeń cache). Numer `0.09.1` jest drukowany na ekranie powitalnym i w nagłówkach dashboardu oraz Ghost Race. Zwykłe wgranie nie narusza NVS (rekordów).

Otwórz `https://mpuchara.github.io/ArduRower/?v=0.09.1`, użyj Ctrl+F5 i sprawdź u dołu strony napis `Kompilacja: 0.09.1-installer-...`. Dopiero gdy pojawi się zielony napis `Gotowy do wgrania firmware`, kliknij instalację. Po restarcie płytki powinno być widoczne `v0.09.1`. Jeśli nie — możliwy jest nieudany flash, błędny port lub uruchomienie innego firmware; porównaj numer COM oraz logi wgrywania i monitora szeregowego. Nie wybieraj „erase all flash” jeśli chcesz zachować Ghost Race/NVS.


## v0.09.3 — diagnostyka nieukończonego połączenia HR

Poprawa jest ograniczona do opcjonalnego odbiornika pulsu przez BLE; serwer FTMS i działające pole Connect IQ Garmina pozostają bez zmian. Nowe logi w monitorze portu szeregowego odróżniają blokowanie w `connect()`, znalezienie usługi `0x180D`, charakterystyki `0x2A37`, włączenie powiadomień i faktyczne otrzymanie BPM. Gdy etap trwa >16 sekund, górny status HR pokazuje `HR ZAW`. Należy przesłać kilka ostatnich linii `HR:`, bez konieczności wykonywania ręcznego restartu. Nie jest to twardy timeout zrywający połączenie — starsza biblioteka nie ma bezpiecznego publicznego anulowania `connect()`, a rozłączanie adresu przez GAP może zerwać FTMS.

Po prawidłowym połączeniu, lecz braku pierwszego BPM przez 15 sekund, klient HR bezpiecznie się rozłącza i ponawia próbę. Nowe pliki instalatora mają identyfikator `0.09.3-hr-link-` i nie nadpisują partycji NVS.


## CYD 0.09.4: niezależna diagnostyka CCCD i wejście do Ghost Race

Wersja `0.09.4-hr-cccd-ghost-...` wymaga instalatora z pasującymi obrazami. Najpierw lokalna rejestracja GATT HR, potem zapis do CCCD `0x2902`, ze sprawdzeniem statusu obu operacji. Nowy status `HR CCCD` opisuje oczekiwanie na zapis deskryptora. Odbiór BPM nie jest gwarantowany przez samo podłączenie GATT. Działa wejście do wyścigów zarówno dotknięciem nagłówka dashboardu, jak i dwukrotnym przyciskiem BOOT; nie ma wymuszonej kalibracji dotyku przy otwieraniu menu. Pierwszy wyścig na nowym algorytmie tworzy nowy rekord, starsze rekordy nie są automatycznie konwertowane. Wyłącz pełne kasowanie flash, jeśli zachowujesz rekordy Ghost Race.


## v0.09.5 — odbiór HR bez zakłócania aktywnego FTMS

Aplikacja QZ (qdomyos-zwift) czyta Garmin Heart Rate przez systemowy stos Qt Bluetooth, nie przez biblioteki ESP32 Arduino BLE; używa standardu `180D/2A37` i zapisu CCCD `01 00`, który w naszych logach już zwraca powodzenie. Od 0.09.5 aktywne skanowanie BLE oraz GATT setup wykonywane są tylko wtedy, gdy Garmin nie jest aktualnie połączony z FTMS i od ostatniego ruchu minęło 15 s. Działający FTMS otrzymuje ciągły strumień co sekundę; odbiór HR po istniejącym linku pozostaje pasywny. Gdy FTMS przyłączy się podczas trwającego GATT setup, przerwiemy próbę przy najbliższym bezpiecznym punkcie, ale nie możemy przerwać wątku biblioteki blokującego się wewnątrz zapisu. Licznik zerwań FTMS trafia do logu. Nie obiecujemy twardych czasowych slotów RF, bo wykonuje je sam kontroler.

Przy wejściu do Ghost Race preferowana jest zmiana strony gestem poziomym w LEWO lub w PRAWO (również przy zamienionych fizycznych osiach dotyku). Log `GHOST: horizontal swipe detected` pozwala odróżnić problem czujnika dotyku od warstwy menu. Awaryjna opcja to podwójne BOOT.


## v0.09.7 — krytyczna naprawa bootloopa NimBLE na Arduino-ESP32 3.3.12

Użytkownik zgłosił `ESP_ERR_INVALID_STATE (0x103)` w `NimBLEDevice::init()` / `esp_bt_controller_init()`, plik `NimBLEDevice.cpp:880`. Nie jest to problem z czujnikiem, dotykiem, NVS ani danymi Garmin: kontroler BLE nie zostaje poprawnie uruchomiony.

Logi CI ujawniły, że PlatformIO z `platform = espressif32` obecnie pobiera **framework-arduinoespressif32 3.3.12** (nie 2.x, jak poprzednio zakładaliśmy). W tej wersji `initArduino()` sprawdza `btInUse()` i `bleInUse()`, a przy ich domyślnym `false` bezpowrotnie zwalnia pamięć kontrolera BLE podczas tego startu. Stara biblioteka NimBLE-Arduino 1.4.3 nie aktywowała tych nowych hooków. W efekcie `esp_bt_controller_init()` zwraca 0x103. Samo przywołanie `btStarted()` niczego nie zmieniało w 3.3.x, ponieważ `btInUse()` z `esp32-hal-bt.c` jest teraz słabym aliasem również zwracającym `false`.

Firmware 0.09.7 dodaje **silne implementacje C** `btInUse()=true` i `bleInUse()=true`, dzięki którym Arduino nie zwolni pamięci potrzebnej NimBLE. Aplikacja nie uruchamia Bluetooth dwa razy; klasyczne BT pozostaje nieużywane. CI nie tylko kompiluje CYD, ale sprawdza w firmware ELF, czy oba symbole są silne (`T`), a nie słabe (`W`). Firmware loguje stan kontrolera BT przed startem usługi.

Nie kasuj NVS przy reinstalacji — rekordy Ghost Race i kalibracja powinny pozostać. Gdy port COM rozłącza się w pętli restartów, użyj trybu flashowania: przytrzymaj przycisk BOOT podczas resetu/podłączania USB, następnie uruchom instalator i po zakończeniu odłącz/restartuj normalnie. Nie jest to test fizycznego odbioru BPM; najpierw zweryfikuj stabilny rozruch i połączenie FTMS.


## 0.09.8 — pakiet advertising Garmin FTMS i nieblokujący Ghost Race

W wersji 0.09.7 potwierdzono tętno od Garmin przez NimBLE, ale przez całą sesję `FTMS=0 tx=0`. Od 0.09.8 nazwa `WR S4BL3` jest także w **pierwszym pakiecie reklamowym**, wraz z oryginalnymi 9 bajtami usług/flag (0x1826 i 0x180A). Nie polegamy na metodzie NimBLE `addServiceUUID` po niestandardowym `setAdvertisementData`, bo te interfejsy się nie łączą. W logu: `FTMS: primary advertising started=yes`, `FTMS: advertising active`, `FTMS: CONNECTED`, a później liczniki `tx`. Jeżeli po migracji BLE poprzedni sensor zegarka ma niezgodną pamięć uchwytów GATT, należy usunąć i ponownie dodać `WR S4BL3` na zegarku; nie trzeba resetować ani czyścić NVS ESP32.

Ghost Race znowu sterowany swipem w lewo/prawo. Wcześniej brak kalibracji po tapnięciu menu uruchamiał blokujący kreator dotykowy do 45 sekund, co wyglądało jak zamrożony ekran. Teraz żadne tapnięcie menu nie wywołuje kalibracji. Bez kalibracji: dotyk lub krótki BOOT zmienia dystans, długi BOOT (1,5 s) uzbraja wyścig. Log `GHOST: alive` co 6 s potwierdza działającą pętlę; powrót swipem jest zawsze dostępny.


## 0.09.9 — użytkownik wymaga dwóch kierunków BLE z tym samym Garminem oraz dotykowego Ghost Race

**Ustalenie normatywne:** Bluetooth Core 6.2, LE Link Layer 4.5: *There shall be only one ACL connection, whether or not established, between two LE device addresses*. To tłumaczy logi fizycznego CYD v0.09.8. Gdy najpierw połączymy FTMS (`FTMS=1`, licznik `tx` rośnie), `NimBLEClient::connect()` do TEGO SAMEGO Garmina zwraca `FAIL` po 1 ms, mimo prawidłowego reklamowania HR. Gdy najpierw CYD połączy się jako BLE central do usługi HR, Garmin nie łączy się już jako central z CYD FTMS. Test z QZ i telefonem nie obala tej zasady, bo w nim występują trzy różne urządzenia Bluetooth.

**Wariant do sprawdzenia bez dodatkowych urządzeń:** rozdzielić GAP central/peripheral od GATT client/server. W jednym i tym samym już zestawionym połączeniu Garmin(central) ↔ CYD(peripheral/FTMS server) CYD *może* wykonać operację GATT client odczytując zdalną bazę usług Garmin. Firmware 0.09.9 przechodzi więc na niski poziom NimBLE `ble_gattc_disc_svc_by_uuid`, `ble_gattc_disc_chrs_by_uuid`, `ble_gattc_disc_all_dscs`, `ble_gattc_write_flat` na **uchwycie istniejącego FTMS ACL** (z callbacku `NimBLEServerCallbacks::onConnect(...,ble_gap_conn_desc*)`). Odbiera powiadomienia `BLE_GAP_EVENT_NOTIFY_RX` dla 0x2A37 przez niezależny, pasywny listener NimBLE. Nie wywołuje `NimBLEDevice::createClient()`, `NimBLEClient::connect()` ani aktywnego skanowania. Jeśli zegarek udostępnia 0x180D GATT Server również na połączeniu, gdzie jest central, możemy uzyskać HR i FTMS bez drugiego połączenia. **Nie jest pewne, czy Garmin eksponuje tę usługę w tym trybie**; na podstawie QZ wiadomo tylko, że robi to jako BLE Peripheral podczas osobnego połączenia z telefonem. Jeśli odpowie `no 180D on FTMS link`, rozwiązanie wymaga zmian w Garmin Connect IQ lub dodatkowej tożsamości BLE/urządzenia odbierającego HR. To test protokołu, nie obietnica fizycznej kompatybilności.

**Ghost Race:** log v0.09.8 zawierał `TOUCH: DOWN #1` i `GHOST: LIVE SWIPE #1`, ale przez dziesiątki sekund licznik `touch=1 swipes=1` nie wzrastał, mimo prób nawigacji. W v0.09.6 błędnie usunęliśmy działające wcześniej przerwanie GPIO36 z `XPT2046_Touchscreen touch(33,36)`, zastępując je `touch(33)`; przywrócono obsługę T_IRQ z działającego v0.08 i log `TOUCH: UP`. W trybie Ghost Race nie ma automatycznego blokującego kreatora kalibracji ani zależności od przycisku BOOT. Wiersze **500 / 1000 / 2000 m** obsługiwane są dotknięciem (także orientacyjnie przy braku zapisanej kalibracji); przesunięcie palcem w lewo lub prawo przełącza Ghost Race ↔ Dashboard bez resetowania ESP32.

Przyszły test fizyczny: FTMS połączone i wzrost `tx`, `HR: discover Garmin HR 180D on existing FTMS ACL`, wynik odkrywania 180D/2A37/2902, jeśli dostępne `HR: shared FTMS ACL BPM packet...`; trzy niezależne dotknięcia trzech wierszy, dwa swipy powrotne i logi `TOUCH: DOWN`/`TOUCH: UP`. Jeśli Garmin nie ma GATT HR service na wspólnym linku, nie próbować dalej podwójnych połączeń do identycznego adresu BLE.


## 0.09.10 — pressure touch recovery and Garmin HR tracing

- XPT2046 polls Z pressure continuously, not the GPIO36 IRQ latch; a dedicated PressTracker debounces release and can recover from a stuck DOWN without inventing a tap.
- The existing 0x1826 FTMS/Connect IQ payload and one-ACL link architecture are preserved.
- HR 0x2902 descriptor must belong to value handle 0x2A37. The client logs 180D/2A37 handles, CCCD readback and incoming GAP notifications to distinguish peripheral silence from a local callback filter issue.
- Passing CI does not establish that the Garmin sends heart rate on the FTMS link. Test on physical hardware and examine `HR: raw NOTIFY`, `HR: CCCD readback` and `HR: single-ACL`.
