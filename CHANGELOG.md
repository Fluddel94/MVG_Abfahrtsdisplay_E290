# Changelog

Alle nennenswerten Änderungen am Abfahrtsdisplay. Neueste Version oben.
Versionierung nach Semantic Versioning (`MAJOR.MINOR.PATCH`): PATCH =
Korrektur, MINOR = neue Funktion, MAJOR = grundlegender Umbau.

## [Unveröffentlicht]

## [2.6.0] – 08.11.2026

### Neu
- **Störungstext in der ersten Zeile:** Hat eine der ersten drei Abfahrten
  eine Störungsmeldung, zeigt die erste Zeile Warndreieck, Linie und
  Meldung (z.B. „S2: Notarzteinsatz auf der Strecke“, zu lange Texte mit
  „...“ gekürzt), darunter drei Abfahrten. Es zählt die Meldung der
  frühesten gestörten Abfahrt; Ausfälle ohne Meldung und „Fährt nur bis …“
  bleiben beim Symbol. Gilt für Station 1 (auch in der Richtungsansicht),
  nicht für Station 2; Seite 2 beginnt dann mit der vierten Abfahrt.
  Im Portal unter „Anzeige“ → „Störungen zeigen als“: *Symbol* oder
  *Text + Symbol* (Standard, auch nach einem Update).
- **Neustart bei dauerhaften Abruffehlern:** Scheitern die Abrufe trotz
  WLAN 10 Minuten am Stück, startet das Display neu – nur wenn seit dem
  Start schon einmal ein Abruf geklappt hat und das Portal nicht offen ist.

### Geändert
- Vor jedem Neustart (Firmware-Update, Werkseinstellungen, Abruffehler)
  meldet sich das Display sauber vom WLAN ab.
- Technik: neue Einstellung im Gerätespeicher `incText`; `Departure.incident`
  enthält den Text der ersten `INCIDENT`-Meldung; neue Funktion
  `restartCleanly()` in `wifi_conn`.

## [2.5.0] – 31.10.2026

### Neu
- **Störungen abschaltbar:** Häkchen „Störungen anzeigen“ im Portal unter
  „Anzeige“ (Standard: an). Ohne Häkchen kein Warndreieck; ausgefallene
  Fahrten sind weiter durchgestrichen, das tatsächliche Ziel bei
  vorzeitigem Fahrtende bleibt.
- **Full Refresh einstellbar:** Feld „Full Refresh alle … Minuten“ im
  Portal unter „Anzeige“ (Standard 10 wie bisher, 1–60).

### Geändert
- **Warndreieck an fester Stelle:** Bei Störung und Ausfall steht das
  Dreieck immer direkt links vom Trennstrich vor der Abfahrtszeit (bisher
  hinter dem Ziel bzw. bei Ausfall rechts der Zeit). Verspätung und
  Zeitspalte bleiben unverändert.
- **„Einstellungen/Info“ statt System-Log** (mittlere Taste 3 s): QR-Code
  zum Einstellungsportal, IP-Adresse, „Portal offen bis …“, Version,
  WLAN-Name, Signal (Bewertung und dBm) und Laufzeit. WLAN-Abbrüche,
  API-Fehler und Board-Start werden dort nicht mehr angezeigt.
- Portal: Eingaben werden nur noch von der Seite selbst geprüft (Fehler rot
  am Feld statt Browser-Hinweis).
- Technik: neue Einstellungen im Gerätespeicher (`showInc`, `fullRef`), bei
  einem Update gelten die bisherigen Werte (Dreieck an, alle 10 Minuten);
  `displayShowLog()` heißt jetzt `displayShowInfo()`.

## [2.4.0] – 07.10.2026

### Neu
- **Tasten-Ansicht ohne Zeitlimit:** Neues Häkchen „Tasten-Ansicht nach
  30 s beenden“ im Portal unter „Tasten“ (Standard: an). Ohne Häkchen
  bleiben Station 2, die andere Richtung oder Seite 2 stehen, bis eine
  Taste gedrückt wird.
- **WLAN-Protokoll im seriellen Monitor** (mit `DEBUG_LOG 1`): jeder
  Abbruch sofort mit Reason-Code, jede Verbindung und einmal pro Minute
  Zugangspunkt (BSSID), Kanal und Signalstärke.

### Geändert
- **WLAN stabiler:** Das Display sucht auf allen Kanälen und verbindet sich
  mit dem Zugangspunkt mit dem stärksten Signal statt mit dem ersten
  gefundenen (wichtig bei Mesh-WLAN und Repeatern). Ein neuer
  Verbindungsversuch startet erst, wenn der laufende beendet ist (Ablehnung
  durch den Router oder 10 s ohne Ergebnis), mit wachsenden Pausen (2 s bis
  30 s); nach sechs Fehlschlägen in Folge wird das WLAN-Modul kurz neu
  gestartet. Bisher kam ein fester Versuch alle 10 s einer noch laufenden
  Anmeldung in die Quere. Ein beim Start hängender erster Versuch wird so
  vor dem Fehlerbildschirm wiederholt.
