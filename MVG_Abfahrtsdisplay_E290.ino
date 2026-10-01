// Version: 2.0.0
// Letzte Änderung: 01.10.2026 11:03
#define FW_VERSION "2.0.0"

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
//   display        alles, was gezeichnet wird
//   line_icons.h   Liniensymbole (S/U/Tram als Bitmap, Bus generiert)
//   buttons        Tastenauswertung
//   stats          API-Stoerungen, WLAN-Signalbewertung
//   time_utils     Laufzeit, Zeitformate
//   text_utils     UTF-8 -> Latin-1 fuer echte Umlaute auf dem Display
//   wifi_diag      Ursache von WLAN-Abbruechen fuer den Fehlerbildschirm
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
#include "src/wifi_diag.h"
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

// Angezeigte Richtung: true = Zentrum, false = Auswaerts (nur bei
// getrennter Anzeige). Start und Auto-Reset jeweils auf die Standardansicht
// (appSettings.defaultViewZentrum, gesetzt in setup()).
bool showZentrum = true;
// Seite 2 (Abfahrt 5-8) aktiv - nur bei gemischter Anzeige
bool showPage2 = false;
String stationName = "Bahnhof";

// Zwischenspeicher der zuletzt abgerufenen Abfahrten. Umschalten, Blaettern
// und Auto-Reset zeichnen nur daraus neu - abgerufen wird nur beim
// Minuten-Update (plus Start, WLAN-Wiederkehr und Retries bei Stoerung).
// Getrennte Anzeige: je Richtung ein Speicher. Gemischte Anzeige: cacheAll.
Departure cacheZentrum[MAX_DEPARTURES_SHOWN];
Departure cacheAuswaerts[MAX_DEPARTURES_SHOWN];
int cacheZentrumCount = 0;
int cacheAuswaertsCount = 0;
Departure cacheAll[MAX_DEPARTURES_SHOWN * 2];   // Seite 1 + Seite 2
int cacheAllCount = 0;

