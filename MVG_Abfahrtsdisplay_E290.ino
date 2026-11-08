// Version: 2.5.0
// Letzte Änderung: 08.11.2026 14:27
#define FW_VERSION "2.5.0"

// ------------------------------------------------------------
// Konfiguration
// ------------------------------------------------------------
// Die Geraete-Einstellungen (WLAN, Station, Anzeige, Verkehrsmittel,
// WLAN-QR) liest src/settings beim Start aus dem Geraetespeicher (NVS).
// Fehlt dort ein Wert, gilt der Standardwert aus config.h bzw. aus
// secrets.h (optional, Vorlage: secrets_example.h). Im Code immer
// appSettings verwenden, nicht die Werte aus config.h/secrets.h direkt.

// ------------------------------------------------------------
// Uebersicht
// ------------------------------------------------------------
// E-Ink-Abfahrtsdisplay - Heltec Vision Master E290 (MVG, Muenchen)
//
// Funktionen, Einrichtung, Bedienung und wichtige Hinweise: README.md
// Selbst kompilieren, Aufbau, technische Hinweise: README_TECHNIK.md
// Aenderungen je Version: CHANGELOG.md
// Lizenz: GPL-3.0-or-later (siehe LICENSE)
//
// Dateien (Hauptordner = alles, was pro Geraet angepasst wird):
//   dieses .ino    Ablaufsteuerung (setup/loop, Tasten-Aktionen, WLAN-Fehler)
//   config.h       Standardwerte der Einstellungen + gemeinsame Konstanten
//   secrets.h      optional: WLAN-Vorbelegung (nie teilen; Vorlage:
//                  secrets_example.h)
//   partitions.csv Partitionsschema (nie aendern, sonst gehen die
//                  gespeicherten Einstellungen bei Updates verloren)
// Web-Installer (fertige Firmware, GitHub Pages): docs/, Firmware dorthin
//   mit werkzeuge/firmware_fuer_installer.ps1 (Export ohne secrets.h)
// Programmcode in src/ (Arduino-IDE kompiliert nur einen Ordner namens src):
//   settings       Einstellungen: NVS mit Standardwerten aus config.h
//   improv_serial  WLAN-Einrichtung per USB aus dem Browser (Improv)
//   portal         Einstellungsportal im Heimnetz (Webseite, auf Abruf,
//                  inkl. Firmware-Upload)
//   mvg_api        Abruf/Auswertung der MVG-API
//   line_select    Linienauswahl (Linienliste einer Station fuers Portal)
//   display        alles, was gezeichnet wird
//   line_icons.h   Liniensymbole (S/U/Tram als Bitmap, Bus generiert)
//   buttons        Tastenauswertung
//   stats          API-Stoerungen, WLAN-Signalbewertung
//   time_utils     Laufzeit, Zeitformate
//   text_utils     UTF-8 -> Latin-1 fuer echte Umlaute auf dem Display
//   wifi_diag      Ursache von WLAN-Abbruechen fuer den Fehlerbildschirm
//   wifi_conn      WLAN verbinden und nach Abbruechen wiederherstellen
//   debug_log.h    Zusatz-Diagnose im seriellen Monitor (DEBUG_LOG in config.h)
//   extras         Andockstellen fuer optionale Erweiterungen (Abschnitt
//                  Erweiterungen)
// Startbildschirm: Abschnitt Startbildschirm am Dateiende

// ------------------------------------------------------------
// Includes
// ------------------------------------------------------------
#include "WiFi.h"
// HTTPClient.h wird nur in mvg_api.cpp gebraucht, MUSS aber hier im .ino
// stehen: Die Arduino-IDE sucht Libraries in der Reihenfolge der Dateien.
// Ist die Library ArduinoHttpClient installiert (z.B. als Abhaengigkeit
// einer anderen Library), wird sonst deren "HttpClient.h" unter Windows
// (Gross-/Kleinschreibung egal) faelschlich statt der ESP32-"HTTPClient.h"
// eingebunden.
#include <HTTPClient.h>
#include <time.h>
#include "config.h"
#include "src/settings.h"
#include "src/improv_serial.h"
#include "src/portal.h"
#include "src/mvg_api.h"
#include "src/display.h"
#include "src/buttons.h"
#include "src/stats.h"
#include "src/time_utils.h"
#include "src/text_utils.h"
#include "src/wifi_diag.h"
#include "src/wifi_conn.h"
#include "src/extras.h"

// ------------------------------------------------------------
// Globale Zustandsvariablen
// ------------------------------------------------------------
unsigned long lastErrorRetry = 0;
unsigned long autoResetStart = 0;
unsigned long qrModeStart = 0;
unsigned long logModeStart = 0;
bool autoResetPending = false;
bool qrModeActive = false;
bool logModeActive = false;
int updateCounter = 0;
int lastUpdateMinute = -1;

// Angezeigte Ansicht: Standardansicht (BTN_NONE) oder die Ansicht einer
// Taste (BTN_DIRECTION, BTN_STATION2, BTN_PAGE2 - Belegung im Portal).
// Dieselbe Taste erneut oder der Auto-Reset (abschaltbar im Portal) fuehrt
// zur Standardansicht zurueck, die andere Taste zeigt direkt ihre Ansicht.
uint8_t activeKey = BTN_NONE;
// Angezeigte Station: 0 = Station 1, 1 = Station 2 (bei BTN_STATION2)
int activeStation = 0;
String stationName = "Bahnhof";       // Station 1 (Latin-1 fuer die Anzeige)
String station2Name = "Station 2";    // Station 2

