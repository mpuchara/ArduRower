# Obrót ekranu: 0.09.25

Na życzenie użytkownika TFT ST7789 ma orientację 1 zamiast 3: obraz 320×240 jest obrócony o 180° względem poprzedniej wersji.

Zapisana kalibracja dotyku nie jest kasowana. Istniejąca transformacja przelicza współrzędne ze starej orientacji przez x=319-x i y=239-y. Brak znacznika orientacji w starszej kalibracji oznacza historyczną orientację 3. Nowa kalibracja zapisuje orientację 1; nie jest później obracana ponownie. Również awaryjne mapowanie nieskalibrowanego dotyku uwzględnia obrót. Wykrywanie poziomego gestu w obu kierunkach pozostaje zgodne.

Kompilacja ST7789/GPIO21 zakończona sukcesem, przeszło 6 istniejących sprawdzeń Python. Wgrano aplikację na COM6 od 0x10000, esptool potwierdził hash zapisu. Boot zgłosił 0.09.25 i poprawnie wczytaną kalibrację orientacji 3, używaną z obrotem współrzędnych. Bez kasowania NVS, rekordów, medali i parowania BLE. Pole zegarka pozostaje w wersji 0.1.5.

Fizyczną orientację obrazu i trafianie w przyciski może ostatecznie potwierdzić użytkownik patrzący na ekran; odczyt USB i log startu nie są wizualnym potwierdzeniem.
