# Changelog

Alle nennenswerten Änderungen am Abfahrtsdisplay. Neueste Version oben.
Versionierung nach Semantic Versioning (`MAJOR.MINOR.PATCH`): PATCH =
Korrektur, MINOR = neue Funktion, MAJOR = grundlegender Umbau.

## [Unveröffentlicht]

### Neu
- Einstellungsportal: **Linienauswahl** je Station. „Linien auswählen“
  listet alle Linien der Station, nach Verkehrsmittel und Nummer sortiert,
  mit Beispielzielen je Richtungskennung H/R. Je Linie „beide Richtungen“,
  „nur H“ oder „nur R“, höchstens 16 Linien; keine Auswahl = alle Linien.
  Die Liste enthält nur Linien der angehakten Verkehrsmittel (und bereits
  gewählte Linien). Sind Linien gewählt, zeigt das Display nur diese.
- Optionale **zweite Station** mit eigenen Verkehrsmitteln, Richtung und
  Linien (im Einstellungsportal). Die BOOT-Taste schaltet dann zwischen
  Station 1 und 2 um, nach 30 s (`STATION_AUTO_RESET_MS`) geht es zurück
  zu Station 1, nach dem Neustart ebenso. Die Nummer steht weiß in einem
  schwarzen Kästchen vor dem Stationsnamen. Beide Stationen werden jede
  Minute abgerufen, das Umschalten geht daher ohne Wartezeit. Je Station
  vier Abfahrten; getrennte Ansicht Zentrum/Auswärts und Seite 2 gibt es
  nur mit einer Station.
- Einstellung **„Richtung“ je Station**: alle Richtungen, nur Richtung H
  oder nur Richtung R (gilt für alle Linien der Station; die
  Richtungswahl je Linie entfällt dann). Bei Station 1 zusätzlich wie bisher die getrennte
  Ansicht Zentrum/Auswärts (H/R). Ein Hinweis erklärt, dass H und R die
  Richtungskennungen der MVG sind. Neue Werte im Gerätespeicher: `dir1`, `dir2`.
  „Richtungen anzeigen“ gibt es jetzt für beide Stationen und zeigt nur
  die angehakten Verkehrsmittel (höchstens 15 Einträge).
- Einstellungsportal: Der Name der eingestellten Station steht in der
  Überschrift („Station 1: Pasing“), nicht nur die ID; eine unbekannte ID
  wird gemeldet.

### Geändert
- Kurze Störungen der MVG-API (z.B. HTTP 503 oder keine Antwort) werden
  überbrückt: Die bisherigen Abfahrten bleiben stehen (Uhrzeit läuft
  weiter), alle 10 s gibt es einen neuen Versuch, ohne die Tasten zu
  blockieren. Der Fehlerbildschirm erscheint erst, wenn 60 s lang kein
  Abruf geklappt hat (`API_ERROR_SCREEN_DELAY_MS`). Bisher kam er nach
  zwei Versuchen im Abstand von 2 s.
- Ebenso bei WLAN-Abbrüchen im Betrieb: Die Abfahrten bleiben 60 s stehen
  (bisher 20 s, `WIFI_LOST_SCREEN_DELAY_MS`), die Uhrzeit läuft weiter,
  im Hintergrund wird alle 10 s neu verbunden. Beim Start bleibt es bei
  20 s (`WIFI_ERROR_SCREEN_DELAY_MS`).
- Schienenersatzverkehr wird als „SEV“ im Bus-Rahmen angezeigt. Bisher
  erschien je nach Fall die Zugnummer (z.B. „67116“ in Weilheim) oder die
  ersetzte Linie (z.B. „S2“), ohne Hinweis auf den Ersatzbus.
- Gekoppelte Regionalzüge mit verschiedenen Liniennummern erscheinen als
  eine Zeile mit dem Symbol „RB“ bzw. „RE“ und allen Zielen (bisher eine
  Zeile je Zugteil): BRB RB 55/56/57, RB 6/60, RB 65/66, RE 80/89. Lange
  Ziele werden gleichmäßig gekürzt, angezeigt wird die kleinste
  Verspätung der Zugteile.
- Geteilte Züge (z.B. S1 Flughafen / Freising) zeigen jetzt ebenfalls die
  kleinste statt der größten Verspätung der Zugteile.

### Intern
- Einstellungsportal: neue Abfrage `/linien?id=…` liefert alle Linien einer
  Station (MVG-Endpunkt `lines`) mit Beispielzielen je Richtungskennung
  H/R aus den Abfahrten der nächsten rund 12 Stunden – Grundlage für die
  geplante Linienauswahl. Die Antworten werden direkt aus dem Datenstrom
  ausgewertet (spart Arbeitsspeicher); der serielle Monitor zeigt dabei
  Dauer und freien Arbeitsspeicher.
- Der minütliche Abruf der Abfahrten wird ebenfalls direkt aus dem
  Datenstrom ausgewertet, statt die ganze Antwort (bis 46 KB) erst als Text
  zu speichern, und nur noch einmal statt je Richtung eingelesen.
