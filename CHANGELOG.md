# Changelog

Alle nennenswerten Änderungen am Abfahrtsdisplay. Neueste Version oben.
Versionierung nach Semantic Versioning (`MAJOR.MINOR.PATCH`): PATCH =
Korrektur, MINOR = neue Funktion, MAJOR = grundlegender Umbau.

## [Unveröffentlicht]
### Behoben
- Haltestellenliste wurde auf GitHub nicht als durchsuchbare Tabelle
  angezeigt (Datei zu groß, Semikolon als Trennzeichen). Neu:
  `haltestellen/Haltestellen_Suche_s26.csv` mit Name, Ort und Globaler ID,
  nach Name sortiert, durch Komma getrennt (466 KB). Die Originaldatei des
  MVV bleibt für Excel erhalten.

## [1.1.0] – 29.09.2026
### Neu
- Haltestellenliste des MVV (`haltestellen/`, CC BY 4.0) mit den globalen
  IDs aller rund 11.000 Haltestellen in München und Umland. „Station finden“
  im README und der Hinweis in `config.h` verweisen jetzt darauf statt auf
  die Suche über die API.
### Behoben
- Tastendrücke gingen verloren, wenn sie in ein Display-Update oder einen
  API-Abruf fielen (die Anzeige reagierte erst beim zweiten Druck). Die
  Tasten werden jetzt per Interrupt erfasst; ein Druck während eines
  Updates wird danach ausgeführt. Gedrückt halten der BOOT-Taste löst nicht
  mehr wiederholt aus. Neue Konstanten `BUTTON_EDGE_STABLE_MS` und
  `BUTTON_LATCH_MAX_AGE_MS` in `config.h`.
- README: missverständlicher Hinweis zu den Meldungstypen präzisiert – nur
  `INCIDENT` und `EARLY_TERMINATION` lösen ein Warndreieck aus, `INFO` nicht.

## [1.0.0] – 28.09.2026
Erste öffentliche Version.

### Funktionen
- Live-Abfahrten einer MVG-/MVV-Haltestelle über die (inoffizielle)
  MVG-API, Aktualisierung einmal pro Minute.
- Anzeige gemischt mit Seite 2 oder getrennt nach Zentrum/Auswärts.
- Verkehrsmittel-Filter (S-Bahn, U-Bahn, Tram, Bus, Regionalzug).
- Liniensymbole, generierte Bus-/Zug-Symbole, echte Umlaute.
- Verspätungen, durchgestrichene Uhrzeit bei Ausfall, Warndreieck bei
  Störungen, tatsächliches Ziel bei vorzeitigem Fahrtende.
- Flügelzüge als eine Zeile.
- System-Log und Fehlerbildschirme; bei WLAN-Problemen nach 20 s mit
  vermuteter Ursache („Passwort falsch?“, „Netz nicht gefunden“).
- Andockstellen für eigene Erweiterungen (`src/extras.h`), standardmäßig
  ohne Funktion.
- Optional: WLAN-QR-Code und Firmware-Update per WLAN (OTA), beide
  standardmäßig aus.
