# ArduRower CYD 0.09.28

Na fizycznym CYD zarejestrowano zbocza kontaktronu z dwóch serii po sześć pociągnięć podobnej długości. W spokojnej serii próg 15 ms nie pominął zarejestrowanych zboczy. W szybkiej odrzucił 52, z czego 46 pojawiło się około 13–15 ms po poprzednim zboczu. Odtworzenie szybkiej serii z progiem 10 ms przyjmuje 277 odstępów zamiast 228. W spokojnej serii w obu wariantach pozostaje 259 odstępów.

Firmware rozdziela wejścia pomiaru drogi i detektora ruchów. Prędkość i dystans przyjmują zbocza co najmniej 10 ms od poprzedniego przyjętego zbocza. Licznik ruchów i kadencja zachowują dotychczasowy strumień z progiem 15 ms. Filtr prędkości ma tę samą stałą czasową 2,5 s, a dystans zatrzymuje się z ostatnim impulsem.

Bazowy krok dystansu podniesiono o około 14%, z 0,14 do 0,16 m na przyjęty odstęp przed zastosowaniem zapisanego mnożnika kalibracji. Przy mnożniku 0,70 daje to 0,112 m. W odtworzeniu krótkich serii najwyższe wskazania prędkości to około 7,9 km/h w spokojnej serii i 10,3 km/h w szybkiej, wobec około 6,9 i 7,1 km/h w 0.09.27. To porównanie z odtworzenia impulsów. Wartości bezwzględne pozostają estymacją do kalibracji względem zewnętrznego punktu odniesienia.

Nowe rekordy, przebiegi tętna i medale trafiają do kluczy `g5_*`, `h5_*`, `w5_*`. Wcześniejsze dane Ghost Race i ustawienia kalibracji pozostają w NVS.

[Poprzedni model prędkości i dystansu](PULSE_DISTANCE_0.09.27.md) · [Instalacja](WEB_INSTALL_CYD.md)

## Powtórka na firmware 0.09.28

Na CYD licznik wzrósł o sześć w każdej z dwóch serii. Zarejestrowane impulsy dają odpowiednio 6,52 i 9,09 km/h średnio w przedziale od pierwszego do ostatniego impulsu oraz szczyty filtra około 8,2 i 10,8 km/h. W szybszej serii pomiar dystansu przyjął dodatkowe impulsy, które nie zasiliły detektora ruchów. Kolejka nie zgubiła zdarzeń. Po zatrzymaniu dystans ustabilizował się na 59,14 m, a prędkość spadła do zera. Podczas próby log potwierdził równoczesne FTMS i tętno z Garmina.

## Wznowienie w 0.09.29

W fizycznej próbie 0.09.28 użytkownik widział około 7 km/h przy spokojnym ruchu i 9 km/h przy szybszym. Zgłosił również sporadyczne wyjście z automatycznej pauzy. W logu 0.09.27 pojedyncze zbocze potrafiło wznowić czas bez przyrostu dystansu.

W 0.09.29 trening po pauzie wznawia się dopiero po dwóch impulsach ze strumienia licznika ruchów w odstępie do 350 ms. Odstęp między nimi trafia do dystansu, a czas aktywny rusza od potwierdzenia. Zbyt późne zbocze rozpoczyna nową próbę potwierdzenia. Testy obejmują osamotnione zbocza, wznowienie po parze i przepełnienie licznika mikrosekund.
