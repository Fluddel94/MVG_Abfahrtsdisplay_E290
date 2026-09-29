// Version: 1.0.0
// Letzte Änderung: 29.09.2026 16:01
#define FW_VERSION "1.0.0"

// ===== BLOCK 01: KONFIGURATION START =====
#pragma region Block 1 - Konfiguration
// Zugangsdaten (WLAN, optional QR-WLAN und OTA) liegen in secrets.h
// (Vorlage mit Platzhaltern: secrets_example.h).
// Geraeteeinstellungen (Station, Richtungen, Verkehrsmittel, Zusatz-
// funktionen) liegen in config.h - dort der Reihe nach durchgehen.
#if !__has_include("secrets.h")
#error "secrets.h fehlt: secrets_example.h kopieren, in secrets.h umbenennen und WLAN-Daten eintragen (siehe README)"
#endif
#include "secrets.h"
#pragma endregion
// ===== BLOCK 01: KONFIGURATION ENDE =====

// ===== BLOCK 02: UEBERSICHT START =====
#pragma region Block 2 - Uebersicht
// E-Ink-Abfahrtsdisplay - Heltec Vision Master E290 (MVG, Muenchen)
//
// Funktionen, Einrichtung, Bedienung und wichtige Hinweise: README.md
// Aenderungen je Version: CHANGELOG.md
// Lizenz: GPL-3.0-or-later (siehe LICENSE)
//
// Dateien (Hauptordner = alles, was pro Geraet angepasst wird):
//   dieses .ino    Ablaufsteuerung (setup/loop, Tasten-Aktionen, WLAN-Fehler)
//   config.h       Geraete-Einstellungen + gemeinsame Konstanten
//   secrets.h      Zugangsdaten (nie teilen; Vorlage: secrets_example.h)
// Programmcode in src/ (Arduino-IDE kompiliert nur einen Ordner namens src):
//   mvg_api        Abruf/Auswertung der MVG-API
//   display        alles, was gezeichnet wird
//   line_icons.h   Liniensymbole (S/U/Tram als Bitmap, Bus generiert)
//   buttons        Tastenauswertung
//   stats          API-Stoerungen, WLAN-Signalbewertung
//   time_utils     Laufzeit, Zeitformate
//   text_utils     UTF-8 -> Latin-1 fuer echte Umlaute auf dem Display
//   wifi_diag      Ursache von WLAN-Abbruechen fuer den Fehlerbildschirm
//   extras         Andockstellen fuer optionale Erweiterungen (Block 10)
#pragma endregion
// ===== BLOCK 02: UEBERSICHT ENDE =====

// ===== BLOCK 03: INCLUDES START =====
#pragma region Block 3 - Includes
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
#if FEATURE_OTA
#include <ArduinoOTA.h>
#endif
#include "src/mvg_api.h"
#include "src/display.h"
#include "src/buttons.h"
#include "src/stats.h"
#include "src/time_utils.h"
#include "src/wifi_diag.h"
#include "src/extras.h"
#pragma endregion
// ===== BLOCK 03: INCLUDES ENDE =====

// ===== BLOCK 04: GLOBALE ZUSTANDSVARIABLEN START =====
#pragma region Block 4 - Globale Zustandsvariablen
unsigned long lastErrorRetry = 0;
unsigned long autoResetStart = 0;
unsigned long qrModeStart = 0;
unsigned long logModeStart = 0;
bool autoResetPending = false;
bool qrModeActive = false;
bool logModeActive = false;
int updateCounter = 0;
int lastUpdateMinute = -1;

// Angezeigte Richtung: true = Zentrum, false = Auswaerts.
// Start und Auto-Reset jeweils auf DEFAULT_VIEW_ZENTRUM (config.h)
// (nur bei FEATURE_DIRECTION_VIEW 1)
bool showZentrum = DEFAULT_VIEW_ZENTRUM;
// Seite 2 (Abfahrt 5-8) aktiv - nur bei FEATURE_DIRECTION_VIEW 0
bool showPage2 = false;
String stationName = "Bahnhof";

