# Wgrywanie ArduRower CYD przez GitHub Pages

Instalator: **https://mpuchara.github.io/ArduRower/**

## Jednorazowa konfiguracja GitHub Pages

Repozytorium → **Settings → Pages → Build and deployment → Source**: ustaw **GitHub Actions** (zamiast „Deploy from a branch”). Strona jest w katalogu głównym repo (index.html), ale kompletne binaria powstają dopiero podczas kompilacji trzech profili w GitHub Actions. Publikowanie samego main / (root) nie udostępni manifestów ani firmware.

Workflow: .github/workflows/cyd-web-installer.yml („ArduRower CYD - Pages installer”). Uruchamia się na zmianach kodu i przez „Run workflow”. Dla gałęzi main kompiluje firmware i publikuje stronę razem z trzema manifestami oraz pełnymi obrazami ESP32. Na gałęzi feature tylko buduje i testuje; nie nadpisuje publicznej strony.

Bez działania workflow instalator celowo pokazuje informację o braku binariów zamiast niesprawnego przycisku.

## Co zostanie wgrane

web/build_site.py łączy dla **każdego profilu LCD** cztery obrazy:

- bootloader: **0x1000**
- tabela partycji: **0x8000**
- boot_app0: **0xe000**
- firmware: **0x10000**

Następnie tworzy site/firmware/*.bin (pełne obrazy flash, adres 0) oraz site/manifests/*.json dla ESP Web Tools. GitHub Actions sprawdza nagłówki obrazów oraz granicę 4 MB. **Nie można wgrać samego firmware.bin pod adresem 0**. Manifest wskazuje bezpośrednio na statyczne pliki z GitHub Pages.

Warianty:
1. ST7789 / podświetlenie GPIO21 — domyślny wybór testowy
2. ST7789 / podświetlenie GPIO27
3. ILI9341 / podświetlenie GPIO21

Zawsze wybieraj wariant działający wcześniej na **tym konkretnym CYD** z HA_UBOX_bridge.

## Procedura

1. Otwórz instalator w aktualnym **Chrome lub Edge na komputerze** (HTTPS i Web Serial). Nie używaj przeglądarki w telefonie.
2. Podłącz płytkę **kablem micro-USB umożliwiającym transmisję danych**. W przypadku tej rewizji USB-C bywa problematyczne.
3. Wybierz profil LCD i kliknij „Połącz i zainstaluj”; zezwól na port COM rozpoznanej płytki.
4. Jeśli instalator nie może połączyć się z ESP32, zamknij Serial Monitor/PlatformIO. W razie potrzeby zgodnie z komunikatem instalatora wejdź w bootloader za pomocą BOOT podczas resetu i ponów próbę.
5. Po flashowaniu odłącz/podłącz płytkę lub ją zresetuj. Sprawdź dashboard, reakcję na dotyk i odczyt kontaktronu (GPIO22 + GND).
6. Na Garmin fēnix 6X Pro Solar otwórz **to samo** pole Connect IQ co poprzednio. Współpraca BLE WR S4BL3 / FTMS została celowo zachowana; nie zmieniaj ustawień pola bez potrzeby.

**Uwaga:** flashowanie zastąpi dotychczasowy UBox/HA firmware. Nowy port nie był jeszcze sprawdzony end-to-end na fizycznej płytce i zegarku. Przed wgraniem upewnij się, że możesz przywrócić stare firmware. W instalatorze wybór kasowania pamięci może usunąć konfigurację NVS. Nie odłączaj USB w trakcie zapisu.

## Bezpieczny development

- Heltec OLED pozostaje w ArduRower/ bez zmian.
- Nowy port jest w cyd_rower/; wyświetlacz dziedziczy pinout i profile z HA_UBOX_bridge.
- Regresja BLE sprawdzana skryptem tests/cyd_ble_contract_test.py.
- Workflow zachowuje także artefakt ardurower-cyd-web-installer z gotowym pakietem strony.
- W razie awarii GitHub Pages stan publikacji można sprawdzić w **Actions**.
