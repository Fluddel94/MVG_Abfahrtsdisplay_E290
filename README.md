# E-Ink-Abfahrtsdisplay für MVG-Haltestellen (Heltec Vision Master E290)

Zeigt die nächsten Live-Abfahrten einer Münchner Haltestelle (MVG/MVV) auf
einem 2,9"-E-Ink-Display an – mit Liniensymbolen, Verspätungen, Ausfällen
und Störungshinweisen. Das Display läuft im Dauerbetrieb am USB-Netzteil und
aktualisiert sich einmal pro Minute.

> **Hinweis:** Das Projekt nutzt die **inoffizielle, undokumentierte
> MVG-API**. Es steht in keiner Verbindung zur MVG, zum MVV oder zur
> Deutschen Bahn. Laut Impressum der MVG wird eine *gemäßigte Nutzung für
> private, nicht-kommerzielle Zwecke* ohne ausdrückliche Zustimmung
> geduldet; Data-Mining ist ausgeschlossen. Die API kann sich jederzeit
> ändern oder abgeschaltet werden. Gedacht ist das Projekt nur für das
> MVG-/MVV-Gebiet.

Aktuelle Version: siehe `FW_VERSION` in `MVG_Abfahrtsdisplay_E290.ino`,
Änderungen siehe [CHANGELOG.md](CHANGELOG.md).

---

## Funktionen

- **Abfahrtsliste:** die nächsten 4 Abfahrten mit Linie, Ziel, geplanter
  Zeit und Verspätung (`+3`, max. ±99). Verfrühte Abfahrten erst ab
  3 Minuten (`-3`), pünktliche Abfahrten zeigen nur die Uhrzeit.
  Aktualisierung einmal pro Minute, alle 10 Updates ein vollständiger
  Bildaufbau gegen Grauschleier.
- **Zwei Anzeigearten:** alle Richtungen gemischt mit einer zweiten Seite
  (Abfahrt 5–8) – oder getrennt nach „Zentrum“ und „Auswärts“ mit
  Umschalttaste.
- **Verkehrsmittel-Filter:** S-Bahn, U-Bahn, Tram, Bus (inkl. Regionalbus)
  und Regionalzug einzeln ein-/ausschaltbar.
- **Liniensymbole:** S-Bahn (S1–S8, S20), U-Bahn (U1–U8), Tram und
  Nacht-Tram als Bitmap; Busse und Regionalzüge („RB56“) werden automatisch
  als Rahmen mit Liniennummer erzeugt.
- **Störungen:** Bei Zugausfällen wird die Uhrzeit durchgestrichen und ein
  Warndreieck angezeigt. Fahrtrelevante Meldungen bekommen ein Warndreieck
  hinter dem Ziel, bei vorzeitigem Fahrtende steht das tatsächliche Ziel da.
- **Flügelzüge:** Züge, die unterwegs geteilt werden (z.B. S1 Flughafen /
  Freising), erscheinen als eine Zeile.
- **Echte Umlaute** auf dem Display.
- **System-Log** (Taste 3 s halten): Firmware-Version, Startzeitpunkt,
  WLAN-Laufzeit und -Signal, WLAN-Abbrüche, API-Störungen der letzten 24 h.
- **Fehleranzeige:** eigene Bildschirme bei WLAN- oder API-Ausfall mit
  automatischem Neuversuch. Klappt die WLAN-Verbindung länger als 20 s
  nicht, zeigt das Display die vermutete Ursache (z.B. „Passwort falsch?“
  oder „Netz nicht gefunden“).