- **WLAN-Fehlerbildschirm:** Eine Abmeldung durch das Display selbst
  (Reason-Code 8) gilt nicht mehr als Ursache („Verbindung unterbrochen“).
- **Tasten:** In der Ansicht einer Taste zeigt die andere Taste direkt ihre
  Ansicht (z.B. von der anderen Richtung direkt zu Station 2). Dieselbe
  Taste nochmal führt zurück zur Standardansicht. Bisher führte jeder
  zweite Druck erst zurück.
- **Portal:** Bei „Welche Richtung fährt ins Zentrum?“ heißt die dritte
  Antwort jetzt „Keine oder gemischt“ (Querverbindungen oder Linien mit
  verschiedenen Zentrum-Richtungen).
- **README:** Ungetestete Router-Tipps (fester WLAN-Kanal, nur WPA2) aus
  der Hilfe bei WLAN-Abbrüchen entfernt; Portal-Bilder aktualisiert.
- Technik: neues Modul `src/wifi_conn` – alle Verbindungsversuche (auch per
  Web-Installer) laufen darüber und ersetzen `WiFi.reconnect()`. Das
  automatische Wiederverbinden des ESP32 ist aus, außer eine Erweiterung
  übernimmt das Wiederverbinden (`extrasHandlesWifiReconnect()`).

## [2.3.0] – 05.10.2026

### Neu
- **Richtung je Station mit Bezeichnung:** Station 1 und 2 zeigen bei
  „nur Richtung H/R“ und bei umschaltbarer Richtung im Header
  „Station → Zentrum“ bzw. „→ Auswärts“, je nachdem, welche Kennung (H
  oder R) Richtung Zentrum fährt. Ohne Zuordnung („Keine“) steht bei
  umschaltbarer Richtung „→ H“/„→ R“, sonst nur der Stationsname.
  Umschaltbare Richtung (bisher „getrennt“) geht jetzt auch mit zwei
  Stationen (nur Station 1).
- **Tastenbelegung:** Linke und mittlere Taste haben je eine Funktion:
  Richtung wechseln, Station 2, weitere Abfahrten (Seite 2) oder keine;
  die mittlere Taste alternativ den WLAN-QR-Code. Ein Druck zeigt die
  Ansicht der Taste, ein weiterer Druck (auf eine der beiden Tasten) oder
  30 s später geht es zurück zur Standardansicht. Seite 2 gibt es damit
  auch mit zwei Stationen und bei umschaltbarer Richtung. Mittlere Taste
  lang drücken: wie bisher Systeminfo und Einstellportal.
- **Abfahrtszeit in Minuten** („**4** min“, „**jetzt**“, Zahl fett; ab
  60 Minuten Uhrzeit) als Alternative zur Uhrzeit, Verspätung eingerechnet (`SHOW_MINUTES`
  in `config.h`).

### Geändert
- **Einstellungsportal neu aufgebaut:** Jede Station in vier Schritten
  (Station, Verkehrsmittel, Richtung, Linien), beide Stationen gleich
  einzustellen. Die gewählte Station steht als Karte mit „Ändern“, die
  Station-ID nur noch unter „Station-ID manuell eingeben“.
  Verkehrsmittel als farbige Schalter; was an der Station nicht fährt,
  ist gestrichelt. „Richtungsanzeige“: alle Richtungen gemischt,
  umschaltbar per Taste (H/R), nur H, nur R oder je Linie (nur dann
  gibt es die Richtung in der Linienauswahl). Bei umschaltbar/nur H/nur R
  fragt das Portal „Welche Richtung fährt ins Zentrum?“ (H, R oder Keine)
  und zeigt dazu Beispielziele aus den aktuellen Abfahrten, auch direkt
  in der Auswahl (ersetzt „Richtungen anzeigen“). Unter jeder Einstellung
  steht, was das Display zeigt.
- Neue Bereiche „Anzeige“ (Uhrzeit oder Minuten) und „Tasten“
  (Standardansicht, Belegung links/Mitte mit Vorschau, WLAN-QR-Code bei
  der mittleren Taste). Eine neue Funktion (Station 2, umschaltbare
  Richtung) wird automatisch einer freien Taste zugeordnet, sonst
  erscheint ein Hinweis.
- „Speichern“ steht in einer Leiste am unteren Rand, die ungespeicherte
  Änderungen anzeigt. Gespeichert wird ohne Seitenwechsel; fehlende
  Angaben werden direkt am Feld gemeldet statt per Hinweisfenster.