// Zwischenspeicher der zuletzt abgerufenen Abfahrten. Umschalten, Blaettern
// und Auto-Reset zeichnen nur daraus neu - abgerufen wird nur beim
// Minuten-Update (plus Start, WLAN-Wiederkehr und Retries bei Stoerung).
#if FEATURE_DIRECTION_VIEW
Departure cacheZentrum[MAX_DEPARTURES_SHOWN];
Departure cacheAuswaerts[MAX_DEPARTURES_SHOWN];
int cacheZentrumCount = 0;
int cacheAuswaertsCount = 0;
#else
Departure cacheAll[MAX_DEPARTURES_SHOWN * 2];   // Seite 1 + Seite 2
int cacheAllCount = 0;
#endif

// Statistik fuer den Log-Screen (API-Stoerungen: siehe stats.cpp)
// wifiConnectedSince als 64-Bit-Wert (Quelle: uptimeMs()), damit die
// WLAN-Laufzeit nicht nach ~49,7 Tagen ueberlaeuft
uint64_t wifiConnectedSince = 0;
int wifiDisconnectCount = 0;

// WLAN-Fehlerbildschirm: seit wann keine Verbindung besteht und welche
// Ursache zuletzt angezeigt wurde (nullptr = noch kein Fehlerbildschirm)
unsigned long wifiLostSince = 0;
const char* wifiErrorShownReason = nullptr;

enum SystemState { STATE_NORMAL, STATE_WIFI_ERROR, STATE_API_ERROR };
SystemState currentState = STATE_NORMAL;
#pragma endregion
// ===== BLOCK 04: GLOBALE ZUSTANDSVARIABLEN ENDE =====

// ===== BLOCK 05: SETUP START =====
#pragma region Block 5 - Setup
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print("Abfahrtsdisplay Firmware v");
  Serial.println(firmwareVersionText());

  extrasBegin();
  buttonsInit();
  displayInit();

  Serial.print("Verbinde mit ");
  Serial.println(ssid);

  wifiDiagInit();   // vor WiFi.begin(): merkt sich die Gruende von Abbruechen
  WiFi.begin(ssid, password);

  // Klappt die Anmeldung nicht innerhalb von WIFI_ERROR_SCREEN_DELAY_MS,
  // erscheint der Fehlerbildschirm mit der vermuteten Ursache. Neuer
  // Versuch alle ERROR_RETRY_INTERVAL_MS - nach einem Anmeldefehler
  // (z.B. falsches Passwort) versucht es der ESP32 sonst nicht erneut.
  unsigned long wifiStart = millis();
  unsigned long lastWifiRetry = millis();
  while (WiFi.status() != WL_CONNECTED) {
    extrasStatus(true);
    delay(300);
    Serial.print(".");
    if (millis() - wifiStart >= WIFI_ERROR_SCREEN_DELAY_MS) {
      showWifiErrorIfChanged();
    }
    if (millis() - lastWifiRetry >= ERROR_RETRY_INTERVAL_MS) {
      WiFi.reconnect();
      lastWifiRetry = millis();
    }
  }

  Serial.println("");
  Serial.println("WLAN verbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  wifiConnectedSince = uptimeMs();

#if FEATURE_OTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(otaPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA-Update gestartet...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA-Update abgeschlossen!");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA-Fehler [");
    Serial.print(error);
    Serial.println("]");
  });

  ArduinoOTA.begin();
  Serial.println("OTA bereit.");
#endif

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
  fetchStationName(STATION_GLOBAL_ID, stationName);

  extrasSetup(firmwareVersionText().c_str());

  attemptUpdate(true);
  lastUpdateMinute = timeinfo.tm_min;
}
#pragma endregion
// ===== BLOCK 05: SETUP ENDE =====

