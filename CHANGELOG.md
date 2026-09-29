# Changelog

Alle nennenswerten Änderungen am Abfahrtsdisplay. Neueste Version oben.
Versionierung nach Semantic Versioning (`MAJOR.MINOR.PATCH`): PATCH =
Korrektur, MINOR = neue Funktion, MAJOR = grundlegender Umbau.

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
