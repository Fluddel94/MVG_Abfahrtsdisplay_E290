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
  gespeicherte Werte haben immer Vorrang. Linienauswahl und Richtung je
  Station gibt es nur im Portal.
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

Nötig für die getrennte Anzeige und für „nur Richtung H/R“. Am einfachsten
mit *Richtungen anzeigen* oder *Linien auswählen* im Portal. Von Hand: in der Abfahrtsliste aus Schritt 4 bei
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
| `src/buttons.h/.cpp` | Tastenauswertung (Entprellen, Kurz-/Lang-Druck) |
| `src/stats.h/.cpp` | API-Störungsstatistik, WLAN-Signalbewertung |
| `src/time_utils.h/.cpp` | Laufzeit, Zeitformate |
| `src/text_utils.h/.cpp` | Umwandlung UTF-8 → Latin-1 für echte Umlaute |
| `src/wifi_diag.h/.cpp` | Ursache von WLAN-Abbrüchen für den Fehlerbildschirm |
| `src/extras.h/.cpp` | Andockstellen für eigene Zusatzfunktionen – in dieser Version leer, ohne Funktion (siehe [Technische Hinweise](#technische-hinweise-für-änderungen-am-code)) |
| `src/FreeSans9pt8b.h`, `src/FreeSansBold9pt8b.h` | Display-Schriften mit Umlauten |
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
  ergänzen.
- **Verkehrsmittel-Filter:** `downloadDepartures()` hängt
  `&transportTypes=…` an die URL. Ohne diesen Parameter liefert die API nur
  S-Bahn, U-Bahn, Tram und Stadtbus (beobachtet).
- **API-Abrufe:** Abfahrten werden in `fetchStation()` (.ino, Abschnitt
  Update-Steuerung) abgerufen – beim Minuten-Update, beim Start, nach
  WLAN-Wiederkehr, bei Rückkehr aus QR/Log in einer neuen Minute und
  während einer Störung. Mit zwei Stationen ruft `attemptUpdate()` erst
  die angezeigte Station ab und zeichnet, danach die andere
  (`fetchOtherStation()`); Umschalten, Blättern und Auto-Reset zeichnen nur
  aus dem Zwischenspeicher (Daten bis 2 Minuten alt, sonst neuer Abruf).
- **Störungen überbrücken:** Misslingt ein Abruf, bleibt die Anzeige
  stehen (Uhrzeit wird zur vollen Minute aktualisiert) und es wird alle
  `ERROR_RETRY_INTERVAL_MS` (10 s) erneut versucht, ohne zu blockieren.
  Erst wenn die Fehlschläge `API_ERROR_SCREEN_DELAY_MS` (60 s) andauern,
  erscheinen Fehlerbildschirm und API-Fail. Ebenso beim WLAN: Fehlerbildschirm
  im Betrieb nach `WIFI_LOST_SCREEN_DELAY_MS` (60 s), beim Start nach
  `WIFI_ERROR_SCREEN_DELAY_MS` (20 s).
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
  sich bei einer Auswahl aus den gewählten Linien.
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
  gelten die bisherigen Werte als Station 1. Die NVS-Schlüssel nie
  umbenennen, sonst gehen gespeicherte Einstellungen bei Updates verloren. `appSettings` wird
  nur im `loop()`-Kontext geändert; andere Tasks lesen mit `SettingsLock`.
- **Web-Installer:** Der Firmware-Name in der Improv-Antwort
  (`IMPROV_FIRMWARE_NAME` in `improv_serial.cpp`) muss mit `name` in
  `docs/manifest.json` übereinstimmen – nur dann bietet der Installer
  *Update* ohne Löschen an. Die Kennung `FIRMWARE_MARKER` (Firmware-Upload
  im Portal, Prüfung im Export-Skript) nie ändern.
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
  (Stationsname), `/richtungen?id=&types=` (Linien je Kennung, höchstens 15
  Einträge), `/linien?id=&types=` (Linienliste, dauert einige Sekunden),
  `/speichern`, `/update`. `types` = Verkehrsmittel als Bits wie `types2`.
  Die Seite ist ein Raw-String in `portal.cpp` (Umlaute im HTML als
  Entities, im JavaScript als UTF-8).
- **Serielle Ausgabe:** `Serial.setTxTimeoutMs(0)` direkt nach
  `Serial.begin()` nicht entfernen – sonst wartet jede Ausgabe bis zu 2 s,
  wenn das Board am PC hängt und kein Programm mitliest.
