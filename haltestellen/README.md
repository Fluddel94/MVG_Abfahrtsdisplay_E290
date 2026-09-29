# Haltestellenliste (globale IDs)

Alle rund 11.000 Haltestellen im MVV-Gebiet (München und Umland) mit ihrer
**Globalen ID** – das ist der Wert für `STATION_GLOBAL_ID` in `config.h`.

| Datei | Inhalt | Geeignet für |
|---|---|---|
| [`Haltestellen_Suche_s26.csv`](Haltestellen_Suche_s26.csv) | Name, Ort, Globale ID – nach Name sortiert, durch Komma getrennt | Suche direkt im Browser auf GitHub |
| [`MVV_Haltestellen_Report_s26.csv`](MVV_Haltestellen_Report_s26.csv) | Originaldatei des MVV (alle Spalten inkl. Koordinaten, durch Semikolon getrennt) | Herunterladen und per Doppelklick in Excel öffnen |

## Station suchen

- **Im Browser:** [`Haltestellen_Suche_s26.csv`](Haltestellen_Suche_s26.csv)
  anklicken und oben im Suchfeld „Search this file…“ den Stationsnamen
  eingeben.
- **In Excel:** `MVV_Haltestellen_Report_s26.csv` herunterladen, per
  Doppelklick öffnen und mit Strg+F suchen.

Beispiel (Suchliste):

```
Marienplatz,München,de:09162:2
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
- **Änderungen:** `MVV_Haltestellen_Report_s26.csv`: Inhalt unverändert,
  am Dateianfang wurde eine UTF-8-Kennung (BOM) ergänzt, damit Excel die
  Umlaute richtig anzeigt. `Haltestellen_Suche_s26.csv`: aus der
  Originaldatei abgeleitet – nur die Spalten Name, Ort und Globale ID, nach
  Name sortiert, Komma statt Semikolon (GitHub zeigt CSV-Dateien nur mit
  Komma und bis 512 KB als durchsuchbare Tabelle an).
- Der MVV übernimmt keine Gewähr für Richtigkeit, Aktualität und
  Vollständigkeit der Daten. Die Liste wird jährlich aktualisiert.

Die GPL-Lizenz des Programmcodes gilt **nicht** für die Dateien in diesem
Ordner.