// Statistik fuer den Log-Screen (API-Stoerungen: siehe stats.cpp)
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
  showZentrum = appSettings.defaultViewZentrum;

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
  wifiDiagInit();   // vor WiFi.begin(): merkt sich die Gruende von Abbruechen
  if (settingsHasWifi()) {
    Serial.print("Verbinde mit ");
    Serial.println(appSettings.wifiSsid);
    WiFi.begin(appSettings.wifiSsid.c_str(), appSettings.wifiPassword.c_str());
  } else {
    Serial.println("Keine WLAN-Daten - Einrichtung per Web-Installer (WLAN verbinden)");
  }

  // Klappt die Anmeldung nicht innerhalb von WIFI_ERROR_SCREEN_DELAY_MS,
  // erscheint der Fehlerbildschirm mit der vermuteten Ursache (fruehestens
  // nach dem Startbildschirm; ohne WLAN-Daten sofort danach). Neuer
  // Versuch alle ERROR_RETRY_INTERVAL_MS - nach einem Anmeldefehler
  // (z.B. falsches Passwort) versucht es der ESP32 sonst nicht erneut.
  // Neue WLAN-Daten per Improv verbinden das Board direkt, die Schleife
  // endet dann von selbst.
  unsigned long wifiStart = millis();
  unsigned long lastWifiRetry = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (improvLoop()) portalRequested = true;
    extrasStatus(true);
    delay(300);
    Serial.print(".");
    bool errorDue = !settingsHasWifi() || millis() - wifiStart >= WIFI_ERROR_SCREEN_DELAY_MS;
    if (errorDue && !splashShowing()) {
      showWifiErrorIfChanged();
    }
    if (settingsHasWifi() && !improvBusy() &&
        millis() - lastWifiRetry >= ERROR_RETRY_INTERVAL_MS) {
      WiFi.reconnect();
      lastWifiRetry = millis();
    }
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
  if (settingsHasStation()) {
    fetchStationName(appSettings.stationId.c_str(), stationName);
  }

  extrasSetup(firmwareVersionText().c_str());

  // Erste Abfahrten schon waehrend des Startbildschirms laden, gezeichnet
  // wird erst danach (Abfahrten, API-Fehler- oder Einrichtungs-Screen)
  if (settingsHasStation()) fetchDepartures();
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
      currentState = STATE_WIFI_ERROR;
      wifiDisconnectCount++;
      wifiLostSince = millis();
      wifiErrorShownReason = nullptr;
      lastErrorRetry = millis();
    }

    // Fehlerbildschirm erst nach WIFI_ERROR_SCREEN_DELAY_MS - kurze
    // Aussetzer bleiben auf dem Display unsichtbar
    if (millis() - wifiLostSince >= WIFI_ERROR_SCREEN_DELAY_MS) {
      showWifiErrorIfChanged();
    }

    // Neu verbinden alle ERROR_RETRY_INTERVAL_MS - ausser eine Erweiterung
    // uebernimmt das selbst (extrasHandlesWifiReconnect(), siehe extras.h)
    // oder Improv benutzt gerade das WLAN
    if (!extrasHandlesWifiReconnect() && !improvBusy() && settingsHasWifi() &&
        millis() - lastErrorRetry >= ERROR_RETRY_INTERVAL_MS) {
      Serial.println("Versuche WLAN-Reconnect...");
      WiFi.reconnect();
      lastErrorRetry = millis();
    }
    extrasStatus(true);

    delay(50);
    return;
  }

  if (currentState == STATE_WIFI_ERROR) {
    Serial.println("WLAN wiederhergestellt!");
    currentState = STATE_NORMAL;
    setupScreenShown = false;   // IP-Adresse kann sich geaendert haben
    if (settingsHasStation()) attemptUpdate(true);
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

  // --- QR-Taste (GPIO21): Kurz-Druck = WLAN-QR, Lang-Druck (>=3s) = Log-Screen ---
  handleQrButton(triggerQrAction, triggerLogAction);

  // --- Log-Screen aktiv? ---
  // Differenzform (millis() - start >= dauer) ist ueberlaufsicher
  if (logModeActive) {
    if (millis() - logModeStart >= LOG_DISPLAY_DURATION_MS) {
      Serial.println("Log-Anzeige beendet (Timeout)");
      logModeActive = false;
      returnToDepartures();
    } else {
      return;
    }
  }

  // --- QR-Screen aktiv? ---
  if (qrModeActive) {
    if (millis() - qrModeStart >= QR_DISPLAY_DURATION_MS) {
      Serial.println("QR-Anzeige beendet (Timeout), zurueck zu Abfahrten");
      qrModeActive = false;
      returnToDepartures();
    } else {
      return;
    }
  }

  // --- BOOT-Taste: Richtung umschalten bzw. blaettern ---
  handleBootButton(triggerBootAction);

  // --- Auto-Reset: zurueck zur Standardansicht (Richtung bzw. Seite 1) ---
  const unsigned long viewResetMs =
      appSettings.directionView ? DIRECTION_AUTO_RESET_MS : PAGE_AUTO_RESET_MS;
  if (autoResetPending && millis() - autoResetStart >= viewResetMs) {
    resetViewToDefault();
    autoResetPending = false;

    redrawCurrentView();   // kein Abruf, nur aus dem Zwischenspeicher
    updateCounter++;
  }

  if (currentState == STATE_API_ERROR) {
    if (millis() - lastErrorRetry >= ERROR_RETRY_INTERVAL_MS) {
      Serial.println("Erneuter API-Versuch nach Fehler...");
      attemptUpdate(true);
      lastErrorRetry = millis();
    }
    return;
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

    bool doFullRefresh = (updateCounter >= FULL_REFRESH_EVERY);
    if (doFullRefresh) {
      updateCounter = 0;
    }

    Serial.print("Update #");
    Serial.print(updateCounter);
    Serial.println(doFullRefresh ? " (Full Refresh)" : " (Fast Mode)");

    attemptUpdate(doFullRefresh);
  }
}

// ------------------------------------------------------------
// Update-Steuerung
// ------------------------------------------------------------
// Einziger Ort, an dem Abfahrten von der MVG-API abgerufen werden
// (fetchDepartures). Laedt die Rohdaten einmal, wertet sie fuer alle
// Ansichten aus (Zwischenspeicher) und zeichnet dann die aktuelle Ansicht.
void attemptUpdate(bool preferFullRefresh) {
  bool wasApiError = (currentState == STATE_API_ERROR);

  if (!fetchDepartures()) {
    // Fehlerbildschirm nur beim Wechsel normal -> Fehler, nicht bei jedem
    // Retry waehrend einer laufenden Stoerung
    if (!wasApiError) displayShowApiError();
    return;
  }
  redrawFromCache(preferFullRefresh);
}

