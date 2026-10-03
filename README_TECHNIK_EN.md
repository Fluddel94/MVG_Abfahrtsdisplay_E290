# Technical notes & building it yourself – MVG Departure Display (Heltec Vision Master E290)

[Deutsch](README_TECHNIK.md) | **English**

Supplement to the [README](README_EN.md) for anyone who wants to change the
code, build the firmware themselves or know in more detail how the display
works. **None of this is needed for setup** – the
[web installer](https://fluddel94.github.io/MVG_Abfahrtsdisplay_E290/) is
enough for that.

Code comments and serial output are in German.

## Contents

- [Building it yourself](#building-it-yourself)
- [Station ID and directions by hand](#station-id-and-directions-by-hand)
- [Limitations in detail](#limitations-in-detail)
- [Files](#files)
- [Technical notes](#technical-notes-for-code-changes)

## Building it yourself

### Preparing the Arduino IDE

1. **Board package:** *Tools → Board → Boards Manager*, search for "esp32"
   and install **esp32 by Espressif Systems** (developed with version
   3.3.12).
2. **Libraries:** *Tools → Manage Libraries*, install:

   | Library | Note |
   |---|---|
   | heltec-eink-modules | display driver incl. graphics functions and fonts (tested with 4.6.0) |
   | ArduinoJson | version 7 |

   Nothing else is needed: Wi-Fi, HTTPS, web server, settings storage and
   the QR generator are part of the ESP32 board package, the graphics
   functions (derived from Adafruit GFX) are in heltec-eink-modules. The
   library "QRCode" by Richard Moore must **not** be installed: its
   `qrcode.h` has the same name as the board package's and would be
   included instead.
3. **Select the board:** *Tools → Board → esp32 →* **Heltec Vision Master
   E290**. Leave the other board settings at their defaults.

### Downloading the project

On GitHub *Code → Download ZIP*, extract. The folder must be called
`MVG_Abfahrtsdisplay_E290` – exactly like the `.ino` file inside, otherwise
the Arduino IDE won't open the sketch (the ZIP download names it
`MVG_Abfahrtsdisplay_E290-main`, so rename it). With Git (`git clone`) the
folder gets the right name directly.

### Defaults and uploading

- **`config.h`** contains the defaults of all settings (stop, optional
  second stop, view, modes of transport, Wi-Fi QR). They only apply as long
  as nothing is stored on the device – values saved in the portal always
  take precedence. Line selection and direction per stop exist only in the
  portal.
- **Debugging:** `DEBUG_LOG 1` in `config.h` enables additional output in
  the serial monitor (response sizes, free memory, individual steps of the
  line list). Use `0` for everyday use and releases.
- **`secrets.h`** is optional and presets Wi-Fi and Wi-Fi QR: copy the
  template `secrets_example.h`, rename it to `secrets.h`, fill in the
  values. `secrets.h` is listed in `.gitignore` and must never be shared –
  nor any firmware built with it (it contains the credentials).
- **Upload** via USB as usual. Stored settings are kept as long as *Erase
  All Flash Before Sketch Upload* is off in the IDE (default).
- **Never change `partitions.csv`** – otherwise stored settings are lost on
  updates.

### Building firmware for the web installer

For co-developers publishing a new version:

1. Remove `secrets.h` from the sketch folder (the firmware must not contain
   credentials).
2. Arduino IDE: *Sketch → Export Compiled Binary*.
3. Double-click `werkzeuge/firmware_fuer_installer.bat`. The script copies
   bootloader, partition table and firmware to `docs/firmware/` and writes
   the version from `FW_VERSION` into `docs/manifest.json`. It checks the
   files first and aborts if the firmware was built with `secrets.h`.
4. Commit and push – GitHub Pages publishes the `docs/` folder.

The firmware deliberately comes in four parts (bootloader, partitions,
`boot_app0.bin`, firmware) instead of one merged image: this way an update
via the installer doesn't overwrite the settings storage.

## Station ID and directions by hand

Normally you search for the stop by name in the
[settings portal](README_EN.md#settings-portal). The ID (`globalId`, format
`de:09162:2`) can also be entered by hand – in the portal field
*Station-ID* or as default `STATION_GLOBAL_ID` (or `STATION2_GLOBAL_ID`) in
`config.h`:

1. Open the stop list:
   [`haltestellen/Haltestellen_Suche_s26.csv`](haltestellen/Haltestellen_Suche_s26.csv)
   – GitHub shows it as a table with a search field. MVV's original file
   for Excel is in the same folder.
2. Type the stop name into the search field ("Search this file…"). For
   stops with the same name, check the column *Ort* (town).
3. Take the value from the column *Globale ID*.
4. Optional check: open
   `https://www.mvg.de/api/bgw-pt/v3/departures?globalId=<ID>` in the
   browser – if a list of departures comes back, the ID is correct.

More about the list (columns, source, license): [`haltestellen/README.md`](haltestellen/README.md) (German).

### Checking directions

Needed for the split view and for "only direction H/R". Easiest with
*Richtungen anzeigen* or *Linien auswählen* in the portal. By hand: in the
departure list from step 4, compare `lineId` (contains `:H:` or `:R:`) with
`destination` for a few entries. There is no fixed rule – it depends on
line and stop.

## Limitations in detail

- **Line selection:** the list in the portal comes from the MVG endpoint
  `lines/<globalId>` (all lines of the stop); grouped entries like "S6/8"
  and rail replacement are hidden. The example destinations per code H/R
  come from four departure requests covering about 12 hours
  (`offsetInMinutes` 0/120/360/720, 100 departures each) – lines without a
  trip in that period show "keine Fahrt in den nächsten 12 h" (no trip in
  the next 12 h). At most 16 selected lines per stop (`LINE_SELECT_MAX`),
  up to 80 lines are collected (`LINE_LIST_MAX`). Line labels are compared
  without spaces in upper case ("RE 80" = "RE80").
- **Rail replacement with line selection:** replacement buses for S-Bahn
  and tram carry the replaced line as label and match exactly. Replacement
  buses for regional trains only carry the train number – they appear as
  soon as any regional train is selected.
- Whether the direction markers `:H:`/`:R:` are as reliable everywhere for
  bus, tram and U-Bahn as for the S-Bahn has not been tested. Tangential
  lines run neither towards the centre nor outbound – the mixed view is
  often the better choice there.
- The API reports punctual departures without a real-time flag. "Live and
  on time" therefore can't be told apart from "no live data".
- 60 departures are requested (`API_DEPARTURE_LIMIT`), 100 with a line
  selection (`API_DEPARTURE_LIMIT_LINES`, the API's maximum). The API
  counts this limit across **all** modes of transport of the stop and only
  filters afterwards (observed, also with `transportTypes`). At large
  stations the preview therefore covers about 60–90 minutes; rarely served
  lines may produce fewer than 4 rows.
- Outside Munich, MVG returns the stop name without the town (`name`
  "Stadt Busbahnhof", `place` "Wasserburg am Inn"); only `name` is shown.
- The header shortens long stop names automatically ("." at the end).
- The settings portal has no password and uses `http`: there are no
  trusted certificates for devices on a home network, and a self-signed
  one leads to a full-page browser warning. That's why the portal only
  opens on request and closes after 30 minutes.

## Files

The main folder only contains what is adjusted per device; the program code
is in `src/`. The subfolder **must** be called `src` – besides the main
folder, the Arduino IDE only compiles this folder.

| File | Contents |
|---|---|
| `MVG_Abfahrtsdisplay_E290.ino` | control flow: `setup()`, `loop()`, button actions, Wi-Fi error display |
| `config.h` | defaults of the **device settings** and shared constants |
| `secrets_example.h` | template for `secrets.h` (optional: Wi-Fi preset) |
| `partitions.csv` | partition scheme – never change it, otherwise stored settings are lost on updates |
| `src/settings.h/.cpp` | settings in device storage (NVS), defaults from `config.h` |
| `src/improv_serial.h/.cpp` | Wi-Fi setup via USB from the browser (Improv Serial) |
| `src/portal.h/.cpp` | settings portal on the home network (web page, stop search, directions, line list, firmware upload) |
| `src/mvg_api.h/.cpp` | fetching and processing the MVG API |
| `src/line_select.h/.cpp` | line selection: collect the line list for the portal, evaluate the stored selection (pure data logic) |
| `src/debug_log.h` | additional diagnostics in the serial monitor (`DEBUG_LOG` in `config.h`) |
| `src/display.h/.cpp` | everything that is drawn |
| `src/line_icons.h` | line symbols and generator for bus/train symbols |
| `src/buttons.h/.cpp` | button handling (debouncing, short/long press) |
| `src/stats.h/.cpp` | API disruption statistics, Wi-Fi signal rating |
| `src/time_utils.h/.cpp` | uptime, time formats |
| `src/text_utils.h/.cpp` | conversion UTF-8 → Latin-1 for real umlauts |
| `src/wifi_diag.h/.cpp` | cause of Wi-Fi drops for the error screen |
| `src/extras.h/.cpp` | hooks for your own add-ons – empty in this version, no function (see [Technical notes](#technical-notes-for-code-changes)) |
| `src/FreeSans9pt8b.h`, `src/FreeSansBold9pt8b.h` | display fonts with umlauts |
| `docs/` | web installer (GitHub Pages): page, `manifest.json`, firmware in `docs/firmware/` |
| `werkzeuge/` | script that prepares the exported firmware for the web installer |
| `haltestellen/` | MVV stop list with global IDs (own license, see there) |
| `gehaeuse/` | 3D-printable case (own license, see there) |

## Technical notes (for code changes)

- **MVG API:** direction codes (`:H:`/`:R:`), message types and transport
  values (`SBAHN`, `UBAHN`, `TRAM`, `BUS`, `REGIONAL_BUS`, `BAHN`) were
  determined by observation, they are not documented. Message types:
  `INCIDENT` (disruption) and `EARLY_TERMINATION` (trip ends early) trigger
  a warning triangle, `INFO` (e.g. fare notes) is deliberately ignored. Add
  new trip-relevant message types in `mvg_api.cpp`.
- **Transport filter:** `downloadDepartures()` appends `&transportTypes=…`
  to the URL. Without this parameter the API only returns S-Bahn, U-Bahn,
  tram and city bus (observed).
- **API requests:** departures are fetched in `fetchStation()` (.ino,
  section Update-Steuerung) – on the minute update, at startup, after Wi-Fi
  returns, when returning from QR/log in a new minute and during a
  disruption. With two stops, `attemptUpdate()` first fetches and draws the
  displayed stop, then the other one (`fetchOtherStation()`); switching,
  paging and auto-reset only draw from the cache (data up to 2 minutes
  old, otherwise a new request).
- **Bridging disruptions:** if a request fails, the display keeps showing
  the departures (the time is updated on the full minute) and retries
  every `ERROR_RETRY_INTERVAL_MS` (10 s) without blocking. Only when the
  failures last `API_ERROR_SCREEN_DELAY_MS` (60 s) do the error screen and
  the API fail counter appear. Same for Wi-Fi: error screen during
  operation after `WIFI_LOST_SCREEN_DELAY_MS` (60 s), at startup after
  `WIFI_ERROR_SCREEN_DELAY_MS` (20 s).
- **Responses as a stream:** MVG answers without a length header and not
  chunked. `fetchJsonStream()` reads the response with `HttpBodyStream`
  (waits up to `HTTP_TIMEOUT_MS` for more data) directly into ArduinoJson,
  with a field filter – only what is evaluated gets stored (up to 46 KB
  response for 100 departures). Add newly evaluated fields to the
  respective filter.
- **Line filter:** `parseDepartures()` checks line (`lineKey`) and code
  (`:H:`/`:R:` from `lineId`) against the selection (`LineSelection`, text
  form `S2:H:S,RE80:B:Z` = line : H/R/B : transport code). The stop's
  direction ("only H/R") applies in addition. With a selection,
  `transportTypes` is derived from the selected lines.
- **Split trains:** the API returns split trains as separate trips.
  `mvg_api.cpp` merges entries with the same line, direction, planned time,
  platform and cancellation status (not for buses or missing platform). For
  early termination the shown destination counts; entries with the same
  destination become one row. Fixed short forms are in the table
  `SPLIT_TRAIN_LABELS`. Coupled regional trains with different line numbers
  (e.g. BRB RB 55/56/57) are merged by the fixed table `SPLIT_TRAIN_GROUPS`
  (symbol "RB"/"RE", destinations shortened evenly); the smallest delay of
  the parts with real-time data is shown.
- **Settings:** `src/settings` keeps all values in `appSettings` (NVS
  namespace `abfahrt`). Added in 2.2.0: `lines1`, `lines2` (line selection
  as text), `station2`, `types2` (modes of transport as bits), `dir1`,
  `dir2` (direction `B`/`H`/`R`). If they are missing (update from 2.1.0),
  the previous values count as stop 1. Never rename the NVS keys, otherwise
  stored settings are lost on updates. `appSettings` is only changed in the
  `loop()` context; other tasks read it holding a `SettingsLock`.
- **Web installer:** the firmware name in the Improv response
  (`IMPROV_FIRMWARE_NAME` in `improv_serial.cpp`) must match `name` in
  `docs/manifest.json` – only then does the installer offer *Update*
  without erasing. Never change the marker `FIRMWARE_MARKER` (firmware
  upload in the portal, check in the export script).
- **Line symbols:** add new bitmaps in `line_icons.h` – at most 36 px wide
  (checked by `static_assert` in `display.cpp`). The pixel font of the
  generated symbols knows 0–9, "N", "X", "R", "B", "E", "S", "V"; other
  characters appear in the built-in 6×8 font.
- **Umlauts:** the display fonts are Latin-1 encoded. Texts from the API and
  the settings (UTF-8) are converted by `utf8ToLatin1()`. Write fixed texts
  in the code as escapes, e.g. `"Ausw\xE4rts"` – if a character 0–9/a–f/A–F
  follows directly, split the string (`"Zur\xFC" "ck"`).
- **Wi-Fi cause:** `wifi_diag.cpp` evaluates the ESP32's reason code when
  the connection drops. A wrong password is usually only reported as a
  timeout during login – which can also happen with very weak reception,
  hence "Passwort falsch?" with a question mark.
- **Your own add-ons (hooks):** `src/extras.h` contains fixed points in the
  program flow – at startup, in the main loop, on errors, after a Wi-Fi
  change – where your own variant of the project can run additional code,
  e.g. drive a status LED or append a suffix to the version number. The
  shared code stays unchanged, so updates can be merged without conflicts.
  The author uses this himself for a private variant with extra functions
  for personal use.
  - **In this version the hooks do nothing:** the default versions in
    `extras.cpp` are empty (marked `weak`). Apart from the departure
    requests to MVG, the published firmware sends no data anywhere.
  - **An extension only becomes active** if someone puts their own code in
    `src/` and compiles the firmware themselves – not afterwards and not
    remotely.
  - **How to:** create your own `.cpp` in `src/` and define the same
    functions there; the linker then uses your version.
- **Portal requests:** `/suche?q=` (stop search), `/name?id=` (stop name),
  `/richtungen?id=&types=` (lines per code, at most 15 entries),
  `/linien?id=&types=` (line list, takes a few seconds), `/speichern`,
  `/update`. `types` = modes of transport as bits like `types2`. The page
  is a raw string in `portal.cpp` (umlauts as entities in the HTML, as
  UTF-8 in the JavaScript).
- **Don't remove `#include <HTTPClient.h>` from the .ino:** if the library
  ArduinoHttpClient is installed, the IDE on Windows would otherwise include
  its `HttpClient.h` (case sensitivity).
- **Serial output:** don't remove `Serial.setTxTimeoutMs(0)` right after
  `Serial.begin()` – otherwise every output waits up to 2 s when the board
  is connected to a computer and no program is reading.