- Einstellungen für Linienauswahl und zweite Station vorbereitet (neue
  Werte im Gerätespeicher: `lines1`, `station2`, `types2`, `lines2`).
  Bestehende Einstellungen bleiben beim Update erhalten und gelten für
  Station 1. Mit gewählten Linien werden 100 statt 60 Abfahrten abgefragt
  (`API_DEPARTURE_LIMIT_LINES`), die Verkehrsmittel ergeben sich dann aus
  den gewählten Linien. Ersatzbusse für Regionalzüge erscheinen, sobald
  ein Regionalzug gewählt ist (die API nennt nur die Zugnummer).

### Dokumentation
- Fotos des Displays oben in der README (neuer Ordner `bilder/`).
- README: Link zum Gehäuse auf MakerWorld, kurzer Hinweis zur Entwicklung
  mit Agentic Coding.

## [2.1.0] – 01.10.2026

### Behoben
- Zu wenige Abfahrten an großen Stationen, wenn nur ein Verkehrsmittel
  gewählt ist (z.B. Moosach, nur S-Bahn, eine Richtung: nur 2 Zeilen).
  Die MVG zählt das Abfragelimit über alle Verkehrsmittel der Station.
  Das Display fragt jetzt 60 statt 20 Abfahrten ab und liest davon nur die
  benötigten Angaben ein (weniger Speicherbedarf).

### Geändert
- Misslingt ein Abruf der Abfahrten, versucht das Display es nach 2 s
  still ein zweites Mal. Erst wenn auch das scheitert, erscheint der
  Fehlerbildschirm und der Abruf zählt als API-Störung – einzelne
  Aussetzer (z.B. Zeitüberschreitung) bleiben unsichtbar.

### Intern
- Kommentare und technische Dokumentation überarbeitet.

## [2.0.0] – 30.09.2026

Einrichtung ohne Arduino IDE: Firmware über den Web-Installer im Browser
aufspielen, WLAN per USB eintragen, alles Weitere im Einstellungsportal.

### Umstieg von 1.x
- Die Einstellungen liegen jetzt im Gerät (Einstellungsspeicher) und
  bleiben bei Updates erhalten. `config.h` und `secrets.h` liefern nur
  noch Standardwerte für ein Gerät ohne gespeicherte Einstellungen.
- Umstieg am einfachsten über den Web-Installer: einmal mit „Erase device“
  installieren und neu einrichten (WLAN, Station im Portal).
- Wer weiter per Arduino IDE aufspielt: Boardpaket esp32 3.3.12, Anleitung
  in `README_TECHNIK.md`. Arduino-OTA gibt es nicht mehr – Updates über
  den Web-Installer oder den Firmware-Upload im Portal.

### Neu
- Web-Installer (`docs/`, über GitHub Pages): Firmware ohne Arduino IDE
  per USB aus Chrome, Edge oder Firefox (ab 151) aufspielen (ESP Web
  Tools), danach WLAN verbinden und die Einstellungen öffnen. Ein Update
  über den Installer behält alle Einstellungen (Firmware in vier Teilen
  statt merged .bin).
- Firmware-Update im Einstellungsportal: Firmware-Datei (.bin) auswählen
  und hochladen, mit Fortschrittsanzeige. Das Display zeigt „Firmware-Update
  läuft“ und startet danach neu, alle Einstellungen bleiben erhalten.
  Geprüft wird vorher, ob die Datei eine Firmware für dieses Display
  (ESP32-S3, Kennung des Projekts) ist; bei Fehler oder Abbruch läuft die
  bisherige Firmware weiter.
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
- Einstellungsportal: Der Hilfe-Link führt direkt zum Abschnitt
  „Einstellungsportal“ der README, der Hinweis zum WLAN-Wechsel nennt den
  Knopf des Web-Installers („Change Wi-Fi“).
- README aufgeteilt: `README.md` ist jetzt eine einfache Anleitung für
  Nutzer (Einrichtung per Web-Installer, Bedienung, Einstellungsportal,
  Update, WLAN ändern, „Hilfe bei Problemen“ inkl. Notlösung per
  BOOT-Taste). Neu `README_TECHNIK.md` für Technikinteressierte: Selbst
  kompilieren (Boardpaket esp32 3.3.12), Firmware für den Web-Installer
  bauen, Station-ID und Richtungen von Hand, Einschränkungen im Detail,
  Dateien und technische Hinweise.
- Web-Installer-Seite: Hinweise zu leerem Board (PC piept im
  Sekundentakt), Neustart nach der BOOT-Notlösung, fehlendem WLAN in der
  Liste und zum Häkchen „Erase device“ bei Updates.
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
  `OTA_HOSTNAME`, `otaPassword` in `secrets.h`). Ersatz: Firmware-Upload
  im Einstellungsportal.

### Intern
- `werkzeuge/firmware_fuer_installer.ps1`: übernimmt den IDE-Export nach
  `docs/firmware/`, setzt die Version im Manifest und bricht ab, wenn die
  Firmware mit secrets.h gebaut wurde. `.gitignore`: `secrets.h*` (auch
  umbenannte Zugangsdaten), Ausnahme für `docs/firmware/*.bin`.
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