// Laedt die Abfahrten und fuellt den Zwischenspeicher, ohne zu zeichnen.
// Setzt currentState (STATE_NORMAL bzw. STATE_API_ERROR). true bei Erfolg.
// Ein misslungener Versuch wird nach API_RETRY_DELAY_MS einmal still
// wiederholt; bis dahin bleibt die bisherige Anzeige stehen.
bool fetchDepartures() {
  bool success = fetchDeparturesOnce();
  // Nur aus dem Normalbetrieb heraus; waehrend einer laufenden Stoerung
  // versucht es loop() ohnehin alle ERROR_RETRY_INTERVAL_MS erneut
  if (!success && currentState != STATE_API_ERROR && WiFi.status() == WL_CONNECTED) {
    Serial.println("Abruf fehlgeschlagen - zweiter Versuch");
    delay(API_RETRY_DELAY_MS);
    success = fetchDeparturesOnce();
  }

  if (!success) {
    // Nur beim Wechsel von normal -> Fehler zaehlen (neue Stoerung),
    // nicht bei jedem einzelnen Retry-Versuch waehrend einer laufenden Stoerung
    if (currentState != STATE_API_ERROR) {
      recordApiFail();
      currentState = STATE_API_ERROR;
      lastErrorRetry = millis();
    }
    return false;
  }

  currentState = STATE_NORMAL;
  return true;
}

// Ein Abrufversuch: Rohdaten laden und fuer alle Ansichten auswerten.
// Aendert den Zwischenspeicher nur bei Erfolg. true bei Erfolg.
bool fetchDeparturesOnce() {
  String payload;
  bool success = downloadDepartures(appSettings.stationId.c_str(), payload);

  if (success && appSettings.directionView) {
    // Beide Richtungen aus denselben Rohdaten, damit das Umschalten ohne
    // neuen Abruf auskommt. zentrumIsH legt fest, welcher API-Marker
    // (":H:"/":R:") Richtung Zentrum faehrt.
    bool zentrumIsH = appSettings.zentrumIsH;
    DirectionFilter zentrumFilter = zentrumIsH ? DIR_FILTER_H : DIR_FILTER_R;
    DirectionFilter auswaertsFilter = zentrumIsH ? DIR_FILTER_R : DIR_FILTER_H;
    int countZ = parseDepartures(payload, zentrumFilter, cacheZentrum, MAX_DEPARTURES_SHOWN);
    int countA = parseDepartures(payload, auswaertsFilter, cacheAuswaerts, MAX_DEPARTURES_SHOWN);
    success = (countZ >= 0 && countA >= 0);
    if (success) {
      cacheZentrumCount = countZ;
      cacheAuswaertsCount = countA;
    }
  } else if (success) {
    // Alle Richtungen gemischt: 8 Abfahrten fuer Seite 1 (1-4) und Seite 2 (5-8)
    int count = parseDepartures(payload, DIR_FILTER_ALL, cacheAll, MAX_DEPARTURES_SHOWN * 2);
    success = (count >= 0);
    if (success) {
      cacheAllCount = count;
    }
  }
  return success;
}

// Zeichnet die aktuelle Ansicht (Richtung bzw. Seite) aus dem
// Zwischenspeicher - ohne Netzwerkzugriff.
void redrawFromCache(bool fullRefresh) {
  if (appSettings.directionView) {
    if (showZentrum) {
      displayShowDepartures(cacheZentrum, cacheZentrumCount, stationName,
                            true, true, false, fullRefresh);
    } else {
      displayShowDepartures(cacheAuswaerts, cacheAuswaertsCount, stationName,
                            true, false, false, fullRefresh);
    }
    return;
  }

  int offset = showPage2 ? MAX_DEPARTURES_SHOWN : 0;
  int pageCount = cacheAllCount - offset;
  if (pageCount < 0) pageCount = 0;
  if (pageCount > MAX_DEPARTURES_SHOWN) pageCount = MAX_DEPARTURES_SHOWN;
  displayShowDepartures(cacheAll + offset, pageCount, stationName,
                        false, false, showPage2, fullRefresh);
}