// Zwischenspeicher der zuletzt abgerufenen Abfahrten. Tasten und
// Auto-Reset zeichnen nur daraus neu - abgerufen wird nur beim
// Minuten-Update (plus Start, WLAN-Wiederkehr und Retries bei Stoerung).
// Station 1: umschaltbare Richtung (dir1 'U') je Richtung ein Speicher,
// sonst cacheAll - jeweils Seite 1 + Seite 2.
Departure cacheDirH[MAX_DEPARTURES_SHOWN * 2];
Departure cacheDirR[MAX_DEPARTURES_SHOWN * 2];
int cacheDirHCount = 0;
int cacheDirRCount = 0;
Departure cacheAll[MAX_DEPARTURES_SHOWN * 2];
int cacheAllCount = 0;
// Station 2 (mit ihrer Richtung, ohne Seite 2). Ist sie einer Taste
// zugeordnet, werden beide Stationen jede Minute abgerufen, damit das
// Umschalten ohne Abruf auskommt. lastSuccess: Zeitpunkt des letzten erfolgreichen Abrufs je
// Station (hasData: ueberhaupt schon einer), siehe cacheFresh().
Departure cacheStation2[MAX_DEPARTURES_SHOWN];
int cacheStation2Count = 0;
unsigned long lastSuccess[2] = { 0, 0 };
bool hasData[2] = { false, false };
// Abruf misslungen, Anzeige ueberbrueckt (noch kein Fehlerbildschirm):
// neuer Versuch alle ERROR_RETRY_INTERVAL_MS (ab lastErrorRetry).
// failSince: erster Fehlschlag der laufenden Serie.
bool apiRetryPending = false;
bool apiFailing = false;
unsigned long failSince = 0;
// Seit dem Start hat schon ein Abruf geklappt (Bedingung fuer den Neustart
// nach API_RESTART_AFTER_MS, verhindert eine Neustart-Schleife)
bool everFetched = false;

// WLAN-Abbrueche seit dem Start (fuer Erweiterungen, siehe extras.h;
// API-Stoerungen: siehe stats.cpp)
int wifiDisconnectCount = 0;

// Einrichtungs-Screen (noch keine Station) wird gerade angezeigt
bool setupScreenShown = false;
// Neue WLAN-Daten per Improv waehrend setup(): Portal danach oeffnen
bool portalRequested = false;
// Screen "Firmware-Update laeuft" wird gerade angezeigt
bool updateScreenShown = false;

// WLAN-Fehlerbildschirm: seit wann keine Verbindung besteht und welche
// Ursache zuletzt angezeigt wurde (nullptr = noch kein Fehlerbildschirm)
unsigned long wifiLostSince = 0;
// WLAN weg, aber die Abfahrten bleiben noch stehen (bis
// WIFI_LOST_SCREEN_DELAY_MS): zur vollen Minute aus dem Zwischenspeicher
// neu zeichnen, damit die Uhrzeit weiterlaeuft
bool wifiBridging = false;
const char* wifiErrorShownReason = nullptr;

// Startbildschirm: Zeitpunkt der Anzeige (Abschnitt Startbildschirm)
unsigned long splashStart = 0;

enum SystemState { STATE_NORMAL, STATE_WIFI_ERROR, STATE_API_ERROR };
SystemState currentState = STATE_NORMAL;

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  // Ausgaben ueber den USB-Anschluss nicht abwarten: Haengt das Board am PC,
  // ohne dass ein Programm mitliest (serieller Monitor zu), blockiert sonst
  // jede Ausgabe bis zu 2 s (Boardpaket 3.3.x) - Start, Tasten und Portal
  // werden dann extrem traege. Ungelesene Ausgaben gehen verloren.
  Serial.setTxTimeoutMs(0);
