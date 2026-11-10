# Technik & Selbst kompilieren – MVG-Abfahrtsdisplay (Heltec Vision Master E290)

**Deutsch** | [English](README_TECHNIK_EN.md)

Ergänzung zur [README](README.md) für alle, die den Code ändern, die
Firmware selbst bauen oder genauer wissen wollen, wie das Display
arbeitet. **Zum Einrichten wird nichts davon gebraucht** – dafür reicht der
[Web-Installer](https://fluddel94.github.io/MVG_Abfahrtsdisplay_E290/).

## Inhalt

- [Selbst kompilieren](#selbst-kompilieren)
- [Station-ID und Richtungen von Hand](#station-id-und-richtungen-von-hand)
- [Einschränkungen im Detail](#einschränkungen-im-detail)
- [Dateien](#dateien)
- [Technische Hinweise](#technische-hinweise-für-änderungen-am-code)

## Selbst kompilieren

### Arduino IDE vorbereiten

1. **Boardpaket:** *Werkzeuge → Board → Boardverwalter*, nach „esp32“
   suchen und **esp32 von Espressif Systems** installieren (entwickelt mit
   Version 3.3.12).
2. **Libraries:** *Werkzeuge → Bibliotheken verwalten*, installieren:

   | Library | Hinweis |
   |---|---|
   | heltec-eink-modules | Display-Treiber inkl. Grafikfunktionen und Schriften (getestet mit 4.6.0) |
   | ArduinoJson | Version 7 |

   Mehr ist nicht nötig: WLAN, HTTPS, Webserver, der Einstellungsspeicher
   und der QR-Generator stecken im ESP32-Boardpaket, die Grafikfunktionen
   (abgeleitet von Adafruit GFX) in heltec-eink-modules. Die Library
   „QRCode“ von Richard Moore darf **nicht** installiert sein: Ihre
   `qrcode.h` heißt genauso wie die des Boardpakets und würde stattdessen
   eingebunden.
3. **Board auswählen:** *Werkzeuge → Board → esp32 →* **Heltec Vision
   Master E290**. Die übrigen Board-Einstellungen bleiben auf Standard.

### Projekt herunterladen

Auf GitHub *Code → Download ZIP*, entpacken. Der Ordner muss
`MVG_Abfahrtsdisplay_E290` heißen – genau wie die `.ino`-Datei darin,
sonst öffnet die Arduino IDE den Sketch nicht (beim ZIP-Download heißt er
`MVG_Abfahrtsdisplay_E290-main` und muss umbenannt werden). Wer mit Git
arbeitet (`git clone`), bekommt den Ordner direkt mit dem richtigen Namen.

### Standardwerte und Hochladen

- **`config.h`** enthält die Standardwerte aller Einstellungen (Station,
  optional zweite Station, Anzeige, Verkehrsmittel, WLAN-QR). Sie gelten
  nur, solange im Gerät noch nichts gespeichert ist – im Portal
  gespeicherte Werte haben immer Vorrang. Linienauswahl, Richtung je
  Station (außer „umschaltbar“ über `FEATURE_DIRECTION_VIEW`), Zentrum-
  Zuordnung und Tastenbelegung gibt es nur im Portal. `SHOW_MINUTES`
  legt fest, ob die Abfahrtszeit als Uhrzeit oder in Minuten erscheint.
- **Fehlersuche:** `DEBUG_LOG 1` in `config.h` schaltet zusätzliche
  Ausgaben im seriellen Monitor ein (Antwortgrößen, freier Arbeitsspeicher,
  Einzelschritte der Linienliste). Für den Alltag und Releases `0`.
- **`secrets.h`** ist optional und belegt WLAN und WLAN-QR vor: Vorlage
  `secrets_example.h` kopieren, in `secrets.h` umbenennen, Werte eintragen.
  `secrets.h` steht in `.gitignore` und darf nie weitergegeben werden –
  ebenso keine Firmware, die damit gebaut wurde (sie enthält die
  Zugangsdaten).
- **Hochladen** wie gewohnt per USB. Die gespeicherten Einstellungen
  bleiben erhalten, solange in der IDE *Erase All Flash Before Sketch
  Upload* ausgeschaltet ist (Standard).
- **`partitions.csv` nie ändern** – sonst gehen die gespeicherten
  Einstellungen bei Updates verloren.

### Firmware für den Web-Installer bauen

Für Mitentwickler, die eine neue Version veröffentlichen:

1. `secrets.h` aus dem Sketch-Ordner entfernen (die Firmware darf keine
   Zugangsdaten enthalten).
2. Arduino IDE: *Sketch → Kompilierte Binärdatei exportieren*.
3. Doppelklick auf `werkzeuge/firmware_fuer_installer.bat`. Das Skript
   kopiert Bootloader, Partitionstabelle und Firmware nach `docs/firmware/`
   und trägt die Version aus `FW_VERSION` in `docs/manifest.json` ein.
   Vorher prüft es die Dateien und bricht ab, wenn die Firmware mit
   `secrets.h` gebaut wurde.
4. Committen und pushen – GitHub Pages veröffentlicht den Ordner `docs/`.

Die Firmware liegt bewusst in vier Teilen vor (Bootloader, Partitionen,
`boot_app0.bin`, Firmware) statt als ein Gesamtabbild: So überschreibt ein
Update über den Installer den Einstellungsspeicher nicht.

## Station-ID und Richtungen von Hand

Normalerweise sucht man die Station im
[Einstellungsportal](README.md#einstellungsportal) per Name. Die ID
(`globalId`, Format `de:09162:2`) lässt sich auch von Hand eintragen – im
Portal im Feld *Station-ID* oder als Standardwert `STATION_GLOBAL_ID`
(bzw. `STATION2_GLOBAL_ID`) in `config.h`:

1. Die Haltestellenliste öffnen:
   [`haltestellen/Haltestellen_Suche_s26.csv`](haltestellen/Haltestellen_Suche_s26.csv)
   – auf GitHub wird sie als Tabelle mit Suchfeld angezeigt. Für Excel gibt
   es die Originaldatei des MVV im selben Ordner.
2. Im Suchfeld („Search this file…“) den Stationsnamen eingeben. Bei
   gleichnamigen Haltestellen auf die Spalte *Ort* achten.
3. Den Wert aus der Spalte *Globale ID* übernehmen.
4. Optional prüfen: `https://www.mvg.de/api/bgw-pt/v3/departures?globalId=<ID>`
   im Browser aufrufen – kommt eine Liste mit Abfahrten zurück, stimmt die ID.

Mehr zur Liste (Spalten, Quelle, Lizenz): [`haltestellen/README.md`](haltestellen/README.md).

### Richtungen prüfen

Nötig für „umschaltbar“ und „nur Richtung H/R“. Am einfachsten im Portal:
Es zeigt bei der Richtungsanzeige Beispielziele je Kennung, ausführlicher
unter *Linien auswählen*. Von Hand: in der Abfahrtsliste aus Schritt 4 bei
einigen Einträgen `lineId` (enthält `:H:` oder `:R:`) mit `destination`
vergleichen. Eine feste Regel gibt es nicht – das hängt von Linie und
Station ab.

## Einschränkungen im Detail

- **Linienauswahl:** Die Liste im Portal kommt aus dem MVG-Endpunkt
  `lines/<globalId>` (alle Linien der Station); Sammel-Einträge wie
  „S6/8“ und Ersatzverkehr werden ausgeblendet. Die Beispielziele je
  Kennung H/R stammen aus vier Abfahrtsabrufen über rund 12 Stunden
  (`offsetInMinutes` 0/120/360/720, je 100 Fahrten) – Linien ohne Fahrt in
  dieser Zeit zeigen „keine Fahrt in den nächsten 12 h“. Höchstens 16
  gewählte Linien je Station (`LINE_SELECT_MAX`), gesammelt werden bis zu
  80 Linien (`LINE_LIST_MAX`). Verglichen wird das Linien-Label ohne
  Leerzeichen in Großbuchstaben („RE 80“ = „RE80“).
- **Ersatzverkehr bei Linienauswahl:** Ersatzbusse für S-Bahn und Tram
  tragen die ersetzte Linie als Label und passen genau. Ersatzbusse für
  Regionalzüge tragen nur die Zugnummer – sie erscheinen, sobald
  irgendein Regionalzug gewählt ist.
- Ob die Richtungsmarker `:H:`/`:R:` bei Bus, Tram und U-Bahn überall so
  zuverlässig sind wie bei der S-Bahn, ist nicht getestet. Tangentiallinien
  fahren weder Richtung Zentrum noch stadtauswärts – dort ist die gemischte
  Anzeige oft die bessere Wahl.
- Pünktliche Abfahrten meldet die API ohne Echtzeit-Kennzeichen. „Live und
  pünktlich“ ist daher nicht von „keine Live-Daten“ zu unterscheiden.
- Abgefragt werden 60 Abfahrten (`API_DEPARTURE_LIMIT`), mit
  Linienauswahl 100 (`API_DEPARTURE_LIMIT_LINES`, Maximum der API). Die API
  zählt dieses Limit über **alle** Verkehrsmittel der Station und filtert
  erst danach (beobachtet, auch mit `transportTypes`). An großen Stationen
  reicht die Vorschau damit für etwa 60–90 Minuten; selten fahrende Linien
  können weniger als 4 Zeilen ergeben.
- Außerhalb Münchens liefert die MVG den Haltestellennamen ohne Ort
  (`name` „Stadt Busbahnhof“, `place` „Wasserburg am Inn“); angezeigt wird
  nur `name`.
- Der Header kürzt lange Stationsnamen automatisch („.“ am Ende).
- Das Einstellungsportal hat kein Passwort und nutzt `http`: Für Geräte
  im Heimnetz gibt es keine anerkannten Zertifikate, ein selbst signiertes
  führt zu einer ganzseitigen Browser-Warnung. Deshalb öffnet das Portal
  nur auf Abruf und schließt sich nach 30 Minuten.

## Dateien

Im Hauptordner liegt nur, was pro Gerät angepasst wird; der Programmcode
liegt in `src/`. Der Unterordner **muss** `src` heißen – die Arduino IDE
kompiliert außer dem Hauptordner nur diesen Ordner.

| Datei | Inhalt |
|---|---|
| `MVG_Abfahrtsdisplay_E290.ino` | Ablaufsteuerung: `setup()`, `loop()`, Tasten-Aktionen, WLAN-Fehleranzeige |
| `config.h` | Standardwerte der **Geräte-Einstellungen** und gemeinsame Konstanten |
| `secrets_example.h` | Vorlage für `secrets.h` (optional: WLAN-Vorbelegung) |
| `partitions.csv` | Partitionsschema – nie ändern, sonst gehen die gespeicherten Einstellungen bei Updates verloren |
| `src/settings.h/.cpp` | Einstellungen im Gerätespeicher (NVS), Standardwerte aus `config.h` |
| `src/improv_serial.h/.cpp` | WLAN-Einrichtung per USB aus dem Browser (Improv Serial) |
| `src/portal.h/.cpp` | Einstellungsportal im Heimnetz (Webseite, Stationssuche, Richtungen, Linienliste, Firmware-Upload) |
| `src/mvg_api.h/.cpp` | Abruf und Auswertung der MVG-API |
| `src/line_select.h/.cpp` | Linienauswahl: Linienliste fürs Portal sammeln, gespeicherte Auswahl auswerten (reine Datenlogik) |
| `src/debug_log.h` | Zusätzliche Diagnose im seriellen Monitor (`DEBUG_LOG` in `config.h`) |
| `src/display.h/.cpp` | Alles, was gezeichnet wird |
| `src/line_icons.h` | Liniensymbole und Generator für Bus-/Zug-Symbole |
| `src/buttons.h/.cpp` | Tastenauswertung (Entprellen, Kurz-/Lang-Druck; ein Druck zählt nur beim Wechsel losgelassen → gedrückt) |
| `src/stats.h/.cpp` | API-Störungsstatistik, WLAN-Signalbewertung |
| `src/time_utils.h/.cpp` | Laufzeit, Zeitformate |
| `src/text_utils.h/.cpp` | Umwandlung UTF-8 → Latin-1 für echte Umlaute |
| `src/wifi_diag.h/.cpp` | Ursache von WLAN-Abbrüchen für den Fehlerbildschirm |
| `src/wifi_conn.h/.cpp` | WLAN verbinden und nach Abbrüchen wiederherstellen |
| `src/extras.h/.cpp` | Andockstellen für eigene Zusatzfunktionen – in dieser Version leer, ohne Funktion (siehe [Technische Hinweise](#technische-hinweise-für-änderungen-am-code)) |
| `src/FreeSans9pt8b.h`, `src/FreeSansBold9pt8b.h` | Display-Schriften mit Umlauten |
| `src/FreeSansBold8pt8b.h` | fette Schrift eine Größe kleiner, nur für die Störungszeile |
| `src/FreeSans8pt8b.h` | reguläre Schrift 8 pt, derzeit ungenutzt (Reserve) |
| `docs/` | Web-Installer (GitHub Pages): Seite, `manifest.json`, Firmware in `docs/firmware/` |
| `werkzeuge/` | Skript, das die exportierte Firmware für den Web-Installer bereitstellt |
| `haltestellen/` | Haltestellenliste des MVV mit globalen IDs (eigene Lizenz, siehe dort) |
| `gehaeuse/` | 3D-Druck-Gehäuse (eigene Lizenz, siehe dort) |

## Technische Hinweise (für Änderungen am Code)

- **MVG-API:** Richtungscodes (`:H:`/`:R:`), Meldungstypen und
  Verkehrsmittel-Werte (`SBAHN`, `UBAHN`, `TRAM`, `BUS`, `REGIONAL_BUS`,
  `BAHN`) sind durch Beobachtung ermittelt, nicht dokumentiert.
  Meldungstypen: `INCIDENT` (Störung) und `EARLY_TERMINATION` (vorzeitiges
  Fahrtende) lösen ein Warndreieck aus, `INFO` (z.B. Tarifhinweise) wird
  bewusst ignoriert. Neue fahrtrelevante Meldungstypen in `mvg_api.cpp`
  ergänzen. Das Dreieck (Störung und Ausfall) zeichnet `drawContent()` in
  `display.cpp` an fester Stelle links vom Trennstrich (`WARNING_ICON_X`),
  nur bei `showIncidents`; das Ziel endet dann bei `WARNING_DEST_END`.
- **Störungszeile:** `parseDepartures()` merkt sich je Abfahrt den Text der
  ersten `INCIDENT`-Meldung (`Departure.incident`, Latin-1, höchstens
  `INCIDENT_TEXT_MAX` Zeichen); zusammengefasste Fahrten und Ersatzzüge
  übernehmen ihn. `incidentRowIndex()` (.ino) wählt die früheste der ersten
  drei Abfahrten mit Text (nur Station 1, `showIncidents` und
  `incidentText`); `drawContent()` zeichnet dann Dreieck, Linie und Text fett
  (`fitEllipsis()`) in Zeile 1 und drei Abfahrten darunter. Seite 2 beginnt
  in dem Fall mit der vierten Abfahrt. Die Zeile hat ein größeres Dreieck
  (`drawIncidentTriangle()`) und nutzt die fette 8-pt-Schrift.
- **Verkehrsmittel-Filter:** `downloadDepartures()` hängt
  `&transportTypes=…` an die URL. Ohne diesen Parameter liefert die API nur
  S-Bahn, U-Bahn, Tram und Stadtbus (beobachtet).
- **API-Abrufe:** Abfahrten werden in `fetchStation()` (.ino, Abschnitt
  Update-Steuerung) abgerufen. Seit 2.6.1 ruft das Display jede Minute zu
  einer zufälligen Sekunde zwischen `FETCH_WINDOW_START` und
  `FETCH_WINDOW_END` (:30–:50, jede Minute neu gewürfelt) ab
  (`prefetchDepartures()`, füllt nur den Zwischenspeicher) und zeichnet zur
  vollen Minute (`UPDATE_TARGET_SECOND`) aus dem Zwischenspeicher – so
  fragen nicht alle Geräte gleichzeitig bei der MVG an, und die Uhrzeit
  springt pünktlich um. Beim Zeichnen blendet `dropDeparted()` inzwischen
  abgefahrene Züge aus. Station 2 (wenn einer Taste zugeordnet) wird direkt
  nach der angezeigten Station abgerufen, aber nur bis Sekunde
  `FETCH_OTHER_LATEST` (48), sonst in der nächsten Minute. Sind die Daten
  beim Zeichnen älter als `FETCH_MAX_AGE_MS` (Abruf verpasst, z.B. weil
  QR/Info offen war), ruft `attemptUpdate()` wie früher direkt davor ab;
  läuft nach einem Fehlschlag schon der leise neue Versuch, nicht.
  Zusätzlich wird beim Start, nach WLAN-Wiederkehr, bei Rückkehr aus
  QR/Info in einer neuen Minute und während einer Störung abgerufen. Tasten
  und Auto-Reset zeichnen nur aus dem Zwischenspeicher (Daten bis 2 Minuten
  alt, sonst neuer Abruf).
- **Tasten:** Belegung `keyLeft`/`keyMid` (Konstanten `BTN_...` in
  `settings.h`). `keyPressed()` im .ino: die Ansicht der Taste zeigen
  (`activeKey`); dieselbe Taste (oder eine ohne Funktion) erneut führt zurück
  zur Standardansicht, die andere Taste zeigt direkt ihre Ansicht; kein
  Weiterschalten durch mehrere Ansichten. Auto-Reset nach 30 s nur bei
  `viewAutoBack` (Portal, Standard `VIEW_AUTO_BACK` in `config.h`).
  `handleKeysAndScreens()` wertet Tasten, QR-/Info-Screen und Auto-Reset
  aus, auch während ein WLAN-Ausfall überbrückt wird (dann nur aus dem
  Zwischenspeicher). Die Ansicht ergibt sich aus `activeKey`
  (`viewDirection()`, `headerDirection()` für „→ Zentrum/Auswärts/H/R“).
- **Minuten-Anzeige:** `Departure.planned` (geplante Abfahrt, Unix-Zeit)
  plus Verspätung, gerechnet als Differenz der Minuten wie die Uhr im Kopf
  (14:07 → 14:09 = „2 min“); ab 60 Minuten Uhrzeit.
- **Störungen überbrücken:** Misslingt ein Abruf, bleibt die Anzeige
  stehen (Uhrzeit wird zur vollen Minute aktualisiert) und es wird alle
  `ERROR_RETRY_INTERVAL_MS` (10 s) erneut versucht, ohne zu blockieren.
  Erst wenn die Fehlschläge `API_ERROR_SCREEN_DELAY_MS` (60 s) andauern,
  erscheinen Fehlerbildschirm und API-Fail. Ebenso beim WLAN: Fehlerbildschirm
  im Betrieb nach `WIFI_LOST_SCREEN_DELAY_MS` (60 s), beim Start nach
  `WIFI_ERROR_SCREEN_DELAY_MS` (20 s). Der WLAN-Stromsparmodus ist aus
  (`WiFi.setSleep(false)` in `setup()`) – im Sparmodus trennten manche
  Router das Board immer wieder.
- **Neustart bei Abruffehlern:** Scheitern die Abrufe bei bestehendem WLAN
  `API_RESTART_AFTER_MS` (10 Min.) am Stück, startet das Gerät neu – nur,
  wenn seit dem Start schon ein Abruf geklappt hat (`everFetched`, sonst
  droht bei einem längeren MVG-Ausfall eine Neustart-Schleife) und das
  Portal nicht offen ist. WLAN-Ausfälle zählen nicht mit. Alle Neustarts
  laufen über `restartCleanly()` (`wifi_conn`), das sich vorher mit
  `WiFi.disconnect()` vom WLAN abmeldet.
- **Antworten als Datenstrom:** Die MVG antwortet ohne Längenangabe und
  nicht in Blöcken. `fetchJsonStream()` liest die Antwort mit
  `HttpBodyStream` (wartet bis `HTTP_TIMEOUT_MS` auf weitere Daten) direkt
  in ArduinoJson ein, mit Feldfilter – so wird nur gespeichert, was
  ausgewertet wird (bis 46 KB Antwort bei 100 Fahrten). Neue ausgewertete
  Felder im jeweiligen Filter ergänzen.
- **Linienfilter:** `parseDepartures()` prüft Linie (`lineKey`) und
  Kennung (`:H:`/`:R:` aus `lineId`) gegen die Auswahl (`LineSelection`,
  Textform `S2:H:S,RE80:B:Z` = Linie : H/R/B : Verkehrsmittel-Kürzel). Die
  Richtung der Station („nur H/R“) gilt zusätzlich. `transportTypes` ergibt
  sich bei einer Auswahl aus den gewählten Linien. Eine Richtung je Linie
  speichert das Portal nur bei „Je Linie wählen“ (Display: wie „alle
  Richtungen“, `dir` `B`), sonst immer `B`.
- **Flügelzüge:** Die API liefert geteilte Züge als getrennte Fahrten.
  `mvg_api.cpp` fasst Einträge mit gleicher Linie, Richtung, geplanter Zeit,
  gleichem Gleis und gleichem Ausfall-Status zusammen (nicht bei Bussen oder
  fehlendem Gleis). Bei vorzeitigem Fahrtende zählt das angezeigte Ziel;
  Einträge mit gleichem Ziel werden zu einer Zeile. Feste Kurzformen
  stehen in der Tabelle `SPLIT_TRAIN_LABELS`. Gekoppelte Regionalzüge mit
  verschiedenen Liniennummern (z.B. BRB RB 55/56/57) fasst die feste
  Tabelle `SPLIT_TRAIN_GROUPS` zusammen (Symbol „RB“/„RE“, Ziele gleichmäßig
  gekürzt); angezeigt wird die kleinste Verspätung der Zugteile mit
  Echtzeit.
- **Einstellungen:** `src/settings` hält alle Werte in `appSettings`
  (NVS-Namensraum `abfahrt`). Seit 2.2.0 dazu: `lines1`, `lines2`
  (Linienauswahl als Text), `station2`, `types2` (Verkehrsmittel als Bits),
  `dir1`, `dir2` (Richtung `B`/`H`/`R`). Fehlen sie (Update von 2.1.0),
  gelten die bisherigen Werte als Station 1. Seit 2.3.0: `zentrum1`,
  `zentrum2` (`H`/`R`/`K` = keine), `start1` (Startansicht `H`/`R`),
  `keyLeft`, `keyMid`, `minutes`, `name1`, `name2` (Stationsnamen, UTF-8;
  fehlen sie, holt `fetchStationNames()` sie einmal bei der MVG und
  speichert sie); seit 2.4.0 `autoBack` (Auto-Reset der Tasten-Ansicht);
  seit 2.5.0 `showInc` (Warndreieck, Standard `SHOW_INCIDENTS`) und
  `fullRef` (Full Refresh alle n Minuten, Standard `FULL_REFRESH_EVERY`,
  begrenzt auf `FULL_REFRESH_MIN`..`FULL_REFRESH_MAX`); seit 2.6.0
  `incText` (Störungstext in Zeile 1, Standard `SHOW_INCIDENT_TEXT`); „umschaltbar“ ist `dir1` `U` im
  Speicher, im NVS als `dirView` + `dir1` `B` (damit 2.2.0 nach einem
  Downgrade passt; auch `zentrumIsH`, `defZentrum`, `qrOn` werden weiter
  geschrieben). Fehlen die neuen Schlüssel (Update von 2.2.0), übernimmt
  `settingsLoad()` die alten Werte so, dass Anzeige und Tasten gleich
  bleiben. Die NVS-Schlüssel nie
  umbenennen, sonst gehen gespeicherte Einstellungen bei Updates verloren. `appSettings` wird
  nur im `loop()`-Kontext geändert; andere Tasks lesen mit `SettingsLock`.
- **Web-Installer:** Der Firmware-Name in der Improv-Antwort
  (`IMPROV_FIRMWARE_NAME` in `improv_serial.cpp`) muss mit `name` in
  `docs/manifest.json` übereinstimmen – nur dann bietet der Installer
  *Update* ohne Löschen an. Die Kennung `FIRMWARE_MARKER` (Firmware-Upload
  im Portal, Prüfung im Export-Skript) nie ändern.
- **Einstellungen/Info** (mittlere Taste 3 s): `displayShowInfo()` in
  `display.cpp` – QR-Code zur Portal-Adresse (`drawQr()` mit fester
  Modulgröße `INFO_QR_SCALE`), Version darunter (Zusatz ab „-“ ggf. in
  zweiter Zeile), IP, Portal-Schließzeit, WLAN-Name, Signal, Laufzeit.
  Intern heißt der Zustand weiter `logModeActive`.
- **Liniensymbole:** neue Bitmaps in `line_icons.h` ergänzen – maximal
  36 px breit (per `static_assert` in `display.cpp` geprüft). Die
  Pixelschrift der generierten Symbole kennt 0–9, „N“, „X“, „R“, „B“, „E“,
  „S“, „V“;
  andere Zeichen erscheinen in der eingebauten 6×8-Schrift.
- **Umlaute:** Die Display-Schriften sind Latin-1-kodiert. Texte aus API
  und Einstellungen (UTF-8) wandelt `utf8ToLatin1()` um. Feste Texte im Code
  als Escape schreiben, z.B. `"Ausw\xE4rts"` – folgt direkt ein Zeichen
  0–9/a–f/A–F, den String trennen (`"Zur\xFC" "ck"`).
- **WLAN-Ursache:** `wifi_diag.cpp` wertet den Reason-Code des ESP32 beim
  Verbindungsabbruch aus. Ein falsches Passwort meldet der ESP32 meist nur
  als Zeitüberschreitung beim Anmelden – das kann auch bei sehr schwachem
  Empfang auftreten, daher „Passwort falsch?“ mit Fragezeichen.
- **WLAN-Verbindung:** `wifi_conn.cpp` sucht auf allen Kanälen und wählt
  den Zugangspunkt mit dem stärksten Signal (Standard des ESP32: der erste
  gefundene – ungünstig bei Mesh/Repeatern). Alle `WiFi.begin()` (auch
  Improv) laufen über `wifiConnBegin()`. `wifiConnLoop()` startet einen
  neuen Versuch erst, wenn der laufende beendet ist (Abbruch gemeldet oder
  10 s vergangen), mit Pausen von 2/5/15/30 s; nach sechs Fehlschlägen in
  Folge wird das WLAN-Modul aus- und wieder eingeschaltet. Das automatische
  Wiederverbinden des ESP32 ist dafür aus – außer
  `extrasHandlesWifiReconnect()` liefert `true`. Mit `DEBUG_LOG 1` im
  seriellen Monitor: jeder Abbruch sofort mit Reason-Code, jede Verbindung
  und einmal pro Minute Zugangspunkt (BSSID), Kanal und Signal.
- **Eigene Zusatzfunktionen (Andockstellen):** `src/extras.h` enthält
  feste Stellen im Programmablauf – beim Start, in der Hauptschleife, bei
  Fehlern, nach einem WLAN-Wechsel –, an denen eine eigene Variante des
  Projekts zusätzlichen Code ausführen kann, z.B. eine Status-LED
  ansteuern oder einen Zusatz an die Versionsnummer hängen. Der gemeinsame
  Code bleibt dabei unverändert, Updates lassen sich ohne Konflikte
  übernehmen. Der Autor nutzt das selbst für eine private Variante mit
  Zusatzfunktionen für den Eigengebrauch.
  - **In dieser Version tun die Andockstellen nichts:** Die
    Standardfassungen in `extras.cpp` sind leer (als `weak` markiert).
    Die veröffentlichte Firmware sendet außer den Abfahrtsabfragen an die
    MVG keine Daten nach außen.
  - **Aktiv wird eine Erweiterung nur**, wenn jemand eigenen Code in `src/`
    ablegt und die Firmware selbst kompiliert – nicht nachträglich und
    nicht von außen.
  - **So geht's:** eine eigene `.cpp` in `src/` anlegen und dieselben
    Funktionen dort neu definieren; der Linker nimmt dann deren Fassung.
- **`#include <HTTPClient.h>` im .ino nicht entfernen:** Ist die Library
  ArduinoHttpClient installiert, bindet die IDE unter Windows sonst deren
  `HttpClient.h` ein (Groß-/Kleinschreibung).
- **Portal-Abfragen:** `/suche?q=` (Stationssuche), `/name?id=`
  (Stationsname), `/richtungen?id=&types=` (Linien und Ziele je Kennung,
  höchstens 30 Einträge – Beispielziele bei der Richtungsanzeige),
  `/linien?id=&types=` (Linienliste, dauert einige Sekunden), `/speichern`
  (die Seite sendet per `fetch` und bleibt offen), `/update`. `types` =
  Verkehrsmittel als Bits wie `types2`; die Stationssuche liefert sie je
  Treffer als `b` (gestrichelte Verkehrsmittel im Portal).
  Die Seite ist ein Raw-String in `portal.cpp` (Umlaute im HTML als
  Entities, im JavaScript als UTF-8).
- **Serielle Ausgabe:** `Serial.setTxTimeoutMs(0)` direkt nach
  `Serial.begin()` nicht entfernen – sonst wartet jede Ausgabe bis zu 2 s,
  wenn das Board am PC hängt und kein Programm mitliest.
