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