#endif
  delay(1000);

  Serial.print("Abfahrtsdisplay Firmware v");
  Serial.println(firmwareVersionText());

  settingsLoad();
  settingsPrint();

  // Frueh starten, damit der Web-Installer das Geraet schon waehrend des
  // Startbildschirms erkennt
  improvBegin(firmwareVersionText().c_str());
  portalBegin(firmwareVersionText().c_str());

  extrasBegin();
  buttonsInit();
  displayInit();

  // Startbildschirm - alles Folgende laeuft im Hintergrund weiter
  displayShowSplash(firmwareVersionText().c_str());
  splashStart = millis();

  // WLAN-Daten nur aus appSettings, nicht zusaetzlich im WLAN-Speicher des
  // ESP32 ablegen (Werkseinstellungen loeschen sonst nicht alles)
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  // WLAN-Stromsparmodus aus: Im Sparmodus verpasst der ESP32 bei manchen
  // Routern Datenpakete und wird getrennt. Mehrverbrauch bei USB-Betrieb
  // ohne Bedeutung.
  WiFi.setSleep(false);
  wifiDiagInit();   // vor WiFi.begin(): merkt sich die Gruende von Abbruechen
  // Staerksten Zugangspunkt waehlen, eigenes Wiederverbinden mit Pausen
  wifiConnInit(!extrasHandlesWifiReconnect());
  if (settingsHasWifi()) {
    Serial.print("Verbinde mit ");
    Serial.println(appSettings.wifiSsid);
    wifiConnBegin(appSettings.wifiSsid.c_str(), appSettings.wifiPassword.c_str());
  } else {
    Serial.println("Keine WLAN-Daten - Einrichtung per Web-Installer (WLAN verbinden)");
  }

  // Klappt die Anmeldung nicht innerhalb von WIFI_ERROR_SCREEN_DELAY_MS,
  // erscheint der Fehlerbildschirm mit der vermuteten Ursache (fruehestens
  // nach dem Startbildschirm; ohne WLAN-Daten sofort danach). Neue
  // Versuche startet wifiConnLoop() - erst nach dem Ende des laufenden,
  // mit wachsenden Pausen.
  // Neue WLAN-Daten per Improv verbinden das Board direkt, die Schleife
  // endet dann von selbst.
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (improvLoop()) portalRequested = true;
    extrasStatus(true);
    delay(300);
    Serial.print(".");
    bool errorDue = !settingsHasWifi() || millis() - wifiStart >= WIFI_ERROR_SCREEN_DELAY_MS;
    if (errorDue && !splashShowing()) {
      showWifiErrorIfChanged();
    }
    if (settingsHasWifi() && !improvBusy()) wifiConnLoop();
  }
  if (improvLoop()) portalRequested = true;

  Serial.println("");
  Serial.println("WLAN verbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  // Nach "WLAN verbinden" im Web-Installer: Portal fuer die weitere
  // Einrichtung oeffnen ("Geraet oeffnen" im Browser)
  if (portalRequested) portalOpen();

  configTzTime(TIMEZONE_INFO, "de.pool.ntp.org", "time.google.com");

  Serial.print("Warte auf Zeitabgleich");
  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 20) {
    Serial.print(".");
    delay(500);
    retries++;
  }
  Serial.println();

  // Bei Fehler bleibt der Fallback "Bahnhof" stehen
  fetchStationNames();

  extrasSetup(firmwareVersionText().c_str());

  // Erste Abfahrten schon waehrend des Startbildschirms laden, gezeichnet
  // wird erst danach (Abfahrten, API-Fehler- oder Einrichtungs-Screen)
  if (settingsHasStation()) {
    if (fetchDepartures()) fetchOtherStation();
  }
  lastUpdateMinute = timeinfo.tm_min;
  finishSplash();
}