- **Optional:** WLAN-QR-Code (z.B. für ein Gast-WLAN) und Firmware-Update
  per WLAN (OTA), siehe [Zusatzfunktionen](#zusatzfunktionen).

## Bedienung

| Taste | Aktion |
|---|---|
| BOOT (GPIO 0) | gemischte Anzeige: Seite 2 (Abfahrt 5–8); getrennte Anzeige: Richtung umschalten. Nach 30 s geht es automatisch zurück. |
| Taste GPIO 21, 3 s halten | System-Log für 60 s, erneut 3 s halten = zurück |
| Taste GPIO 21, kurz | WLAN-QR-Code für 60 s (nur wenn eingeschaltet), erneuter Druck = zurück |

## Was du brauchst

- **Heltec Vision Master E290** (ESP32-S3 mit integriertem 2,9"-E-Ink-Display)
- USB-C-Kabel und ein USB-Netzteil für den Dauerbetrieb
- WLAN mit 2,4 GHz
- [Arduino IDE 2](https://www.arduino.cc/en/software)
- optional: 3D-Drucker für das [Gehäuse](#gehäuse)

## Einrichtung

### 1. Arduino-IDE vorbereiten

1. **Boardpaket:** *Werkzeuge → Board → Boardverwalter*, nach „esp32“
   suchen und **esp32 von Espressif Systems** installieren (entwickelt mit
   Version 3.3.11).
2. **Libraries:** *Werkzeuge → Bibliotheken verwalten*, installieren:

   | Library | Hinweis |
   |---|---|
   | heltec-eink-modules | Display-Treiber inkl. Grafikfunktionen und Schriften (getestet mit 4.6.0) |
   | ArduinoJson | Version 7 |

   Mehr ist nicht nötig: WLAN, HTTPS, OTA und der QR-Generator stecken im
   ESP32-Boardpaket, die Grafikfunktionen (abgeleitet von Adafruit GFX) in
   heltec-eink-modules.
3. **Board auswählen:** *Werkzeuge → Board → esp32 →* **Heltec Vision Master E290**.

### 2. Projekt herunterladen

Auf GitHub *Code → Download ZIP*, entpacken. Der Ordner muss
`MVG_Abfahrtsdisplay_E290` heißen – genau wie die `.ino`-Datei darin,
sonst öffnet die Arduino-IDE den Sketch nicht (beim ZIP-Download heißt er
`MVG_Abfahrtsdisplay_E290-main` und muss umbenannt werden). Wer mit Git
arbeitet (`git clone`), bekommt den Ordner direkt mit dem richtigen Namen.

### 3. WLAN-Zugangsdaten eintragen

`secrets_example.h` kopieren, die Kopie in **`secrets.h`** umbenennen und
WLAN-Name und -Passwort eintragen. Die übrigen Zeilen (Zusatzfunktionen)
dürfen Platzhalter behalten. `secrets.h` niemals weitergeben.

### 4. `config.h` anpassen

| Schritt | Einstellung | Werte |
|---|---|---|
| 1 Station | `STATION_GLOBAL_ID` | globalId deiner Haltestelle (siehe [Station finden](#station-finden)), Standard: Marienplatz |
| 2 Anzeige | `FEATURE_DIRECTION_VIEW` | 0 = alle Richtungen gemischt mit Seite 2 (Standard), 1 = getrennt nach Zentrum/Auswärts |
| 2a nur bei 1 | `ZENTRUM_IS_H` | 1, wenn `:H:` Richtung Zentrum fährt, sonst 0 (siehe [Richtungen prüfen](#richtungen-prüfen)) |
| 2b nur bei 1 | `DEFAULT_VIEW_ZENTRUM` | Standardansicht: 1 = Zentrum, 0 = Auswärts |
| 3 Verkehrsmittel | `SHOW_SBAHN`, `SHOW_UBAHN`, `SHOW_TRAM`, `SHOW_BUS`, `SHOW_BAHN` | je 1 = anzeigen, 0 = ausblenden; mindestens eines muss 1 sein |

### 5. Hochladen

Board per USB anschließen, Port wählen und hochladen. Nach dem Start
verbindet sich das Board mit dem WLAN, holt die Uhrzeit und zeigt die
Abfahrten. Klappt die WLAN-Verbindung nicht, erscheint nach 20 s ein
Fehlerbildschirm mit der vermuteten Ursache.

## Station finden

Die `globalId` hat das Format `de:09162:2`. So findest du sie:

1. Die Haltestellenliste des MVV öffnen:
   [`haltestellen/MVV_Haltestellen_Report_s26.csv`](haltestellen/MVV_Haltestellen_Report_s26.csv)
   – direkt auf GitHub oder heruntergeladen in Excel.
2. Mit Strg+F nach dem Stationsnamen suchen. Bei gleichnamigen Haltestellen
   auf die Spalte *Ort* achten.
3. Den Wert aus der Spalte *Globale ID* als `STATION_GLOBAL_ID` in
   `config.h` eintragen.
4. Optional prüfen: `https://www.mvg.de/api/bgw-pt/v3/departures?globalId=<ID>`
   im Browser aufrufen – kommt eine Liste mit Abfahrten zurück, stimmt die ID.

Mehr zur Liste (Spalten, Quelle, Lizenz): [`haltestellen/README.md`](haltestellen/README.md).

### Richtungen prüfen

Nur nötig bei `FEATURE_DIRECTION_VIEW 1`. In der Abfahrtsliste aus Schritt 4
bei einigen Einträgen `lineId` (enthält `:H:` oder `:R:`) mit `destination`
vergleichen. Fährt `:H:` Richtung Zentrum, gilt `ZENTRUM_IS_H 1`, sonst 0.
Eine feste Regel gibt es nicht – das hängt von Linie und Station ab.

## Zusatzfunktionen

Beide sind standardmäßig **aus** und werden in `config.h` im Abschnitt
„ZUSATZFUNKTIONEN“ eingeschaltet.

### WLAN-QR-Code (`FEATURE_WIFI_QR`)

Ein kurzer Druck auf die Taste GPIO 21 zeigt 60 s lang einen QR-Code, mit
dem sich Handys direkt mit einem WLAN verbinden können (z.B. Gast-WLAN),
plus SSID und Passwort im Klartext. Zugangsdaten in `secrets.h`
(`qrWlanSsid`, `qrWlanPassword`), Überschrift in `config.h`
(`QR_SCREEN_TITLE`). Das Passwort ist für jeden sichtbar, der vor dem
Display steht – also nur für ein WLAN verwenden, das du teilen möchtest.

Die Arduino-Library „QRCode“ von Richard Moore darf **nicht** installiert
sein: Ihre `qrcode.h` heißt genauso wie die des Boardpakets und würde
stattdessen eingebunden.

### Firmware-Update per WLAN (`FEATURE_OTA`)

Nach dem ersten Hochladen per USB erscheint das Board in der Arduino-IDE als
Netzwerk-Port (Name: `OTA_HOSTNAME`), Updates gehen dann ohne Kabel.

- Passwort in `secrets.h` (`otaPassword`) – stark und einzigartig wählen.
- Das Partitionsschema muss OTA unterstützen (zwei App-Partitionen, z.B.
  der Standard). Ändern lässt es sich nur per USB.
- PC und Board müssen im selben Netz sein. Für das Board keine
  Portfreigabe/UPnP im Router einrichten.
- Wird eine Firmware mit `FEATURE_OTA 0` per OTA aufgespielt, geht das
  nächste Update nur noch per USB.

## Einschränkungen

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

## Dateien

Im Hauptordner liegt nur, was pro Gerät angepasst wird; der Programmcode
liegt in `src/`. Der Unterordner **muss** `src` heißen – die Arduino-IDE
kompiliert außer dem Hauptordner nur diesen Ordner.

| Datei | Inhalt |
|---|---|
| `MVG_Abfahrtsdisplay_E290.ino` | Ablaufsteuerung: `setup()`, `loop()`, Tasten-Aktionen, WLAN-Fehleranzeige |
| `config.h` | **Geräte-Einstellungen** und gemeinsame Konstanten |
| `secrets_example.h` | Vorlage für `secrets.h` (Zugangsdaten) |
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
- **API-Abrufe:** Abfahrten werden nur in `attemptUpdate()` (.ino, Block 07)
  abgerufen – beim Minuten-Update, beim Start, nach WLAN-Wiederkehr, bei
  Rückkehr aus QR/Log in einer neuen Minute und alle 10 s während einer
  Störung. Umschalten und Blättern zeichnen aus einem Zwischenspeicher.
- **Flügelzüge:** Die API liefert geteilte Züge als getrennte Fahrten.
  `mvg_api.cpp` fasst Einträge mit gleicher Linie, Richtung, geplanter Zeit
  und gleichem Gleis zusammen (nicht bei Bussen, fehlendem Gleis,
  abweichendem Ausfall-Status oder vorzeitigem Fahrtende). Feste Kurzformen
  stehen in der Tabelle `SPLIT_TRAIN_LABELS`.
- **Liniensymbole:** neue Bitmaps in `line_icons.h` ergänzen – maximal
  36 px breit (per `static_assert` in `display.cpp` geprüft). Die
  Pixelschrift der generierten Symbole kennt 0–9, „N“, „X“, „R“, „B“, „E“;
  andere Zeichen erscheinen in der eingebauten 6×8-Schrift.
- **Umlaute:** Die Display-Schriften sind Latin-1-kodiert. Texte aus API
  und `secrets.h` (UTF-8) wandelt `utf8ToLatin1()` um. Feste Texte im Code
  als Escape schreiben, z.B. `"Ausw\xE4rts"` – folgt direkt ein Zeichen
  0–9/a–f/A–F, den String trennen (`"Zur\xFC" "ck"`).
- **WLAN-Ursache:** `wifi_diag.cpp` wertet den Reason-Code des ESP32 beim
  Verbindungsabbruch aus. Ein falsches Passwort meldet der ESP32 meist nur
  als Zeitüberschreitung beim Anmelden – das kann auch bei sehr schwachem
  Empfang auftreten, daher „Passwort falsch?“ mit Fragezeichen.
- **Eigene Erweiterungen:** `src/extras.h` bietet Andockstellen in
  `setup()` und `loop()` (z.B. für eine Status-LED oder eine
  Fernüberwachung). Die Standardfassungen in `extras.cpp` sind leer und als
  `weak` markiert. Für eine Erweiterung eine eigene `.cpp` in `src/`
  anlegen und dieselben Funktionen dort neu definieren – der übrige Code
  bleibt unverändert, Updates lassen sich so ohne Konflikte übernehmen.
- **`#include <HTTPClient.h>` im .ino nicht entfernen:** Ist die Library
  ArduinoHttpClient installiert, bindet die IDE unter Windows sonst deren
  `HttpClient.h` ein (Groß-/Kleinschreibung).

## Gehäuse

Im Ordner [`gehaeuse/`](gehaeuse/) liegt ein Gehäuse zum 3D-Drucken. Es ist
ein Remix von **„Vision Master E290 V0.3.1 case for Meshtastic“** von
**HarukiToreda** ([Printables 974647](https://www.printables.com/model/974647)),
lizenziert unter [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Änderungen: Antennenanschluss entfernt, Schrift- und Logovertiefungen
entfernt, das Gehäuse steht geneigt auf dem Tisch. Der Remix steht ebenfalls unter CC BY 4.0.

## Lizenz

- **Code:** [GNU General Public License v3.0 oder später](LICENSE)
  (GPL-3.0-or-later).
- **Schriften** `src/FreeSans9pt8b.h`, `src/FreeSansBold9pt8b.h`: abgeleitet
  aus den Adafruit-GFX-Schriften bzw. aus
  [GNU FreeFont](https://www.gnu.org/software/freefont/) (GPL-3.0-or-later
  mit Font-Ausnahme).
- **Liniensymbole** S-Bahn/U-Bahn: konvertiert aus SVG-Dateien von
  [Wikimedia Commons](https://commons.wikimedia.org/), dort als gemeinfrei
  gekennzeichnet. Tram-, Nacht-Tram-, Bus- und Zug-Symbole sind eigene
  Pixelgrafiken. Die Liniensymbole können Marken der jeweiligen Inhaber
  (MVV, MVG, DB) sein.
- **Haltestellenliste** `haltestellen/`: Münchner Verkehrs- und
  Tarifverbund GmbH (MVV), CC BY 4.0, Details in
  [`haltestellen/README.md`](haltestellen/README.md).
- **Gehäuse:** CC BY 4.0, siehe [Gehäuse](#gehäuse).
- Verwendete Libraries (nicht in diesem Repository enthalten) stehen unter
  ihren eigenen Lizenzen.

Keine Gewähr für Richtigkeit und Vollständigkeit der angezeigten Daten.
