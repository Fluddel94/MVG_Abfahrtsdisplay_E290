# E-Ink-Abfahrtsdisplay für MVG-Haltestellen (Heltec Vision Master E290)

Zeigt die nächsten Live-Abfahrten einer Münchner Haltestelle (MVG/MVV) auf
einem 2,9"-E-Ink-Display an – mit Liniensymbolen, Verspätungen, Ausfällen
und Störungshinweisen. Das Display läuft im Dauerbetrieb am USB-Netzteil und
aktualisiert sich einmal pro Minute.

Eingerichtet wird es komplett im Browser: Firmware über den
**[Web-Installer](https://fluddel94.github.io/MVG_Abfahrtsdisplay_E290/)**
aufspielen, WLAN eingeben, Station im Einstellungsportal auswählen – ohne
Arduino IDE.

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
  Endet die Fahrt schon vor dieser Haltestelle, erscheint sie als Ausfall.
- **Flügelzüge:** Züge, die unterwegs geteilt werden (z.B. S1 Flughafen /
  Freising), erscheinen als eine Zeile – auch bei einem Ausfall beider
  Zugteile. Fällt nur ein Zugteil aus, bleibt er als eigene Zeile sichtbar.
  Doppelt gelieferte gleiche Fahrten erscheinen nur einmal.
- **Echte Umlaute** auf dem Display.
- **Einrichtung im Browser:** Web-Installer für Firmware und WLAN,
  [Einstellungsportal](#einstellungsportal) im Heimnetz mit Stationssuche
  per Name, Richtungsanzeige und Firmware-Update. Alle Einstellungen bleiben
  bei Updates erhalten.
- **Startbildschirm:** Projektname und Firmware-Version für etwa 5 Sekunden
  nach dem Einschalten, währenddessen startet alles im Hintergrund.
- **System-Log** (Taste 3 s halten): Firmware-Version, Adresse des
  Einstellungsportals, Startzeitpunkt, WLAN-Signal, WLAN-Abbrüche und
  API-Störungen der letzten 24 h.
- **Fehleranzeige:** eigene Bildschirme bei WLAN- oder API-Ausfall mit
  automatischem Neuversuch. Klappt die WLAN-Verbindung länger als 20 s
  nicht, zeigt das Display die vermutete Ursache (z.B. „Passwort falsch?“
  oder „Netz nicht gefunden“).
- **Optional:** WLAN-QR-Code (z.B. für ein Gäste-WLAN), einschaltbar im
  Einstellungsportal.

## Was du brauchst

- **Heltec Vision Master E290** (ESP32-S3 mit integriertem 2,9"-E-Ink-Display)
- USB-C-Kabel, das **Daten** überträgt (reine Ladekabel funktionieren
  nicht), und ein USB-Netzteil für den Dauerbetrieb
- PC oder Mac mit **Chrome**, **Edge** oder **Firefox ab Version 151**
  (Safari und Handys können nicht auf USB-Geräte zugreifen)
- WLAN mit 2,4 GHz
- optional: 3D-Drucker für das [Gehäuse](#gehäuse)

## Einrichtung

Alles läuft über den **[Web-Installer](https://fluddel94.github.io/MVG_Abfahrtsdisplay_E290/)**.
Sein Dialog ist englisch, die Knöpfe stehen deshalb unten im Original.

1. Web-Installer öffnen, das Display per USB anstecken und
   **Installieren** klicken. Firefox fragt vorher, ob die Seite auf
   serielle Geräte zugreifen darf – erlauben.
2. Im Browser-Fenster das Gerät wählen (meist *USB JTAG/serial debug
   unit*) und *Connect* klicken.
3. *Install MVG Abfahrtsdisplay* wählen, **Erase device anhaken** und
   bestätigen. Beim ersten Mal ist das nötig: Heltec liefert das Board mit
   einer eigenen Firmware aus, die dabei vollständig gelöscht wird. Das
   Aufspielen dauert etwa eine Minute.
4. *Connect to Wi-Fi*: WLAN wählen, Passwort eingeben. Fehlt dein WLAN in
   der Liste, den Dialog schließen und *Connect to Wi-Fi* erneut öffnen –
   oder unten *Join other…* wählen und den Namen von Hand eingeben.
5. *Visit device* öffnet das [Einstellungsportal](#einstellungsportal).
   Solange noch keine Station gespeichert ist, zeigt das Display außerdem
   einen QR-Code und die Adresse des Portals – so kommst du auch vom Handy
   hin.
6. Im Portal die Station suchen, auswählen, Anzeige und Verkehrsmittel
   einstellen und **Speichern**. Das Display zeigt sofort die Abfahrten.
7. USB-Kabel abziehen, Display ins Gehäuse, ans Netzteil – fertig.

Klappt etwas nicht, siehe [Hilfe bei Problemen](#hilfe-bei-problemen).

## Bedienung

| Taste | Aktion |
|---|---|
| BOOT (GPIO 0) | gemischte Anzeige: Seite 2 (Abfahrt 5–8); getrennte Anzeige: Richtung umschalten. Nach 30 s geht es automatisch zurück. |
| Taste GPIO 21, 3 s halten | System-Log für 60 s **und** Einstellungsportal für 30 Minuten öffnen; erneut 3 s halten = zurück |
| Taste GPIO 21, kurz | WLAN-QR-Code für 60 s (nur wenn eingeschaltet), erneuter Druck = zurück |

## Einstellungsportal

Eine Webseite auf dem Display selbst, erreichbar im Heimnetz von jedem
Browser (auch vom Handy). Änderungen wirken nach dem Speichern sofort, ohne
Neustart.

### Öffnen

- **Taste GPIO 21 drei Sekunden halten:** Das System-Log zeigt die Adresse
  (z.B. `http://192.168.178.42`) und bis wann das Portal erreichbar ist.
  Das Portal bleibt 30 Minuten offen, erneutes Öffnen startet die Zeit neu.
- **Nach *Connect to Wi-Fi* im Web-Installer:** *Visit device* führt direkt
  hin, das Portal ist ebenfalls 30 Minuten offen.
- **Ohne gespeicherte Station** (erste Einrichtung, nach Werkseinstellungen)
  ist das Portal dauerhaft offen, das Display zeigt QR-Code und Adresse.

Nach einem normalen Neustart ist das Portal geschlossen. Die Adresse vergibt
dein Router; sie kann sich ändern und steht immer im System-Log.

### Einstellungen

- **Station:** Namen eingeben und *Suchen*. Die Treffer zeigen Ort,
  Tarifzone, Verkehrsmittel und ID, über den Kartenlink lassen sich
  gleichnamige Haltestellen unterscheiden. *Auswählen* übernimmt die
  Station. Alternativ die ID von Hand eintragen, siehe
  [Station finden](#station-finden).
- **Anzeige:** alle Richtungen gemischt oder getrennt nach Zentrum und
  Auswärts. Für die getrennte Anzeige festlegen, welche Richtungskennung
  (H oder R) Richtung Zentrum fährt – *Richtungen anzeigen* listet dazu die
  aktuellen Linien und Ziele je Kennung. Dazu die Standardansicht.
- **Verkehrsmittel:** S-Bahn, U-Bahn, Tram, Bus (Stadt- und Regionalbus),
  Regionalzug.
- **WLAN-QR-Code:** einschalten, Überschrift, WLAN-Name und Passwort des
  WLANs, das der QR-Code teilen soll (z.B. Gäste-WLAN). Das Passwort steht
  im Klartext auf dem Display – also nur ein WLAN verwenden, das du teilen
  möchtest.
- **Gerät:** zeigt das verbundene WLAN mit Signalstärke, Firmware-Version
  und Adresse. Das WLAN selbst wird im Web-Installer geändert
  (*Change Wi-Fi*, siehe [WLAN ändern](#wlan-ändern)).
- **Firmware aktualisieren:** Firmware-Datei hochladen, siehe
  [Update](#update).
- **Werkseinstellungen:** löscht alle Einstellungen inklusive WLAN und
  startet das Display neu. Danach das WLAN wieder über den Web-Installer
  eintragen (*Connect to Wi-Fi*).

Gespeicherte Passwörter zeigt das Portal nie an; ein leeres Passwortfeld
bedeutet „unverändert“.

> Das Portal hat kein Passwort und läuft über `http` (für Geräte im
> Heimnetz gibt es keine anerkannten Zertifikate). Während es offen ist,
> kann jeder in deinem WLAN die Einstellungen ändern. Deshalb öffnet es nur
> auf Abruf und schließt sich nach 30 Minuten.

## Update

Einstellungen, Station und WLAN bleiben bei beiden Wegen erhalten.

- **Per USB:** Web-Installer öffnen, **Installieren**, Gerät wählen,
  *Update MVG Abfahrtsdisplay*. Bietet der Installer nur *Install* an und
  fragt nach *Erase device*, das Häkchen **weglassen** – sonst sind WLAN
  und Station weg.
- **Ohne Kabel im Portal:** Die Datei `firmware.bin` aus dem
  [aktuellen Release](https://github.com/Fluddel94/MVG_Abfahrtsdisplay_E290/releases/latest)
  herunterladen, im Portal unter *Firmware aktualisieren* auswählen und
  *Hochladen*. Das Display zeigt „Firmware-Update läuft“ und startet danach
  neu. Das Portal prüft vorher, ob die Datei zu diesem Display passt; bei
  Fehler oder Abbruch läuft die bisherige Firmware weiter.

## WLAN ändern

Display per USB anstecken, im Web-Installer **Installieren**, Gerät wählen,
*Change Wi-Fi*. Die neuen Daten werden erst übernommen, wenn die
Verbindung klappt – sonst bleibt das bisherige WLAN. Das funktioniert auch,
wenn das Display gerade den WLAN-Fehlerbildschirm zeigt.

## Hilfe bei Problemen

- **Das Display wird im Browser nicht gefunden:** anderes USB-Kabel (muss
  Daten übertragen) oder einen anderen USB-Anschluss probieren.
- **Der PC piept im Sekundentakt:** Das Board ist leer (z.B. nach einer
  abgebrochenen Installation) und startet ständig neu. Dann die Notlösung
  verwenden.
- **Notlösung (Download-Modus):** Kabel abziehen, die Taste **BOOT**
  gedrückt halten und dabei das Kabel einstecken, nach zwei Sekunden
  loslassen. Dann im Web-Installer **Installieren**. **Wichtig:** Nach dem
  Aufspielen startet das Display erst, wenn du das Kabel ab- und wieder
  ansteckst – bis dahin tut sich auf dem Bildschirm nichts. Danach
  **Installieren** → *Connect to Wi-Fi*.
- **Nach der Installation gibt es kein *Connect to Wi-Fi*:** Kabel ab- und
  wieder anstecken, etwa zehn Sekunden warten, bis das Display
  „Keine WLAN-Daten“ zeigt, dann erneut **Installieren**.
- **Mein WLAN fehlt in der Liste:** Dialog schließen und *Connect to Wi-Fi*
  erneut öffnen, oder *Join other…* und den Namen von Hand eingeben. Das
  Display kann nur 2,4-GHz-WLAN.
- **„Keine WLAN-Daten“ auf dem Display:** Es ist noch kein WLAN
  gespeichert (z.B. nach Werkseinstellungen) – per USB im Web-Installer
  *Connect to Wi-Fi*.
- **WLAN-Fehlerbildschirm:** Das Display zeigt die vermutete Ursache und
  versucht es alle 10 Sekunden erneut. Stimmt das Passwort nicht, im
  Web-Installer *Change Wi-Fi*.
- **Das Portal ist nicht erreichbar:** Es ist nur 30 Minuten nach dem
  Öffnen erreichbar – Taste GPIO 21 drei Sekunden halten und die Adresse
  aus dem System-Log verwenden. Handy/PC müssen im selben WLAN sein.

## Station finden

Normalerweise sucht man die Station im
[Einstellungsportal](#einstellungsportal) per Name. Die ID (`globalId`,
Format `de:09162:2`) lässt sich auch von Hand eintragen:

1. Die Haltestellenliste öffnen:
   [`haltestellen/Haltestellen_Suche_s26.csv`](haltestellen/Haltestellen_Suche_s26.csv)
   – auf GitHub wird sie als Tabelle mit Suchfeld angezeigt. Für Excel gibt
   es die Originaldatei des MVV im selben Ordner.
2. Im Suchfeld („Search this file…“) den Stationsnamen eingeben. Bei
   gleichnamigen Haltestellen auf die Spalte *Ort* achten.
3. Den Wert aus der Spalte *Globale ID* im Portal ins Feld *Station-ID*
   eintragen.
4. Optional prüfen: `https://www.mvg.de/api/bgw-pt/v3/departures?globalId=<ID>`
   im Browser aufrufen – kommt eine Liste mit Abfahrten zurück, stimmt die ID.

Mehr zur Liste (Spalten, Quelle, Lizenz): [`haltestellen/README.md`](haltestellen/README.md).

### Richtungen prüfen

Nur nötig für die getrennte Anzeige. Am einfachsten mit *Richtungen
anzeigen* im Portal. Von Hand: in der Abfahrtsliste aus Schritt 4 bei
einigen Einträgen `lineId` (enthält `:H:` oder `:R:`) mit `destination`
vergleichen. Eine feste Regel gibt es nicht – das hängt von Linie und
Station ab.

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
- Das Einstellungsportal hat kein Passwort und nutzt `http` (siehe
  [Einstellungsportal](#einstellungsportal)).

## Selbst kompilieren

Nur nötig, wer den Code ändern möchte – zum Einrichten reicht der
Web-Installer.

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
  `setup()` und `loop()` (z.B. für eine Status-LED oder eine
  Fernüberwachung). Die Standardfassungen in `extras.cpp` sind leer und als
  `weak` markiert. Für eine Erweiterung eine eigene `.cpp` in `src/`
  anlegen und dieselben Funktionen dort neu definieren – der übrige Code
  bleibt unverändert, Updates lassen sich so ohne Konflikte übernehmen.
- **`#include <HTTPClient.h>` im .ino nicht entfernen:** Ist die Library
  ArduinoHttpClient installiert, bindet die IDE unter Windows sonst deren
  `HttpClient.h` ein (Groß-/Kleinschreibung).
- **Serielle Ausgabe:** `Serial.setTxTimeoutMs(0)` direkt nach
  `Serial.begin()` nicht entfernen – sonst wartet jede Ausgabe bis zu 2 s,
  wenn das Board am PC hängt und kein Programm mitliest.

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
- **Web-Installer:** nutzt [ESP Web Tools](https://github.com/esphome/esp-web-tools)
  (Apache-2.0), geladen von unpkg.com, nicht in diesem Repository enthalten.
- Verwendete Libraries (nicht in diesem Repository enthalten) stehen unter
  ihren eigenen Lizenzen.

Keine Gewähr für Richtigkeit und Vollständigkeit der angezeigten Daten.