// ------------------------------------------------------------
// Loop
// ------------------------------------------------------------
void loop() {
  // Firmware-Upload im Portal: nur den Update-Screen zeigen, keine Abrufe
  // und Display-Updates. portalLoop() startet nach Erfolg neu; bei einem
  // Fehler wird die Anzeige danach neu aufgebaut.
  if (portalUpdateRunning()) {
    if (!updateScreenShown) {
      displayShowUpdate();
      updateScreenShown = true;
    }
    portalLoop();
    delay(50);
    return;
  }
  if (updateScreenShown) {
    updateScreenShown = false;
    applyNewSettings();
  }

  if (improvLoop()) portalOpen();   // neue WLAN-Daten: Portal fuer "Geraet oeffnen"
  extrasLoop();
  checkApiFailWindow();

  if (WiFi.status() != WL_CONNECTED) {
    if (currentState != STATE_WIFI_ERROR) {
      Serial.println("WLAN-Verbindung verloren!");
      // Ueberbruecken nur, wenn gerade Abfahrten zu sehen sind
      wifiBridging = currentState == STATE_NORMAL && hasData[activeStation] &&
                     !qrModeActive && !logModeActive;
      currentState = STATE_WIFI_ERROR;
      wifiDisconnectCount++;
      wifiLostSince = millis();
      wifiErrorShownReason = nullptr;
      lastErrorRetry = millis();
    }

    // Fehlerbildschirm erst nach WIFI_LOST_SCREEN_DELAY_MS - kurze
    // Aussetzer bleiben auf dem Display unsichtbar
    if (millis() - wifiLostSince >= WIFI_LOST_SCREEN_DELAY_MS) {
      wifiBridging = false;
      showWifiErrorIfChanged();
    } else if (wifiBridging) {
      // Tasten wirken weiter (Ansichten aus dem Zwischenspeicher)
      bool showingDepartures = handleKeysAndScreens();
      time_t nowRaw = time(nullptr);
      struct tm nowInfo;
      localtime_r(&nowRaw, &nowInfo);
      if (showingDepartures && nowInfo.tm_min != lastUpdateMinute) {
        lastUpdateMinute = nowInfo.tm_min;
        Serial.println("Kein WLAN - Anzeige ueberbrueckt");
        redrawFromCache(false);
      }
    }

    // Neu verbinden (wifiConnLoop) - ausser eine Erweiterung uebernimmt
    // das selbst (extrasHandlesWifiReconnect(), siehe extras.h) oder
    // Improv benutzt gerade das WLAN
    if (!extrasHandlesWifiReconnect() && !improvBusy() && settingsHasWifi()) {
      wifiConnLoop();
    }
    extrasStatus(true);

    delay(50);
    return;
  }

  if (currentState == STATE_WIFI_ERROR) {
    Serial.println("WLAN wiederhergestellt!");
    currentState = STATE_NORMAL;
    // Fehlschlaege ohne WLAN zaehlen nicht fuer Ueberbruecken/Neustart
    apiFailing = false;
    setupScreenShown = false;   // IP-Adresse kann sich geaendert haben
    if (settingsHasStation()) attemptUpdate(true, true);
    updateCounter = 0;
  }

  // Im Portal gespeicherte Einstellungen uebernehmen
  if (portalLoop()) applyNewSettings();

  // Ersteinrichtung: ohne Station bleibt das Portal offen und der
  // Einrichtungs-Screen stehen
  if (!settingsHasStation()) {
    portalOpen();
    if (!setupScreenShown) showPortalSetupScreen();
    extrasStatus(false);
    delay(50);
    return;
  }

  extrasStatus(currentState != STATE_NORMAL);

  // Tasten, QR-/Log-Screen, Auto-Reset (QR-/Log-Screen steht: hier Ende)
  if (!handleKeysAndScreens()) return;

  if (currentState == STATE_API_ERROR) {
    // Scheitern die Abrufe trotz WLAN schon sehr lange, hilft oft ein
    // Neustart (z.B. wenn die Netzwerk-/TLS-Verbindung des Geraets haengt).
    // Nicht bei offenem Portal (Nutzer koennte gerade etwas einstellen).
    if (apiFailing && everFetched && !portalIsOpen() &&
        millis() - failSince >= API_RESTART_AFTER_MS) {
      Serial.println("Abrufe scheitern seit 10 Minuten - Neustart");
      restartCleanly();
    }
    if (millis() - lastErrorRetry >= ERROR_RETRY_INTERVAL_MS) {
      Serial.println("Erneuter API-Versuch nach Fehler...");
      attemptUpdate(true, true);
      lastErrorRetry = millis();
    }
    return;
  }

  // Abruf misslungen, aber die Daten sind noch keine
  // API_ERROR_SCREEN_DELAY_MS alt: Anzeige bleibt, leiser neuer Versuch
  if (apiRetryPending && millis() - lastErrorRetry >= ERROR_RETRY_INTERVAL_MS) {
    Serial.println("Erneuter Abrufversuch (Anzeige ueberbrueckt)...");
    if (fetchDepartures()) {
      redrawFromCache(false);
      fetchOtherStation();
    } else if (currentState == STATE_API_ERROR) {
      displayShowApiError();
    }
  }

  time_t nowRaw = time(nullptr);
  struct tm nowInfo;
  localtime_r(&nowRaw, &nowInfo);

  // ">=" statt "==": verpasst die Loop Sekunde 1 (z.B. weil ein Display-
  // Update oder ein Abruf kurz blockiert), wird das Update in derselben
  // Minute nachgeholt
  if (nowInfo.tm_sec >= UPDATE_TARGET_SECOND && nowInfo.tm_min != lastUpdateMinute) {
    lastUpdateMinute = nowInfo.tm_min;
    updateCounter++;

    // Abstand der Full Refreshs in Minuten aus den Einstellungen (Portal)
    bool doFullRefresh = (updateCounter >= appSettings.fullRefreshMin);
    if (doFullRefresh) {
      updateCounter = 0;
    }

    Serial.print("Update #");
    Serial.print(updateCounter);
    Serial.println(doFullRefresh ? " (Full Refresh)" : " (Fast Mode)");
    wifiConnLogStatus();

    attemptUpdate(doFullRefresh, true);
  }
}

// ------------------------------------------------------------
// Update-Steuerung
// ------------------------------------------------------------
// Einziger Ort, an dem Abfahrten von der MVG-API abgerufen werden
// (fetchDepartures). Laedt die Rohdaten einmal, wertet sie fuer alle
// Ansichten aus (Zwischenspeicher) und zeichnet dann die aktuelle Ansicht.
// Mit zwei Stationen wird danach auch die andere Station abgerufen
// (withOther), damit das Umschalten sofort aus dem Zwischenspeicher geht.
void attemptUpdate(bool preferFullRefresh, bool withOther) {
  bool wasApiError = (currentState == STATE_API_ERROR);

  if (!fetchDepartures()) {
    if (currentState == STATE_API_ERROR) {
      // Fehlerbildschirm nur beim Wechsel normal -> Fehler, nicht bei
      // jedem Retry waehrend einer laufenden Stoerung
      if (!wasApiError) displayShowApiError();
    } else {
      // Ueberbrueckt: bisherige Abfahrten mit aktueller Uhrzeit
      redrawFromCache(preferFullRefresh);
    }
    return;
  }
  redrawFromCache(preferFullRefresh);
  if (withOther) fetchOtherStation();
}

// Tasten auswerten (Belegung im Portal: Ansicht zeigen bzw. zurueck,
// mittlere Taste lang = Log-Screen), Ende von QR-/Log-Screen und Auto-Reset
// zur Standardansicht. Auch waehrend ein WLAN-Ausfall ueberbrueckt wird.
// Rueckgabe false: QR- oder Log-Screen steht noch.
bool handleKeysAndScreens() {
  handleQrButton(triggerMiddleAction, triggerLogAction);
  handleBootButton(triggerLeftAction);

  // Differenzform (millis() - start >= dauer) ist ueberlaufsicher
  if (logModeActive) {
    if (millis() - logModeStart < LOG_DISPLAY_DURATION_MS) return false;
    Serial.println("Info-Anzeige beendet (Timeout)");
    logModeActive = false;
    returnToDepartures();
  }
  if (qrModeActive) {
    if (millis() - qrModeStart < QR_DISPLAY_DURATION_MS) return false;
    Serial.println("QR-Anzeige beendet (Timeout), zurueck zu Abfahrten");
    qrModeActive = false;
    returnToDepartures();
  }

  const unsigned long viewResetMs =
      activeKey == BTN_STATION2    ? STATION_AUTO_RESET_MS
      : activeKey == BTN_DIRECTION ? DIRECTION_AUTO_RESET_MS
                                   : PAGE_AUTO_RESET_MS;
  if (autoResetPending && millis() - autoResetStart >= viewResetMs) {
    autoResetPending = false;
    Serial.println("Auto-Reset: zurueck zur Standardansicht");
    resetViewToDefault();
    updateCounter++;
  }
  return true;
}

