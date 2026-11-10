# ArduRower CYD 0.09.27

W fizycznej próbie przy podobnej długości pociągnięć ekran 0.09.26 pokazywał około 8 km/h w spokojnej serii i 6 km/h w szybszej. Zapis z COM3 pokazał odpowiednio 15,7 oraz 19,2 przyjętego odstępu między impulsami na sekundę. Poprzedni model zasilał wirtualne koło zamachowe zależnie od odstępu pojedynczego impulsu i rozpoznanej fazy napędu. W rezultacie nierównomierne magnesy i podział na fazy wpływały na prędkość i całkowany dystans.

W 0.09.27 każdy przyjęty odstęp zwiększa dystans o stały krok. Domyślnie jest to 0,14 m × zapisany mnożnik 0,70, czyli 0,098 m. Prędkość to częstotliwość tych samych odstępów wygładzona filtrem wykładniczym o stałej 2,5 s i przeliczona tym samym krokiem. Ekran, wykres i chwilowe tempo FTMS korzystają z jednej prędkości. W przerwie prędkość maleje, po 5 sekundach wynosi zero, a dystans w ogóle nie rośnie bez impulsów. Licznik ruchów i kadencja nadal korzystają z istniejącego detektora faz.

Przeliczenie zarejestrowanych serii daje średnie około 5,52 i 6,77 km/h, czyli około 23% więcej w szybszej serii. To odtworzenie zapisu, a krok dystansu pozostaje estymacją. Zapisany mnożnik kalibracji można dostroić w ustawieniach CYD według znanego dystansu odniesienia.

Nowe rekordy, przebiegi tętna i medale Ghost Race trafiają do kluczy `g4_*`, `h4_*` i `w4_*`. Dane wcześniejszych przejazdów pozostają pod dotychczasowymi kluczami. Pierwszy ukończony przejazd na danym dystansie tworzy punkt odniesienia dla nowej skali.

[Instalacja i zachowanie NVS](WEB_INSTALL_CYD.md)