// Nach Umschalten/Blaettern/Auto-Reset: neu zeichnen ohne Abruf. Bei einer
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
  showZentrum = appSettings.defaultViewZentrum;
  showPage2 = false;
  setupScreenShown = false;
  if (!settingsHasStation()) return;

  stationName = "Bahnhof";
  fetchStationName(appSettings.stationId.c_str(), stationName);

  time_t nowRaw = time(nullptr);
  struct tm nowInfo;
  localtime_r(&nowRaw, &nowInfo);
  lastUpdateMinute = nowInfo.tm_min;
  currentState = STATE_NORMAL;   // damit ein Fehler den API-Fehlerbildschirm zeigt
  attemptUpdate(true);
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
  time_t nowRaw = time(nullptr);
  struct tm nowInfo;
  localtime_r(&nowRaw, &nowInfo);

  if (currentState == STATE_API_ERROR || nowInfo.tm_min != lastUpdateMinute) {
    lastUpdateMinute = nowInfo.tm_min;
    attemptUpdate(true);
  } else {
    redrawFromCache(true);
  }
  updateCounter = 0;
}

// ------------------------------------------------------------
// Tasten-Aktionen
// ------------------------------------------------------------
// Werden von handleBootButton()/handleQrButton() (buttons.cpp) aufgerufen

void triggerBootAction() {
  bool leftDefaultView;
  if (appSettings.directionView) {
    // Richtung umschalten (Zentrum <-> Auswaerts)
    showZentrum = !showZentrum;
    Serial.print("Richtung umgeschaltet: ");
    Serial.println(showZentrum ? LABEL_ZENTRUM : LABEL_AUSWAERTS);
    leftDefaultView = (showZentrum != appSettings.defaultViewZentrum);
  } else {
    // Blaettern: Seite 1 <-> Seite 2 (Abfahrt 5-8)
    showPage2 = !showPage2;
    Serial.println(showPage2 ? "Seite 2 (Abfahrt 5-8)" : "Seite 1");
    leftDefaultView = showPage2;
  }

  // Weicht die Ansicht von der Standardansicht ab, laeuft der Auto-Reset
  if (leftDefaultView) {
    autoResetPending = true;
    autoResetStart = millis();
  } else {
    autoResetPending = false;
  }

  redrawCurrentView();   // kein Abruf, nur aus dem Zwischenspeicher
  updateCounter++;
}

// Zurueck zur Standardansicht (vom Auto-Reset in der Loop aufgerufen)
void resetViewToDefault() {
  if (appSettings.directionView) {
    showZentrum = appSettings.defaultViewZentrum;
    Serial.print("Auto-Reset ausgeloest: zurueck zu ");
    Serial.println(showZentrum ? LABEL_ZENTRUM : LABEL_AUSWAERTS);
  } else {
    showPage2 = false;
    Serial.println("Auto-Reset ausgeloest: zurueck zu Seite 1");
  }
}

void triggerQrAction() {
  // WLAN-QR ausgeschaltet: kurzer Druck auf die QR-Taste ohne Funktion
  if (!appSettings.wifiQr) return;

  if (qrModeActive) {
    Serial.println("QR-Anzeige manuell beendet");
    qrModeActive = false;
    returnToDepartures();
  } else {
    Serial.println("WLAN-QR-Code wird angezeigt (60s)");
    qrModeActive = true;
    qrModeStart = millis();
    displayShowWifiQr(appSettings.qrTitle.c_str(), appSettings.qrSsid.c_str(),
                      appSettings.qrPassword.c_str());
  }
}

void triggerLogAction() {
  if (logModeActive) {
    Serial.println("Log-Anzeige manuell beendet (langer Druck)");
    logModeActive = false;
    returnToDepartures();
  } else {
    Serial.println("Log-Anzeige aktiviert (langer Druck, 60s), Portal offen");
    logModeActive = true;
    logModeStart = millis();
    portalOpen();   // Laufzeit startet bei jedem Oeffnen neu
    String address = WiFi.localIP().toString();
    displayShowLog(firmwareVersionText().c_str(), address.c_str(),
                   portalClosingTime().c_str(), wifiDisconnectCount, getApiFailCount());
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
