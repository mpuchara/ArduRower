# Ghost Race 0.09.36 — rekordy niezależne od widoku

- W menu są 1000, 3000 i 10000 m. Każdy trening od wyzerowania ma trzy równolegle śledzone czasy osiągnięcia dystansu. Nie trzeba otwierać trybu ducha. Nie jest to rekord dowolnego przesuwnego odcinka w środku sesji.
- Dotknięcie wyboru dystansu, zamknięcie widoku lub zmiana dystansu nie zerują treningu. Nową sesję rozpoczyna długie naciśnięcie BOOT w zwykłym widoku albo jawny przycisk nowego wyścigu na ekranie wyniku.
- Wyświetlany znak plus oznacza czas zyskany względem poprzedniego rekordu przy tym samym przebytym dystansie, minus — stratę. Żółty w zerze, zielony przy przewadze, czerwony przy stracie. Nasycenie rośnie liniowo do 5% czasu ducha w danym punkcie, z minimalnym zakresem 3 s. Po ukończeniu porównanie używa dokładnego czasu mety.
- Pauza korzysta nadal z aktywnego zegara treningu: 8 s bez impulsów, cztery impulsy do wznowienia. Rekord zapisuje się tylko przy ukończeniu dystansu i poprawieniu poprzedniego wyniku. Wolniejszy wynik i remis nie nadpisują go. W przypadku pierwszego wyniku nie ma przeciwnika.
- Zachowany jest dotychczasowy model dystansu, kalibracja, FTMS i klucz NVS 1 km. Poprzednie rekordy 1 km v3 są czytane bez migracji; 500/2000 m nie są już wybieralne, ale ich klucze nie są usuwane.
- Format v4 ma niezmieniony rozmiar logiczny, kompaktowy zapis do NVS i próbki początkowo co 2/4/10 s. Dla 10 km rozdzielczość dystansu 0,2 m zapobiega przepełnieniu 16 bitów. W długiej sesji odstęp próbek podwaja się, a zachowane próbki dystansu i HR są redukowane równolegle. Czas mety interpolowany jest na przecięciu dystansu, niezależnie od częstotliwości próbkowania. Zapis jest weryfikowany sumą kontrolną i skalą kalibracji.
- Przy aktualizacji nie kasuj flash. Instalator: https://mpuchara.github.io/ArduRower/

## Weryfikacja

Testy hostowe obejmują trzy równoczesne kamienie milowe, zgodność rekordu 1 km, niepoprawienie rekordu przy gorszym czasie, remis, interpolację mety, pauzę, długi 10 km, redukcję próbek HR i gradient. Testy źródłowe pilnują, aby wybór i zamknięcie widoku nie zerowały treningu. Test na fizycznym CYD nadal potrzebny: przejechać 1 km bez ducha, sprawdzić rekord po restarcie, rozpocząć nową sesję i porównać kolory, następnie sprawdzić 3 i 10 km oraz zachowanie po przełączaniu widoku.
