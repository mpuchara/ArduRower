# Ghost Race 0.09.37 — wybór 3000, 6000 i 10000 m

## Zakres zmiany względem działającego CYD 0.09.36

Menu zawodów z duchem pokazuje **3000 m**, **6000 m**, **10000 m** w tej kolejności. 1000 m nie jest już oferowane na ekranie. Dotknięcie całego wiersza uruchamia podgląd wyścigu, a BOOT: krótkie naciśnięcie zmienia pozycję, przytrzymanie >1,5 s zatwierdza. Przełączanie widoku nie rozpoczyna nowej sesji, zgodnie z zasadami 0.09.36.

Trzy dystanse są nadal równolegle mierzone od początku treningu niezależnie od widoku. Po ukończeniu dystansu poprawiony rekord automatycznie zapisuje się w NVS; dodatkowy zapis przebiegu tętna oraz liczba medali są przypisane do tego samego dystansu. Kolory przewagi, interpolacja mety, automatyczna pauza i odtwarzanie ducha są niezmienione.

### Zachowanie rekordów

| Dystans | Klucz rekordu | Puls | Medale |
| --- | --- | --- | --- |
| 3000 m | `g7_3000` | `h7_3000` | `w7_3000` |
| 6000 m (nowy) | `g7_6000` | `h7_6000` | `w7_6000` |
| 10000 m | `g7_10000` | `h7_10000` | `w7_10000` |

Stary klucz 1 km `g7_1000` (także `h7_1000` i `w7_1000`) pozostaje w NVS, ale nie jest odczytywany przez nowe menu. **Nie** przenosimy rekordu 1000 m na 6000 m ani go nie kasujemy. Rekordy 3 i 10 km zachowują swoje dotychczasowe nazwy, format, sumy kontrolne i zależność od kalibracji.

Dla nowego dystansu 6000 m próbkujemy co 6 sekund przy rozdzielczości 0,1 m (60000 jednostek mieści się w `uint16_t`). Dla 3000 m pozostaje 4 s / 0,1 m, dla 10000 m 10 s / 0,2 m. Przy bardzo długim treningu dotychczasowy mechanizm podwaja interwał próbkowania i redukuje równolegle próbki dystansu/tętna. Pomiar końcowego czasu jest interpolowany niezależnie od interwału próbkowania.

### Ochrona sprawdzonej 0.09.36

Nie zmieniono parametrów kalibracji, detektora ruchu, interfejsu FTMS `WR S4BL3`, pakietów wioślarskich, kanału HR Garmin, wyglądu treningu, gestów ani algorytmu wyścigu. Wersja zmienia stałą listę dystansów, klucze trzech slotów i opisy UI.

Instalator [GitHub Pages](https://mpuchara.github.io/ArduRower/) akceptuje wyłącznie buildy z prefiksem `0.09.37-ghost-distances-` i zgodnymi obrazami BIN. Publikacja nastąpi po scaleniu PR do `main` i poprawnym zakończeniu workflow instalatora. Przed instalacją upewnij się, że widzisz ten prefiks, a na CYD po restarcie wersję `0.09.37`. **Nie wybieraj pełnego kasowania flash**: utracisz rekordy NVS i kalibrację.

### Testy do wykonania

- Test hostowy: lista 3/6/10 km; istniejący 1 km jako zapis historyczny; trzy równoległe czasy mety; nowa trasa 6000 m, poprawne próbki, rekord, brak zderzenia z 3 i 10 km.
- Test statyczny: dotykowe wiersze 3000/6000/10000, brak resetu sesji przy zmianie widoku, nowe niezależne klucze NVS, niezmienione BLE.
- Test fizyczny po kompilacji i instalacji: menu po dotknięciu i BOOT, 3/6/10 km, widoczność dotychczasowego rekordu 3 km, wybór 6000 m, brak podmiany rekordów po restarcie, nadal poprawny FTMS i zwrotny HR.
