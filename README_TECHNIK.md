# Technik & Selbst kompilieren – MVG-Abfahrtsdisplay (Heltec Vision Master E290)

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
  Anzeige, Verkehrsmittel, WLAN-QR). Sie gelten nur, solange im Gerät noch
  nichts gespeichert ist – im Portal gespeicherte Werte haben immer
  Vorrang.
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
Portal im Feld *Station-ID* oder als Standardwert `STATION_GLOBAL_ID` in
`config.h`:

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

Nur nötig für die getrennte Anzeige. Am einfachsten mit *Richtungen
anzeigen* im Portal. Von Hand: in der Abfahrtsliste aus Schritt 4 bei
einigen Einträgen `lineId` (enthält `:H:` oder `:R:`) mit `destination`
vergleichen. Eine feste Regel gibt es nicht – das hängt von Linie und
Station ab.

## Einschränkungen im Detail

- Gefiltert wird nach Verkehrsmittel und Richtung, nicht nach einzelnen
  Linien. An großen Stationen erscheinen alle Linien der eingeschalteten
  Verkehrsmittel gemischt.
- Ob die Richtungsmarker `:H:`/`:R:` bei Bus, Tram und U-Bahn überall so
  zuverlässig sind wie bei der S-Bahn, ist nicht getestet. Tangentiallinien
  fahren weder Richtung Zentrum noch stadtauswärts – dort ist die gemischte
  Anzeige oft die bessere Wahl.
- Pünktliche Abfahrten meldet die API ohne Echtzeit-Kennzeichen. „Live und
  pünktlich“ ist daher nicht von „keine Live-Daten“ zu unterscheiden.
- Bei der gemischten Anzeige reichen die abgerufenen 20 Abfahrten an
  ruhigen Stationen evtl. nicht für eine volle Seite 2.
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
| `src/portal.h/.cpp` | Einstellungsportal im Heimnetz (Webseite, Stationssuche, Richtungen, Firmware-Upload) |
| `src/mvg_api.h/.cpp` | Abruf und Auswertung der MVG-API |
| `src/display.h/.cpp` | Alles, was gezeichnet wird |
| `src/line_icons.h` | Liniensymbole und Generator für Bus-/Zug-Symbole |
| `src/buttons.h/.cpp` | Tastenauswertung (Entprellen, Kurz-/Lang-Druck) |
| `src/stats.h/.cpp` | API-Störungsstatistik, WLAN-Signalbewertung |
| `src/time_utils.h/.cpp` | Laufzeit, Zeitformate |
| `src/text_utils.h/.cpp` | Umwandlung UTF-8 → Latin-1 für echte Umlaute |
| `src/wifi_diag.h/.cpp` | Ursache von WLAN-Abbrüchen für den Fehlerbildschirm |
| `src/extras.h/.cpp` | Andockstellen für eigene Erweiterungen (standardmäßig ohne Funktion) |
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
- **API-Abrufe:** Abfahrten werden nur in `attemptUpdate()` (.ino, Abschnitt
  Update-Steuerung) abgerufen – beim Minuten-Update, beim Start, nach
  WLAN-Wiederkehr, bei Rückkehr aus QR/Log in einer neuen Minute und alle
  10 s während einer Störung. Umschalten und Blättern zeichnen aus einem
  Zwischenspeicher.
- **Flügelzüge:** Die API liefert geteilte Züge als getrennte Fahrten.
  `mvg_api.cpp` fasst Einträge mit gleicher Linie, Richtung, geplanter Zeit,
  gleichem Gleis und gleichem Ausfall-Status zusammen (nicht bei Bussen oder
  fehlendem Gleis). Bei vorzeitigem Fahrtende zählt das angezeigte Ziel;
  Einträge mit gleichem Ziel werden zu einer Zeile. Feste Kurzformen
  stehen in der Tabelle `SPLIT_TRAIN_LABELS`.
- **Einstellungen:** `src/settings` hält alle Werte in `appSettings`
  (NVS-Namensraum `abfahrt`). Die NVS-Schlüssel nie umbenennen, sonst
  gehen gespeicherte Einstellungen bei Updates verloren. `appSettings` wird
  nur im `loop()`-Kontext geändert; andere Tasks lesen mit `SettingsLock`.
- **Web-Installer:** Der Firmware-Name in der Improv-Antwort
  (`IMPROV_FIRMWARE_NAME` in `improv_serial.cpp`) muss mit `name` in
  `docs/manifest.json` übereinstimmen – nur dann bietet der Installer
  *Update* ohne Löschen an. Die Kennung `FIRMWARE_MARKER` (Firmware-Upload
  im Portal, Prüfung im Export-Skript) nie ändern.
- **Liniensymbole:** neue Bitmaps in `line_icons.h` ergänzen – maximal
  36 px breit (per `static_assert` in `display.cpp` geprüft). Die
  Pixelschrift der generierten Symbole kennt 0–9, „N“, „X“, „R“, „B“, „E“;
  andere Zeichen erscheinen in der eingebauten 6×8-Schrift.
- **Umlaute:** Die Display-Schriften sind Latin-1-kodiert. Texte aus API
  und Einstellungen (UTF-8) wandelt `utf8ToLatin1()` um. Feste Texte im Code
  als Escape schreiben, z.B. `"Ausw\xE4rts"` – folgt direkt ein Zeichen
  0–9/a–f/A–F, den String trennen (`"Zur\xFC" "ck"`).
- **WLAN-Ursache:** `wifi_diag.cpp` wertet den Reason-Code des ESP32 beim
  Verbindungsabbruch aus. Ein falsches Passwort meldet der ESP32 meist nur
  als Zeitüberschreitung beim Anmelden – das kann auch bei sehr schwachem
  Empfang auftreten, daher „Passwort falsch?“ mit Fragezeichen.
- **Eigene Erweiterungen:** `src/extras.h` bietet Andockstellen in
  `setup()` und `loop()` (z.B. für eine Status-LED). Die Standardfassungen
  in `extras.cpp` sind leer und als `weak` markiert – die veröffentlichte
  Firmware sendet außer den Abfahrtsabfragen an die MVG keine Daten nach
  außen. Für eine Erweiterung eine eigene `.cpp` in `src/`
  anlegen und dieselben Funktionen dort neu definieren – der übrige Code
  bleibt unverändert, Updates lassen sich so ohne Konflikte übernehmen.
- **`#include <HTTPClient.h>` im .ino nicht entfernen:** Ist die Library
  ArduinoHttpClient installiert, bindet die IDE unter Windows sonst deren
  `HttpClient.h` ein (Groß-/Kleinschreibung).
- **Serielle Ausgabe:** `Serial.setTxTimeoutMs(0)` direkt nach
  `Serial.begin()` nicht entfernen – sonst wartet jede Ausgabe bis zu 2 s,
  wenn das Board am PC hängt und kein Programm mitliest.