// Station 2 eingerichtet und ueber eine Taste erreichbar
bool station2Used() {
  return settingsHasStation2() && (appSettings.keyLeft == BTN_STATION2 ||
                                   appSettings.keyMid == BTN_STATION2);
}

// Angezeigte Richtungskennung: 'B' alle, 'H' oder 'R'. Station 1 mit
// umschaltbarer Richtung: Startansicht, mit BTN_DIRECTION die andere.
char viewDirection() {
  if (activeStation == 1) return appSettings.dir2;
  if (appSettings.dir1 != 'U') return appSettings.dir1;
  char start = appSettings.start1;
  if (activeKey != BTN_DIRECTION) return start;
  return start == 'H' ? 'R' : 'H';
}

// Richtung im Header (dirLabel fuer displayShowDepartures): 'Z'/'A' bei
// zugeordneter Zentrum-Kennung, ohne Zuordnung bei umschaltbarer Richtung
// die Kennung selbst ('H'/'R'), sonst keine (0)
char headerDirection(char dir) {
  if (dir != 'H' && dir != 'R') return 0;
  char zentrum = activeStation == 1 ? appSettings.zentrum2 : appSettings.zentrum1;
  if (zentrum == 'H' || zentrum == 'R') return dir == zentrum ? 'Z' : 'A';
  if (activeStation == 0 && appSettings.dir1 == 'U') return dir;
  return 0;
}

// Stationsnamen setzen (Start, neue Einstellungen): gespeicherte Namen
// (Portal) direkt, fehlende einmal bei der MVG abrufen und speichern (z.B.
// nach dem Update von 2.2.0). Bei Fehler bleibt der Platzhalter stehen.
void fetchStationNames() {
  String name1 = appSettings.name1;
  String name2 = appSettings.name2;
  bool fetched = false;
  if (settingsHasStation() && name1.length() == 0) {
    fetched |= fetchStationName(appSettings.stationId.c_str(), name1, false);
  }
  if (settingsHasStation2() && name2.length() == 0) {
    fetched |= fetchStationName(appSettings.station2Id.c_str(), name2, false);
  }
  if (fetched) settingsSaveStationNames(name1, name2);

  stationName = name1.length() ? utf8ToLatin1(name1) : String("Bahnhof");
  station2Name = name2.length() ? utf8ToLatin1(name2) : String("Station 2");
}

// Die gerade nicht angezeigte Station abrufen (nur mit zwei Stationen).
// Ein Fehler aendert die Anzeige nicht - beim Umschalten wird dann neu
// abgerufen.
void fetchOtherStation() {
  if (!station2Used()) return;
  int other = 1 - activeStation;
  if (!fetchStation(other)) {
    Serial.print("Abruf Station ");
    Serial.print(other + 1);
    Serial.println(" (nicht angezeigt) fehlgeschlagen");
  }
}

// Daten der Station noch aktuell genug, um sie beim Umschalten ohne Abruf
// zu zeigen: hoechstens so alt wie beim Ueberbruecken einer Stoerung
// (Minuten-Takt + API_ERROR_SCREEN_DELAY_MS)
bool cacheFresh(int index) {
  return hasData[index] &&
         millis() - lastSuccess[index] < 60000UL + API_ERROR_SCREEN_DELAY_MS;
}

// Laedt die Abfahrten der angezeigten Station und fuellt den
// Zwischenspeicher, ohne zu zeichnen. true bei Erfolg. Bei einem
// Fehlschlag bleibt currentState normal und apiRetryPending wird gesetzt -
// die Anzeige ueberbrueckt die Stoerung -, solange Daten da sind und die
// Fehlschlaege noch keine API_ERROR_SCREEN_DELAY_MS andauern. Sonst:
// STATE_API_ERROR (Fehlerbildschirm, API-Fail zaehlen).
// Blockiert hoechstens einen Abruf lang (bis HTTP_TIMEOUT_MS).
bool fetchDepartures() {
  if (fetchStation(activeStation)) {
    currentState = STATE_NORMAL;
    apiRetryPending = false;
    apiFailing = false;
    return true;
  }
  // Laufende Stoerung: loop() versucht es alle ERROR_RETRY_INTERVAL_MS
  if (currentState == STATE_API_ERROR) return false;

  lastErrorRetry = millis();
  if (!apiFailing) {
    apiFailing = true;
    failSince = millis();
  }
  if (hasData[activeStation] && millis() - failSince < API_ERROR_SCREEN_DELAY_MS) {
    Serial.println("Abruf fehlgeschlagen - Anzeige bleibt, neuer Versuch in 10 s");
    apiRetryPending = true;
    return false;
  }
  // Neue Stoerung zaehlen (nicht jeden einzelnen Retry)
  recordApiFail();
  currentState = STATE_API_ERROR;
  apiRetryPending = false;
  return false;
}