// ===== BLOCK 06: LOOP START =====
#pragma region Block 6 - Loop
void loop() {
#if FEATURE_OTA
  ArduinoOTA.handle();
#endif
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
    // uebernimmt das selbst (extrasHandlesWifiReconnect(), Block 10)
    if (!extrasHandlesWifiReconnect() &&
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
    wifiConnectedSince = uptimeMs();
    attemptUpdate(true);
    updateCounter = 0;
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
#if FEATURE_DIRECTION_VIEW
  const unsigned long viewResetMs = DIRECTION_AUTO_RESET_MS;
#else
  const unsigned long viewResetMs = PAGE_AUTO_RESET_MS;
#endif
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
#pragma endregion
// ===== BLOCK 06: LOOP ENDE =====

// ===== BLOCK 07: UPDATE-STEUERUNG START =====
#pragma region Block 7 - Update-Steuerung
// Einziger Ort, an dem Abfahrten von der MVG-API abgerufen werden. Laedt die
// Rohdaten einmal, wertet sie fuer alle Ansichten aus (Zwischenspeicher) und
// zeichnet dann die aktuelle Ansicht.
void attemptUpdate(bool preferFullRefresh) {
  String payload;
  bool success = downloadDepartures(STATION_GLOBAL_ID, payload);

  if (success) {
#if FEATURE_DIRECTION_VIEW
    // Beide Richtungen aus denselben Rohdaten, damit das Umschalten ohne
    // neuen Abruf auskommt. ZENTRUM_IS_H legt fest, welcher API-Marker
    // (":H:"/":R:") Richtung Zentrum faehrt.
    DirectionFilter zentrumFilter = ZENTRUM_IS_H ? DIR_FILTER_H : DIR_FILTER_R;
    DirectionFilter auswaertsFilter = ZENTRUM_IS_H ? DIR_FILTER_R : DIR_FILTER_H;
    int countZ = parseDepartures(payload, zentrumFilter, cacheZentrum, MAX_DEPARTURES_SHOWN);
    int countA = parseDepartures(payload, auswaertsFilter, cacheAuswaerts, MAX_DEPARTURES_SHOWN);
    success = (countZ >= 0 && countA >= 0);
    if (success) {
      cacheZentrumCount = countZ;
      cacheAuswaertsCount = countA;
    }
#else
    // Alle Richtungen gemischt: 8 Abfahrten fuer Seite 1 (1-4) und Seite 2 (5-8)
    int count = parseDepartures(payload, DIR_FILTER_ALL, cacheAll, MAX_DEPARTURES_SHOWN * 2);
    success = (count >= 0);
    if (success) {
      cacheAllCount = count;
    }
#endif
  }

  if (!success) {
    // Nur beim Wechsel von normal -> Fehler zaehlen (neue Stoerung),
    // nicht bei jedem einzelnen Retry-Versuch waehrend einer laufenden Stoerung
    if (currentState != STATE_API_ERROR) {
      recordApiFail();
      currentState = STATE_API_ERROR;
      displayShowApiError();
      lastErrorRetry = millis();
    }
    return;
  }

  currentState = STATE_NORMAL;
  redrawFromCache(preferFullRefresh);
}

// Zeichnet die aktuelle Ansicht (Richtung bzw. Seite) aus dem
// Zwischenspeicher - ohne Netzwerkzugriff.
void redrawFromCache(bool fullRefresh) {
#if FEATURE_DIRECTION_VIEW
  if (showZentrum) {
    displayShowDepartures(cacheZentrum, cacheZentrumCount, stationName,
                          true, true, false, fullRefresh);
  } else {
    displayShowDepartures(cacheAuswaerts, cacheAuswaertsCount, stationName,
                          true, false, false, fullRefresh);
  }
#else
  int offset = showPage2 ? MAX_DEPARTURES_SHOWN : 0;
  int pageCount = cacheAllCount - offset;
  if (pageCount < 0) pageCount = 0;
  if (pageCount > MAX_DEPARTURES_SHOWN) pageCount = MAX_DEPARTURES_SHOWN;
  displayShowDepartures(cacheAll + offset, pageCount, stationName,
                        false, false, showPage2, fullRefresh);
#endif
}

// Nach Umschalten/Blaettern/Auto-Reset: neu zeichnen ohne Abruf. Bei einer
// laufenden API-Stoerung bleibt der Fehlerbildschirm stehen (die Ansicht
// wird beim naechsten erfolgreichen Abruf gezeichnet).
void redrawCurrentView() {
  if (currentState == STATE_API_ERROR) return;
  redrawFromCache(true);
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
#pragma endregion
// ===== BLOCK 07: UPDATE-STEUERUNG ENDE =====

// ===== BLOCK 08: TASTEN-AKTIONEN START =====
#pragma region Block 8 - Tasten-Aktionen
// Werden von handleBootButton()/handleQrButton() (buttons.cpp) aufgerufen

void triggerBootAction() {
#if FEATURE_DIRECTION_VIEW
  // Richtung umschalten (Zentrum <-> Auswaerts)
  showZentrum = !showZentrum;
  Serial.print("Richtung umgeschaltet: ");
  Serial.println(showZentrum ? LABEL_ZENTRUM : LABEL_AUSWAERTS);
  bool leftDefaultView = (showZentrum != (bool)DEFAULT_VIEW_ZENTRUM);
#else
  // Blaettern: Seite 1 <-> Seite 2 (Abfahrt 5-8)
  showPage2 = !showPage2;
  Serial.println(showPage2 ? "Seite 2 (Abfahrt 5-8)" : "Seite 1");
  bool leftDefaultView = showPage2;
#endif

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
#if FEATURE_DIRECTION_VIEW
  showZentrum = DEFAULT_VIEW_ZENTRUM;
  Serial.print("Auto-Reset ausgeloest: zurueck zu ");
  Serial.println(showZentrum ? LABEL_ZENTRUM : LABEL_AUSWAERTS);
#else
  showPage2 = false;
  Serial.println("Auto-Reset ausgeloest: zurueck zu Seite 1");
#endif
}

void triggerQrAction() {
#if FEATURE_WIFI_QR
  if (qrModeActive) {
    Serial.println("QR-Anzeige manuell beendet");
    qrModeActive = false;
    returnToDepartures();
  } else {
    Serial.println("WLAN-QR-Code wird angezeigt (60s)");
    qrModeActive = true;
    qrModeStart = millis();
    displayShowWifiQr(qrWlanSsid, qrWlanPassword);
  }
#else
  // WLAN-QR deaktiviert (FEATURE_WIFI_QR = 0 in config.h):
  // kurzer Druck auf die QR-Taste hat keine Funktion
#endif
}

void triggerLogAction() {
  if (logModeActive) {
    Serial.println("Log-Anzeige manuell beendet (langer Druck)");
    logModeActive = false;
    returnToDepartures();
  } else {
    Serial.println("Log-Anzeige aktiviert (langer Druck, 60s)");
    logModeActive = true;
    logModeStart = millis();
    displayShowLog(firmwareVersionText().c_str(), wifiConnectedSince, wifiDisconnectCount, getApiFailCount());
  }
}
#pragma endregion
// ===== BLOCK 08: TASTEN-AKTIONEN ENDE =====

// ===== BLOCK 09: WLAN-FEHLERANZEIGE START =====
#pragma region Block 9 - WLAN-Fehleranzeige
// Zeichnet den WLAN-Fehlerbildschirm, wenn sich die vermutete Ursache
// (wifi_diag.cpp) seit der letzten Anzeige geaendert hat - sonst nichts,
// damit das E-Ink-Display nicht bei jedem Loop-Durchlauf neu zeichnet.
// Aufruf aus setup() (Erstverbindung) und loop() (Abbruch im Betrieb).
void showWifiErrorIfChanged() {
  const char* reason = wifiDiagReasonText();
  if (reason == wifiErrorShownReason) return;
  wifiErrorShownReason = reason;

  Serial.println();
  Serial.print("WLAN-Fehlerbildschirm: ");
  Serial.print(reason);
  Serial.print(" (Reason-Code ");
  Serial.print(wifiDiagLastReason());
  Serial.println(")");

  displayShowWifiError(ssid, reason, wifiDiagHintText());
}
#pragma endregion
// ===== BLOCK 09: WLAN-FEHLERANZEIGE ENDE =====

// ===== BLOCK 10: ERWEITERUNGEN START =====
#pragma region Block 10 - Erweiterungen
// Optionale Erweiterungen docken ueber src/extras.h an (extrasBegin,
// extrasSetup, extrasLoop, extrasStatus, extrasHandlesWifiReconnect,
// extrasVersionSuffix). Ohne Erweiterung sind das leere Funktionen.

// Versionsnummer inkl. Zusatz einer Erweiterung (z.B. "1.0.0-variante")
String firmwareVersionText() {
  return String(FW_VERSION) + extrasVersionSuffix();
}
#pragma endregion
// ===== BLOCK 10: ERWEITERUNGEN ENDE =====