- Während ein WLAN-Ausfall überbrückt wird (bis 60 s), reagieren die
  Tasten weiter (Ansichten aus dem Zwischenspeicher, QR-Code,
  Systeminfo). Bisher waren sie bis zur Rückkehr des WLANs ohne Wirkung.
- **Update von 2.2.0 und älter:** Die gespeicherten Einstellungen werden
  beim ersten Start so übernommen, dass Anzeige und Tasten gleich
  bleiben. Ein Zurückspielen von 2.2.0 funktioniert weiterhin.
- Station 2 wird nur noch abgerufen, wenn sie einer Taste zugeordnet ist.
- Die Stationsnamen werden im Gerät gespeichert: Das Portal zeigt sie
  sofort beim Öffnen, das Display braucht beim Start keine Abfrage dafür
  (bestehende Geräte holen sie einmal nach).
- README und README_TECHNIK (deutsch/englisch) überarbeitet: Bedienung,
  Einstellungsportal mit neuen Bildern, Hilfe bei WLAN-Abbrüchen; „linke
  Taste“ statt „BOOT-Taste“.

### Korrigiert
- WLAN-Stromsparmodus abgeschaltet (`WiFi.setSleep(false)`): Bei manchen
  Routern führte er zu wiederholten Verbindungsabbrüchen.

## [2.2.0] – 03.10.2026

### Neu
- **Linienauswahl** je Station im Einstellungsportal: „Linien auswählen“
  listet alle Linien der angehakten Verkehrsmittel (nach Verkehrsmittel
  und Nummer sortiert) mit Beispielzielen je Richtungskennung H/R aus den
  nächsten rund 12 Stunden. Je Linie „beide Richtungen“, „nur H“ oder
  „nur R“, höchstens 16 Linien; keine Auswahl = alle Linien. Sind Linien
  gewählt, zeigt das Display nur diese und fragt 100 statt 60 Abfahrten
  ab (`API_DEPARTURE_LIMIT_LINES`). Ersatzbusse für Regionalzüge
  erscheinen, sobald ein Regionalzug gewählt ist (die MVG nennt nur die
  Zugnummer).
- Optionale **zweite Station** mit eigenen Verkehrsmitteln, Richtung und
  Linien. Die BOOT-Taste schaltet zwischen Station 1 und 2 um, nach 30 s
  (`STATION_AUTO_RESET_MS`) und nach einem Neustart gilt wieder Station 1.
  Die Nummer steht weiß in einem schwarzen Kästchen vor dem
  Stationsnamen. Beide Stationen werden jede Minute abgerufen, das
  Umschalten geht ohne Wartezeit. Je Station vier Abfahrten; getrennte
  Ansicht Zentrum/Auswärts und Seite 2 gibt es nur mit einer Station.
- Einstellung **„Richtung“ je Station**: alle Richtungen, nur Richtung H
  oder nur Richtung R (gilt für alle Linien der Station; die
  Richtungswahl je Linie entfällt dann). Bei Station 1 zusätzlich wie
  bisher die getrennte Ansicht Zentrum/Auswärts (H/R). Ein Hinweis
  erklärt, dass H und R die Richtungskennungen der MVG sind.
  „Richtungen anzeigen“ gibt es für beide Stationen und zeigt nur die
  angehakten Verkehrsmittel (höchstens 15 Einträge).
- Einstellungsportal: Der Name der eingestellten Station steht in der
  Überschrift („Station 1: Pasing“), eine unbekannte ID wird gemeldet.
- Für Entwickler: zusätzliche Diagnose im seriellen Monitor
  (Antwortgrößen, freier Arbeitsspeicher, Einzelschritte der Linienliste)
  mit `DEBUG_LOG 1` in `config.h` (neue Datei `src/debug_log.h`),
  Standard: aus.

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
- Die Antworten der MVG werden direkt aus dem Datenstrom ausgewertet,
  statt sie erst ganz als Text zu speichern (bis 46 KB) – weniger
  Arbeitsspeicher, und der minütliche Abruf wird nur einmal statt je
  Richtung eingelesen.
- Bestehende Einstellungen bleiben beim Update erhalten und gelten für
  Station 1. Neue Werte im Gerätespeicher: `lines1`, `lines2`,
  `station2`, `types2`, `dir1`, `dir2`.

### Dokumentation
- Fotos des Displays oben in der README (neuer Ordner `bilder/`), dazu
  Bilder vom Einstellungsportal und vom Web-Installer.
- README und README_TECHNIK beschreiben Linienauswahl, Richtung je
  Station, zweite Station und das Überbrücken von Störungen.
- Englische Fassungen: `README_EN.md`, `README_TECHNIK_EN.md`.
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