// Ein Abrufversuch: Abfahrten einer Station (0 oder 1) laden und fuer alle
// Ansichten auswerten. Aendert den Zwischenspeicher nur bei Erfolg und
// merkt sich dann den Zeitpunkt (lastSuccess). true bei Erfolg.
bool fetchStation(int index) {
  StationConfig station = stationConfig(index);
  LineSelection lines;
  lines.parse(station.lines);
  JsonDocument doc;
  if (!downloadDepartures(station, lines, doc)) return false;

  // Richtung der Station (Portal "nur Richtung H/R") gilt zusaetzlich zur
  // Linienauswahl
  DirectionFilter filter = DIR_FILTER_ALL;
  if (station.dir == 'H') filter = DIR_FILTER_H;
  if (station.dir == 'R') filter = DIR_FILTER_R;

  if (index == 1) {
    cacheStation2Count = parseDepartures(doc, filter, lines, cacheStation2, MAX_DEPARTURES_SHOWN);
  } else if (station.dir == 'U') {
    // Umschaltbare Richtung: beide Richtungen aus denselben Daten (je Seite
    // 1 + 2), damit das Umschalten ohne neuen Abruf auskommt
    cacheDirHCount = parseDepartures(doc, DIR_FILTER_H, lines, cacheDirH, MAX_DEPARTURES_SHOWN * 2);
    cacheDirRCount = parseDepartures(doc, DIR_FILTER_R, lines, cacheDirR, MAX_DEPARTURES_SHOWN * 2);
  } else {
    // 8 Abfahrten fuer Seite 1 (1-4) und Seite 2 (5-8)
    cacheAllCount = parseDepartures(doc, filter, lines, cacheAll, MAX_DEPARTURES_SHOWN * 2);
  }
  lastSuccess[index] = millis();
  hasData[index] = true;
  everFetched = true;
  return true;
}

// Zeichnet die aktuelle Ansicht (Station, Richtung, Seite) aus dem
// Zwischenspeicher - ohne Netzwerkzugriff.
void redrawFromCache(bool fullRefresh) {
  char dir = viewDirection();
  const Departure* list = cacheAll;
  int count = cacheAllCount;
  if (activeStation == 1) {
    list = cacheStation2;
    count = cacheStation2Count;
  } else if (appSettings.dir1 == 'U') {
    list = dir == 'H' ? cacheDirH : cacheDirR;
    count = dir == 'H' ? cacheDirHCount : cacheDirRCount;
  }

  // Stoerungszeile (nur Station 1): Seite 1 zeigt dann den Text und drei
  // Abfahrten, Seite 2 beginnt mit der vierten
  int incidentIndex = activeStation == 0 ? incidentRowIndex(list, count) : -1;
  int page1Rows = incidentIndex >= 0 ? MAX_DEPARTURES_SHOWN - 1 : MAX_DEPARTURES_SHOWN;

  // Seite 2 (Abfahrt 5-8 bzw. 4-7) nur bei Station 1
  bool page2 = activeKey == BTN_PAGE2 && activeStation == 0;
  int offset = page2 ? page1Rows : 0;
  count -= offset;
  if (count < 0) count = 0;
  if (count > (page2 ? MAX_DEPARTURES_SHOWN : page1Rows)) {
    count = page2 ? MAX_DEPARTURES_SHOWN : page1Rows;
  }

  // Nummer im Header nur, wenn Station 2 erreichbar ist
  int number = station2Used() ? activeStation + 1 : 0;
  displayShowDepartures(list + offset, count,
                        activeStation == 1 ? station2Name : stationName,
                        headerDirection(dir), page2, appSettings.showMinutes,
                        appSettings.showIncidents, fullRefresh, number,
                        page2 || incidentIndex < 0 ? nullptr : &list[incidentIndex]);
}

// Stoerungszeile: Index der fruehesten der ersten drei Abfahrten mit
// Stoerungstext (INCIDENT), -1 = keine Zeile (auch wenn im Portal
// abgeschaltet). Nur Seite 1 ist massgeblich - auch fuer Seite 2, damit
// dort keine Abfahrt fehlt.
int incidentRowIndex(const Departure list[], int count) {
  if (!appSettings.showIncidents || !appSettings.incidentText) return -1;
  int limit = min(count, MAX_DEPARTURES_SHOWN - 1);
  for (int i = 0; i < limit; i++) {
    if (list[i].incident.length() > 0) return i;
  }
  return -1;
}

// Nach Tastendruck/Auto-Reset: neu zeichnen ohne Abruf. Bei einer
// laufenden API-Stoerung bleibt der Fehlerbildschirm stehen (die Ansicht
// wird beim naechsten erfolgreichen Abruf gezeichnet).
void redrawCurrentView() {
  if (currentState == STATE_API_ERROR) return;
  redrawFromCache(true);
}

