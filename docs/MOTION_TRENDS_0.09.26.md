# ArduRower CYD 0.09.26

Baza to commit [a532be5](https://github.com/mpuchara/ArduRower/commit/a532be596313a094cbf094c32916cd9f477bc5b7) (0.09.25). Z [PR #15](https://github.com/mpuchara/ArduRower/pull/15) przeniesiono filtr prędkości, detekcję nawrotów i autoskalowanie.

- EMA o stałej czasowej 2,5 s wygładza prędkość na ekranie i wykresie. Tempo chwilowe na ekranie i w FTMS wynika z tej samej prędkości. Dystans nadal całkuje model koła zamachowego z dotychczasową kalibracją. Tempo średnie wynika z aktywnego czasu i dystansu. Po automatycznej pauzie obie prędkości spadają do zera.
- Krótki nawrót wymaga lokalnego spowolnienia względem odstępów między impulsami i odstępu sprzed czterech impulsów, po którym następuje krótszy odstęp. Detekcja wolnego nawrotu nadal korzysta z progu zależnego od kadencji. Porównanie z poprzednim obrotem chroni obie ścieżki przed stałym szerokim odstępem między magnesami. Granica raportowania to 90 SPM.
- Wspólny wykres skaluje prędkość od zera do pełnego km/h obejmującego największą widoczną próbkę. Oddzielna skala pulsu obejmuje ważne pomiary BPM z marginesem. Brak pulsu pozostawia lukę. Przebieg ducha i bieżący trening korzystają z tych samych osi. Ekran nadal odświeża wykres pasami.

## Testy i instalacja

Testy hosta obejmują 30 spokojnych ruchów, 30 ruchów przy 50,8 SPM, 30 ruchów przy 77,4 SPM, dwie sekwencje nierównych magnesów, wygładzanie i pauzę, tempo przy większej intensywności, zakresy wykresów oraz kontrakty FTMS i wyświetlacza. Wyniki dotyczą danych syntetycznych. Fizyczny test wymaga wiosłowania oraz połączenia z polem ArduRower HR 015 na Garminie.

Profil PlatformIO to `esp32-2432s028-st7789-gpio21`. Aktualizacja działającej płytki przez esptool zapisuje jedynie aplikację od `0x10000`. Pakiet przeglądarkowy z `web/build_site.py` zawiera cztery osobne segmenty oraz manifest ESP Web Tools. Żaden zakres zapisu nie wchodzi do NVS. Instalator jest aktywny po wystawieniu całego pakietu przez HTTPS (lub localhost).
