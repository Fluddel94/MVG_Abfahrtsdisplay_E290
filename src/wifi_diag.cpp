// wifi_diag.cpp
// WLAN-Diagnose (siehe wifi_diag.h).
//
// Die Reason-Codes stammen aus ESP-IDF (wifi_err_reason_t). Sie werden als
// Zahlen verglichen, damit der Code auch mit Core-Versionen kompiliert, in
// denen einzelne Namen fehlen. Einschraenkung: Ein falsches Passwort meldet
// der ESP32 meist nur als Zeitueberschreitung beim Anmelden (15/204) - das
// kann auch bei sehr schwachem Empfang passieren, daher "Passwort falsch?".

#include <Arduino.h>
#include <WiFi.h>
#include "wifi_diag.h"

// Wird im WLAN-Task geschrieben und in loop() gelesen
static volatile int lastReason = 0;

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastReason = info.wifi_sta_disconnected.reason;
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    lastReason = 0;
  }
}

void wifiDiagInit() {
  WiFi.onEvent(onWifiEvent);
}

static bool isNotFound(int r) {
  // 201 NO_AP_FOUND, 210-212 NO_AP_FOUND_W_COMPATIBLE_SECURITY /
  // _IN_AUTHMODE_THRESHOLD / _IN_RSSI_THRESHOLD
  return r == 201 || (r >= 210 && r <= 212);
}

static bool isAuthProblem(int r) {
  // 2 AUTH_EXPIRE, 14 MIC_FAILURE, 15 4WAY_HANDSHAKE_TIMEOUT,
  // 202 AUTH_FAIL, 204 HANDSHAKE_TIMEOUT
  return r == 2 || r == 14 || r == 15 || r == 202 || r == 204;
}

const char* wifiDiagReasonText() {
  int r = lastReason;
  if (isNotFound(r)) return "Netz nicht gefunden";
  if (isAuthProblem(r)) return "Passwort falsch?";
  if (r == 0) return "Keine Verbindung";
  return "Verbindung unterbrochen";
}

const char* wifiDiagHintText() {
  int r = lastReason;
  // "\xFC" "fen": getrennt, sonst waere "f" Teil der Escape-Sequenz
  if (isNotFound(r)) return "Name und 2,4 GHz pr\xFC" "fen";
  if (isAuthProblem(r)) return "secrets.h pr\xFC" "fen";
  return "";
}

int wifiDiagLastReason() {
  return lastReason;
}