// Nach "Speichern" im Portal: Ansicht zuruecksetzen, Stationsname und
// Abfahrten neu laden (die Einstellungen koennen Station, Anzeigeart und
// Verkehrsmittel betreffen). QR- und Log-Screen werden dabei beendet.
void applyNewSettings() {
  Serial.println("Neue Einstellungen werden uebernommen");
  logModeActive = false;
  qrModeActive = false;
  autoResetPending = false;
  activeKey = BTN_NONE;
  activeStation = 0;
  hasData[0] = hasData[1] = false;
  apiRetryPending = false;
  apiFailing = false;
  setupScreenShown = false;
  if (!settingsHasStation()) return;

  fetchStationNames();

  time_t nowRaw = time(nullptr);
  struct tm nowInfo;
  localtime_r(&nowRaw, &nowInfo);
  lastUpdateMinute = nowInfo.tm_min;
  currentState = STATE_NORMAL;   // damit ein Fehler den API-Fehlerbildschirm zeigt
  attemptUpdate(true, true);
  updateCounter = 0;
}

// Einrichtungs-Screen mit QR-Code und Adresse des Portals
void showPortalSetupScreen() {
  String address = WiFi.localIP().toString();
  Serial.print("Einrichtung: Portal unter ");
  Serial.println(portalUrl());
  displayShowPortalSetup(portalUrl().c_str(), address.c_str());
  setupScreenShown = true;
}

// Rueckkehr aus QR- oder Log-Screen: Ist inzwischen eine neue Minute
// angebrochen, laeuft das faellige Minuten-Update jetzt (mit Abruf), sonst
// wird nur aus dem Zwischenspeicher gezeichnet. Bei API-Stoerung: neuer Versuch.
void returnToDepartures() {
  // WLAN-Ausfall wird ueberbrueckt: nur aus dem Zwischenspeicher
  if (WiFi.status() != WL_CONNECTED) {
    redrawFromCache(true);
    updateCounter = 0;
    return;
  }
  time_t nowRaw = time(nullptr);
  struct tm nowInfo;
  localtime_r(&nowRaw, &nowInfo);

  if (currentState == STATE_API_ERROR || nowInfo.tm_min != lastUpdateMinute) {
    lastUpdateMinute = nowInfo.tm_min;
    attemptUpdate(true, true);
  } else {
    redrawFromCache(true);
  }
  updateCounter = 0;
}

// ------------------------------------------------------------
// Tasten-Aktionen
// ------------------------------------------------------------
// Werden von handleBootButton()/handleQrButton() (buttons.cpp) aufgerufen.
// Jede Taste hat eine Belegung (Portal) und zeigt ihre Ansicht. Dieselbe
// Taste erneut fuehrt zurueck zur Standardansicht, die andere Taste zeigt
// direkt ihre Ansicht.

void triggerLeftAction() {
  keyPressed(appSettings.keyLeft, "Linke Taste");
}

void triggerMiddleAction() {
  keyPressed(appSettings.keyMid, "Mittlere Taste");
}

void keyPressed(uint8_t action, const char* keyName) {
  Serial.print(keyName);
  Serial.print(": ");

  // Log-Screen (mittlere Taste lang): jede Taste fuehrt zurueck
  if (logModeActive) {
    Serial.println("zurueck zu den Abfahrten");
    logModeActive = false;
    activeKey = BTN_NONE;
    autoResetPending = false;
    activeStation = 0;
    returnToDepartures();
    return;
  }

  // Angezeigte Tasten-Ansicht: dieselbe Taste (oder eine Taste ohne
  // Funktion) fuehrt zurueck zur Standardansicht, die andere Taste zeigt
  // direkt ihre Ansicht
  uint8_t current = qrModeActive ? BTN_WIFI_QR : activeKey;
  bool available = keyActionAvailable(action);
  if (current != BTN_NONE && (action == current || !available)) {
    autoResetPending = false;
    if (qrModeActive) {
      Serial.println("zurueck zu den Abfahrten");
      qrModeActive = false;
      returnToDepartures();
    } else {
      Serial.println("zurueck zur Standardansicht");
      resetViewToDefault();
      updateCounter++;
    }
    return;
  }
  if (!available) {
    Serial.println("keine Funktion");
    return;
  }

  bool fromQr = qrModeActive;
  qrModeActive = false;
  autoResetPending = false;

  if (action == BTN_WIFI_QR) {
    Serial.println("WLAN-QR-Code (60 s)");
    activeKey = BTN_NONE;
    activeStation = 0;
    qrModeActive = true;
    qrModeStart = millis();
    displayShowWifiQr(appSettings.qrTitle.c_str(), appSettings.qrSsid.c_str(),
                      appSettings.qrPassword.c_str());
    return;
  }

  int newStation = action == BTN_STATION2 ? 1 : 0;
  bool stationChanged = newStation != activeStation;
  activeKey = action;
  activeStation = newStation;
  if (action == BTN_STATION2) {
    Serial.println("Station 2");
  } else if (action == BTN_DIRECTION) {
    Serial.print("Richtung ");
    Serial.println(viewDirection());
  } else {
    Serial.println("Seite 2 (Abfahrt 5-8)");
  }
  // Zeichnen ohne Abruf, ausser die Station wechselt und ihre Daten sind
  // nicht aktuell (showActiveStation)
  if (stationChanged) {
    showActiveStation();
  } else if (fromQr) {
    returnToDepartures();
  } else {
    redrawCurrentView();
  }
  if (appSettings.viewAutoBack) {
    autoResetPending = true;
    autoResetStart = millis();
  }
  updateCounter++;
}

// Hat die Belegung gerade eine Funktion? (Station 2 eingerichtet,
// Richtung umschaltbar)
bool keyActionAvailable(uint8_t action) {
  switch (action) {
    case BTN_WIFI_QR:   return true;
    case BTN_STATION2:  return station2Used();
    case BTN_DIRECTION: return appSettings.dir1 == 'U';
    case BTN_PAGE2:     return true;
    default:            return false;
  }
}

