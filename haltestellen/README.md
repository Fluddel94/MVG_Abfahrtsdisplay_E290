# Haltestellenliste (globale IDs)

`MVV_Haltestellen_Report_s26.csv` enthält alle rund 11.000 Haltestellen im
MVV-Gebiet (München und Umland) mit ihrer **Globalen ID** – das ist der Wert
für `STATION_GLOBAL_ID` in `config.h`.

## Station suchen

- **Im Browser (GitHub):** die CSV-Datei anklicken und mit Strg+F nach dem
  Stationsnamen suchen.
- **In Excel:** Datei herunterladen und per Doppelklick öffnen, dann mit
  Strg+F suchen.

Die Spalten sind: `HstNummer; Name ohne Ort; Ort; Globale ID; WGS84 X; WGS84 Y`.
Beispiel:

```
2;Marienplatz;München;de:09162:2;48,1364359;11,577658
```

Der Name steht **ohne Ort** – bei gleichnamigen Haltestellen (z.B.
„Bahnhof“, „Rathaus“) auf die Spalte *Ort* achten. Den Wert aus der Spalte
*Globale ID* in `config.h` eintragen:

```cpp
#define STATION_GLOBAL_ID "de:09162:2"   // Marienplatz
```

Prüfen lässt sich die ID im Browser:
`https://www.mvg.de/api/bgw-pt/v3/departures?globalId=<ID>` – kommt eine
Liste mit Abfahrten zurück, stimmt sie.

## Quelle und Lizenz

- **Quelle:** Münchner Verkehrs- und Tarifverbund GmbH (MVV),
  „MVV Haltestellenliste“ (Datenstand 07.01.2026, Fahrplan s26),
  [MVV OpenData](https://www.mvv-muenchen.de/fahrplanauskunft/fuer-entwickler/opendata/index.html),
  abgerufen am 29.09.2026.
- **Lizenz:** [Creative Commons Namensnennung 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).
- **Änderung:** Inhalt unverändert; am Dateianfang wurde eine
  UTF-8-Kennung (BOM) ergänzt, damit Excel die Umlaute richtig anzeigt.
- Der MVV übernimmt keine Gewähr für Richtigkeit, Aktualität und
  Vollständigkeit der Daten. Die Liste wird jährlich aktualisiert.

Die GPL-Lizenz des Programmcodes gilt **nicht** für die Dateien in diesem
Ordner.
