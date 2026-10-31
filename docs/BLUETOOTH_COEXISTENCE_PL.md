# Bluetooth: stan testów COM6, 30.10.2026

**Zmiana zakresu po tych testach:** użytkownik potwierdził odbiór danych na starym polu, a następnie zlecił własne pole z instalacją USB. Nowa ścieżka 0.09.23 jest opisana w `connect_iq/README.md`: tętno wraca ze wspólnego połączenia przez prywatny zapis GATT. Poniższe wyniki dotyczą wcześniejszego wymagania zachowania starego pola.

Firmware diagnostyczny 0.09.22, ESP32-D0WD-V3, Garmin fēnix 6X Pro Solar.
Zachowujemy dotychczasowe pole Connect IQ, nazwę WR S4BL3 i format FTMS.
Jednoczesne, świeże HR z TEGO zegarka i odbiór FTMS przez TO pole nie są jeszcze uruchomione.

## Co potwierdzono

| Próba | Wynik fizyczny |
| --- | --- |
| Garmin HR do ESP32, FTMS do laptopa | Działa równocześnie. Laptop odebrał 25 rzeczywistych powiadomień FTMS, ESP32 nadal odbierał zmienne HR z zegarka. |
| Jak wyżej, FTMS pod adresem publicznym, HR pod oddzielnym prywatnym | Także działa: dwa różne urządzenia zdalne i dwa równoczesne ACL. |
| FTMS z Garmina pierwszy, drugi ACL HR do tego samego zegarka | Kontroler odmawia: HCI 0x0B, w NimBLE 523, Connection Already Exists. Także przy niezależnej prywatnej tożsamości ESP32. |
| HR pierwszy, ta sama aktywność z obecnym polem i nadawaniem HR | Zmienne HR dochodzi, pole pozostaje scanning przez 180 s. Laptop potwierdził 65 reklam FTMS w 20 s. Po zwolnieniu samego HR Garmin łączy FTMS. Telefon był odłączony. |
| Standardowe HR na istniejącym połączeniu FTMS | Odkrywanie 180D/2A37 i zapis CCCD 01 00 działają, brak powiadomień HR. |
| Jawny READ 2A37 | Zwraca zapamiętane BPM, użytkownik potwierdził brak zmian. Od 0.09.18 nie odświeża pulsu ani wykresu. |
| Inicjalizacja dodatkowej charakterystyki legacy Garmina przed HR | Zapis obu CCCD działa, brak HR. Poprzednie ustawienie legacy zostało przywrócone z powodzeniem. |
| MultiLink REAL_TIME_HR (usługa 6), bez szyfrowania | CCCD oraz żądanie rejestracji z potwierdzeniem ATT przyjęte, brak odpowiedzi rejestracji i HR. Poprzednie CCCD przywrócone. |
| Szyfrowanie istniejącego FTMS | Zestawione: encrypted=1, authenticated=0, bonded=1. FTMS nadal wysyła. Nie usuwano parowania telefonu. |
| MultiLink na szyfrowanym FTMS | Odczyt poprzedniego CCCD zwraca NimBLE 389, czyli ATT 0x85. Diagnostyka przerywa bez zapisu, ponieważ nie ma wartości do przywrócenia. Znaczenie tego kodu producenta pozostaje nieustalone. |
| Ponowne włączenie standardowego HR po szyfrowaniu | Zapis CCCD również odrzucony kodem 389 / ATT 0x85. Brak świeżego HR. |

## Granice wniosków

ESP32 potrafi równocześnie nadawać FTMS i odbierać HR: potwierdzają to oba testy kontrolne z laptopem. Nie wolno przedstawiać tego problemu jako ogólnej niemożliwości obsługi dwóch formatów przez ESP32.

Przetestowane standardowe ścieżki nie dają obu funkcji z tym samym zegarkiem i obecnym polem. Zmiana kolejności połączeń przenosi działanie między FTMS i HR. Testy nie stanowią dowodu, że każda możliwa implementacja protokołu producenta jest niemożliwa. Obsługa MultiLink pozostaje eksperymentem, a nie naprawioną funkcją.

Bluetooth Core dopuszcza jeden ACL pomiędzy tą samą parą adresów LE. Osobne usługi mogą działać na wspólnym ACL; firmware próbował takiego klienta GATT. Oddzielny prywatny adres lokalny zmienia parę adresów, ale w tym kontrolerze próba nadal kończy się 0x0B. [Specyfikacja warstwy łącza](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/low-energy-controller/link-layer-specification.html), [definicja kodu 0x0B](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/architecture%2C-change-history%2C-and-conventions/controller-error-codes.html).

## Uruchamianie diagnostyki

Wymagane: Python z pyserial; do próby kontrolnej również bleak. Nie otwierać dwóch monitorów COM6 jednocześnie.

```
python tools/serial_capture.py --port COM6 --seconds 240 --log work/device.log --commands work/device-command.txt
python tools/probe_ftms.py
```

Monitor nie przełącza DTR/RTS przy otwieraniu. Zapis nowego tekstu komendy do pliku poleceń wysyła ją jednokrotnie. Plik `work/device.log.stop` kończy przechwytywanie; przed ponownym użyciem tej nazwy logu należy usunąć ten konkretny znacznik stop.

Komendy USB:

- `reboot`: normalny start, zwykły pulpit TEMPO /500m i FTMS jako priorytet.
- `hrfirst`, `hrpublic`, `hrsame`, `hrfresh`: jednorazowe warianty kolejności/tożsamości z restartem i oknem 180 s. Nie są domyślnym zachowaniem.
- `gattdump`: odczyt struktury GATT na istniejącym połączeniu FTMS.
- `garmininit` / `garminoff`: włączenie legacy notify + standard HR / przywrócenie poprzedniego legacy CCCD.
- `garminml` / `garminmloff`: próba rejestracji wyłącznie własnego strumienia REAL_TIME_HR / zamknięcie własnego uchwytu i przywrócenie poprzedniego ML CCCD. Bez zamykania cudzych uchwytów i bez GFDI.
- `garminsecure`: jawna próba szyfrowania/parowania istniejącego FTMS, bez MITM; nie jest wywoływana automatycznie.
- `trace`, `calibrate`: diagnostyka dotyku. Dalsze testy ducha zostały odłożone na prośbę użytkownika.

Metadane i uchwyty są odkrywane przez UUID oraz związane z konkretną sesją. Brak odczytu poprzedniego CCCD zatrzymuje zapis. Po przekroczeniu limitu procedury firmware nie uruchamia kolejnej, dopóki poprzednie callbacki nie skończą pracy. Próbki HR wymagają rzeczywistego powiadomienia, nigdy odczytu cache.

Dziennik chronologiczny: `CYD_0.09.17_VALIDATION.md`. Surowe logi i obrazy są w przekazanej paczce wynikowej. Obrazy aplikacji wgrywano pod 0x10000 z weryfikacją esptool, bez kasowania NVS, kalibracji i rekordów. Kopia pełnego flash sprzed zmian ma 4 MiB i nie jest zwykłym obrazem aktualizacji.

Protokół MultiLink: [dokumentacja Gadgetbridge](https://gadgetbridge.org/internals/specifics/garmin-protocol/), sprawdzona razem z CommunicatorV2 w wersji edbea98cbe1377001c0b8154ac20cb930e1db7f6. Implementacja diagnostyczna korzysta z opisanych formatów komunikatów; nie wprowadza zamiennika pola Connect IQ.