// Station nach dem Umschalten zeigen: aus dem Zwischenspeicher, wenn ihr
// letzter Abruf geklappt hat, sonst jetzt abrufen (bei Fehler:
// Fehlerbildschirm)
void showActiveStation() {
  if (WiFi.status() != WL_CONNECTED) {
    // WLAN-Ausfall wird ueberbrueckt: Zwischenspeicher zeigen, den Zustand
    // (STATE_WIFI_ERROR) nicht aendern
    redrawFromCache(true);
  } else if (cacheFresh(activeStation)) {
    currentState = STATE_NORMAL;
    redrawFromCache(true);
  } else {
    Serial.println("Keine aktuellen Daten - Abruf");
    attemptUpdate(true, false);
  }
}

// Zurueck zur Standardansicht und neu zeichnen (Tastendruck, Auto-Reset)
void resetViewToDefault() {
  activeKey = BTN_NONE;
  if (activeStation != 0) {
    activeStation = 0;
    showActiveStation();
  } else {
    redrawCurrentView();   // kein Abruf, nur aus dem Zwischenspeicher
  }
}

void triggerLogAction() {
  if (logModeActive) {
    Serial.println("Info-Anzeige manuell beendet (langer Druck)");
    logModeActive = false;
    returnToDepartures();
  } else {
    Serial.println("Info-Anzeige aktiviert (langer Druck, 60s), Portal offen");
    logModeActive = true;
    logModeStart = millis();
    portalOpen();   // Laufzeit startet bei jedem Oeffnen neu
    String address = WiFi.localIP().toString();
    displayShowInfo(firmwareVersionText().c_str(), address.c_str(),
                    portalClosingTime().c_str());
  }
}

// ------------------------------------------------------------
// WLAN-Fehleranzeige
// ------------------------------------------------------------
// Zeichnet den WLAN-Fehlerbildschirm, wenn sich die vermutete Ursache
// (wifi_diag.cpp) seit der letzten Anzeige geaendert hat - sonst nichts,
// damit das E-Ink-Display nicht bei jedem Loop-Durchlauf neu zeichnet.
// Aufruf aus setup() (Erstverbindung) und loop() (Abbruch im Betrieb).
void showWifiErrorIfChanged() {
  // Ohne WLAN-Daten feste Texte, sonst die vermutete Ursache (wifi_diag)
  static const char* const NO_WIFI_REASON = "Keine WLAN-Daten";
  static const char* const NO_WIFI_HINT = "Einrichten per Web-Installer";
  bool hasWifi = settingsHasWifi();
  const char* reason = hasWifi ? wifiDiagReasonText() : NO_WIFI_REASON;
  const char* hint = hasWifi ? wifiDiagHintText() : NO_WIFI_HINT;
  if (reason == wifiErrorShownReason) return;
  wifiErrorShownReason = reason;
  // Fehlerbildschirm ersetzt einen QR-/Log-Screen aus der Ueberbrueckung
  qrModeActive = false;
  logModeActive = false;

  Serial.println();
  Serial.print("WLAN-Fehlerbildschirm: ");
  Serial.print(reason);
  Serial.print(" (Reason-Code ");
  Serial.print(wifiDiagLastReason());
  Serial.println(")");

  displayShowWifiError(hasWifi ? appSettings.wifiSsid.c_str() : "-", reason, hint, hasWifi);
}

// ------------------------------------------------------------
// Erweiterungen
// ------------------------------------------------------------
// Optionale Erweiterungen docken ueber src/extras.h an (extrasBegin,
// extrasSetup, extrasLoop, extrasStatus, extrasHandlesWifiReconnect,
// extrasVersionSuffix). Ohne Erweiterung sind das leere Funktionen.

// Versionsnummer inkl. Zusatz einer Erweiterung (z.B. "1.0.0-variante")
String firmwareVersionText() {
  return String(FW_VERSION) + extrasVersionSuffix();
}

// ------------------------------------------------------------
// Startbildschirm
// ------------------------------------------------------------
// Der Startbildschirm (displayShowSplash) erscheint direkt nach displayInit()
// und bleibt mindestens SPLASH_DURATION_MS stehen. WLAN, Uhrzeit,
// Stationsname und erster Abruf laufen in der Zeit weiter; gezeichnet wird
// erst danach. Dauert der WLAN-Aufbau laenger, bleibt er bis zur
// Verbindung bzw. bis zum WLAN-Fehlerbildschirm stehen.

// true, solange der Startbildschirm noch stehen bleiben soll
bool splashShowing() {
  return millis() - splashStart < SPLASH_DURATION_MS;
}

// Ende von setup(): Restzeit des Startbildschirms abwarten, dann die
// Abfahrten bzw. den API-Fehlerbildschirm zeichnen
void finishSplash() {
  while (splashShowing()) {
    improvLoop();
    extrasLoop();
    extrasStatus(currentState != STATE_NORMAL);
    delay(20);
  }
  Serial.println("Startbildschirm beendet");

  if (!settingsHasStation()) {
    portalOpen();
    showPortalSetupScreen();
    return;
  }
  if (currentState == STATE_API_ERROR) {
    displayShowApiError();
  } else {
    redrawFromCache(true);
  }
}
