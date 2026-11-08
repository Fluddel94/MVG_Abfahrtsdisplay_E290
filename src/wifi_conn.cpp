// wifi_conn.cpp
// WLAN-Verbindung aufbauen und wiederherstellen (siehe wifi_conn.h).

#include <Arduino.h>
#include <WiFi.h>
#include "wifi_conn.h"
#include "settings.h"
#include "debug_log.h"

// Ein Versuch gilt spaetestens nach dieser Zeit als gescheitert (Suche auf
// allen Kanaelen, Anmeldung und IP-Adresse dauern zusammen ca. 3 s). Ein
// haengender Versuch wird so vor dem Fehlerbildschirm beim Start
// (WIFI_ERROR_SCREEN_DELAY_MS, 20 s) neu gestartet.
static const unsigned long ATTEMPT_TIMEOUT_MS = 10000UL;
// Pause vor dem naechsten Versuch, je nach Zahl der Fehlschlaege in Folge
static const unsigned long RETRY_DELAY_MS[] = {2000UL, 5000UL, 15000UL, 30000UL};
static const int RETRY_DELAY_COUNT = sizeof(RETRY_DELAY_MS) / sizeof(RETRY_DELAY_MS[0]);
// Nach so vielen Fehlschlaegen in Folge das WLAN ab- und wieder einschalten
static const int RESTART_WIFI_AFTER = 6;
// Reason-Code ASSOC_LEAVE: eigene Abmeldung (z.B. WiFi.disconnect()),
// beendet keinen laufenden Versuch
static const int REASON_ASSOC_LEAVE = 8;

// Werden im WLAN-Task geschrieben und in loop() gelesen
static volatile bool disconnectSeen = false;
static volatile int lastReason = 0;
static volatile unsigned long waitFrom = 0;
static volatile int failCount = 0;

static bool attemptActive = false;
static unsigned long attemptStart = 0;

// WLAN-Protokoll nur mit DEBUG_LOG 1 (DBG_PRINTF entfaellt sonst ganz)
static void printLink(const char* prefix) {
  DBG_PRINTF("%s%s, AP %s, Kanal %d, Signal %d dBm\n", prefix,
             WiFi.SSID().c_str(), WiFi.BSSIDstr().c_str(), (int)WiFi.channel(),
             (int)WiFi.RSSI());
}

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    int reason = info.wifi_sta_disconnected.reason;
    // Sofort melden (laeuft im WLAN-Task, Serial ist dafuer ausgelegt)
    DBG_PRINTF("WLAN getrennt: Grund %d (%s)\n", reason,
               WiFi.STA.disconnectReasonName((wifi_err_reason_t)reason));
    if (reason == REASON_ASSOC_LEAVE) return;
    lastReason = reason;
    waitFrom = millis();
    disconnectSeen = true;
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    failCount = 0;
    printLink("WLAN verbunden: ");
  }
}

void wifiConnLogStatus() {
  if (WiFi.status() == WL_CONNECTED) printLink("WLAN: ");
}

void wifiConnInit(bool ownReconnect) {
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  // Das automatische Wiederverbinden des ESP32 versucht es sofort und ohne
  // Pause erneut und kommt sich mit eigenen Versuchen in die Quere
  if (ownReconnect) WiFi.setAutoReconnect(false);
  WiFi.onEvent(onWifiEvent);
}

void wifiConnBegin(const char* ssid, const char* password) {
  disconnectSeen = false;
  attemptActive = true;
  attemptStart = millis();
  WiFi.begin(ssid, password);
}

void wifiConnLoop() {
  if (WiFi.status() == WL_CONNECTED) {
    attemptActive = false;
    return;
  }
  unsigned long now = millis();

  if (attemptActive) {
    bool timedOut = now - attemptStart >= ATTEMPT_TIMEOUT_MS;
    if (!disconnectSeen && !timedOut) return;   // Versuch laeuft noch
    attemptActive = false;
    failCount = failCount + 1;
    if (timedOut && !disconnectSeen) waitFrom = now;
    Serial.printf("WLAN: Versuch %d fehlgeschlagen (%s %d)\n", failCount,
                  disconnectSeen ? "Grund" : "Zeitueberschreitung, letzter Grund",
                  lastReason);
  }

  int index = failCount < RETRY_DELAY_COUNT ? failCount : RETRY_DELAY_COUNT - 1;
  if (now - waitFrom < RETRY_DELAY_MS[index]) return;

  String ssid, password;
  {
    SettingsLock lock;
    ssid = appSettings.wifiSsid;
    password = appSettings.wifiPassword;
  }
  if (ssid.length() == 0) return;

  if (failCount > 0 && failCount % RESTART_WIFI_AFTER == 0) {
    Serial.println("WLAN: Neustart des WLAN-Moduls");
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_STA);   // Stromsparmodus-Einstellung bleibt erhalten
  }
  Serial.printf("WLAN: neuer Verbindungsversuch (%d)\n", failCount + 1);
  wifiConnBegin(ssid.c_str(), password.c_str());
}

void restartCleanly() {
  Serial.println("Neustart (WLAN wird vorher abgemeldet)");
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(200);   // Abmeldung noch senden
  }
  ESP.restart();
}
