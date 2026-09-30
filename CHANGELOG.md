# Changelog

Alle nennenswerten Änderungen am Abfahrtsdisplay. Neueste Version oben.
Versionierung nach Semantic Versioning (`MAJOR.MINOR.PATCH`): PATCH =
Korrektur, MINOR = neue Funktion, MAJOR = grundlegender Umbau.

## [Unveröffentlicht]

### Neu
- Einstellungsportal: Webseite im Heimnetz zum Ändern aller Einstellungen
  (Station, Anzeige, Verkehrsmittel, WLAN-QR, Werkseinstellungen), ohne
  Neustart wirksam. Öffnet zusammen mit dem System-Log für 30 Minuten
  (erneutes Öffnen startet die Zeit neu), nach „WLAN verbinden“ im
  Web-Installer und – solange keine Station gespeichert ist – dauerhaft mit
  einem Einrichtungs-Screen (QR-Code und Adresse).
- Stationssuche per Name im Portal (über die inoffizielle MVG-API): bis zu
  15 Treffer mit Ort, Tarifzone, Verkehrsmitteln, ID und Link auf die
  Karte, damit gleichnamige Haltestellen unterscheidbar sind. Die
  Station-ID bleibt als Rückfall eintragbar.
- „Richtungen anzeigen“ im Portal: listet Linien und Ziele je
  Richtungskennung (H/R) – damit lässt sich „Richtung Zentrum“ ohne Blick
  in die API-Antwort festlegen.
- WLAN-Einrichtung per USB aus dem Browser (Improv Serial): WLAN-Name und
  Passwort lassen sich ohne Arduino IDE setzen und ändern, das Gerät meldet
  Name und Version und liefert eine Liste der sichtbaren Netze. Neue Daten
  werden erst gespeichert, wenn die Verbindung klappt – sonst bleibt das
  bisherige WLAN. Funktioniert auch, während der WLAN-Fehlerbildschirm
  angezeigt wird.

### Behoben
- Hängt das Board am PC, ohne dass der serielle Monitor offen ist, waren
  Start, Tasten und Übernahme von Einstellungen sehr träge (jede
  Ausgabe über USB wartete bis zu 2 s). Ausgaben warten jetzt nicht mehr.

### Geändert
- System-Log: neue Zeilen mit der Portal-Adresse und bis wann es erreichbar
  ist; die WLAN-Laufzeit entfällt, WLAN-Abbrüche und API-Störungen stehen
  in einer Zeile.
- `STATION_GLOBAL_ID` in `config.h` ist standardmäßig leer: Neue Geräte
  starten mit dem Einrichtungs-Screen.
- WLAN-Fehlerbildschirm: Hinweis „Passwort prüfen (Web-Installer)“ statt
  „secrets.h prüfen“. Ohne WLAN-Daten erscheint „Keine WLAN-Daten –
  Einrichten per Web-Installer“.
- Die Geräte-Einstellungen (WLAN, Station, Anzeige, Verkehrsmittel,
  WLAN-QR) werden beim Start aus dem Gerätespeicher (NVS) gelesen. Solange
  dort nichts gespeichert ist, gelten wie bisher die Werte aus `config.h`
  und `secrets.h`. Grundlage für die Einrichtung ohne Arduino IDE.
- `secrets.h` ist nur noch optional (Vorbelegung für WLAN und WLAN-QR).
- `QR_SCREEN_TITLE` in `config.h` wird jetzt als normaler Text geschrieben,
  Umlaute direkt (bisher als Latin-1-Escape).
- Das Partitionsschema liegt als `partitions.csv` im Sketch-Ordner
  (identisch mit dem bisherigen Standard `default_8MB`), damit gespeicherte
  Einstellungen auch künftige Updates überstehen.

### Entfernt
- Firmware-Update per WLAN über die Arduino IDE (`FEATURE_OTA`,
  `OTA_HOSTNAME`, `otaPassword` in `secrets.h`). Ersatz folgt als
  Firmware-Upload im Einstellungsportal.

### Intern
- Neue Andockstelle `extrasWifiChanged()` in `src/extras.h` (neue WLAN-Daten
  gespeichert).
- Quelltext: Blockmarker und `#pragma region` in der .ino entfernt, die
  Abschnitte haben jetzt normale Überschriften (keine Funktionsänderung).

## [1.2.0] – 30.09.2026
### Neu
- Startbildschirm: Nach dem Einschalten erscheinen etwa 5 Sekunden lang
  Projektname und Firmware-Version. WLAN, Uhrzeit und erster Abruf laufen
  im Hintergrund weiter, danach folgen direkt die Abfahrten bzw. ein
  Fehlerbildschirm. Dauer über `SPLASH_DURATION_MS` in `config.h`.

### Behoben
- Fahrten mit „Fährt nur bis …“, die diese Haltestelle gar nicht mehr
  erreichen (in der API an dieser Station als ausgefallen markiert), wurden
  als normale Abfahrt zum neuen Endhalt angezeigt (z.B. „Ostbahnhof ⚠“ in
  Karlsfeld bei einer Stammstrecken-Störung). Sie erscheinen jetzt als
  Ausfall mit ursprünglichem Ziel. Hält der Zug noch hier und endet nur
  früher, wird wie bisher das tatsächliche Ziel angezeigt. Neue
  Diagnose-Ausgabe im seriellen Monitor für Fahrten mit vorzeitigem Ende.
- Doppelte Zeilen bei Störungen: Wurden die Zugteile eines Flügelzugs nicht
  vereinigt, erschien dieselbe Fahrt zweimal (z.B. zweimal „S1
  Leuchtenbergring“). Gleiche Fahrten werden jetzt zu einer Zeile
  zusammengefasst. Flügelzüge werden auch bei vorzeitigem Fahrtende
  zusammengefasst (z.B. „Flugh./Freising“ durchgestrichen, wenn beide
  Zugteile ausfallen); fällt nur ein Zugteil aus, bleibt er als eigene Zeile
  sichtbar.
- Haltestellenliste wurde auf GitHub nicht als durchsuchbare Tabelle
  angezeigt (Datei zu groß, Semikolon als Trennzeichen). Neu:
  `haltestellen/Haltestellen_Suche_s26.csv` mit Name, Ort und Globaler ID,
  nach Name sortiert, durch Komma getrennt (466 KB). Die Originaldatei des
  MVV bleibt für Excel erhalten. Der Hinweis in `config.h` (Schritt 1)
  verweist jetzt auf die Suchliste.

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
