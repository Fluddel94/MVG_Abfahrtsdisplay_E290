// stats.cpp
// Betriebsstatistik (siehe stats.h).

#include <Arduino.h>
#include "../config.h"
#include "time_utils.h"
#include "stats.h"

// Ringpuffer mit den Zeitpunkten (uptimeMs) der letzten API-Stoerungen
static uint64_t apiFailTimes[API_FAIL_HISTORY_SIZE];
static int apiFailHistoryCount = 0;   // Anzahl gueltiger Eintraege
static int apiFailHistoryNext = 0;    // naechste Schreibposition
static int apiFailCount = 0;          // Stoerungen der letzten 24h

// Speichert den Zeitpunkt einer neuen API-Stoerung im Ringpuffer.
// Ist der Puffer voll, wird der aelteste Eintrag ueberschrieben.
void recordApiFail() {
  apiFailTimes[apiFailHistoryNext] = uptimeMs();
  apiFailHistoryNext = (apiFailHistoryNext + 1) % API_FAIL_HISTORY_SIZE;
  if (apiFailHistoryCount < API_FAIL_HISTORY_SIZE) {
    apiFailHistoryCount++;
  }
  checkApiFailWindow();
}

// Gleitendes 24h-Fenster: zaehlt alle gespeicherten Stoerungen, die
// juenger als API_FAIL_WINDOW_MS sind. Max. 50 Vergleiche pro Aufruf,
// vernachlaessigbar. Basis ist uptimeMs() (64 Bit), daher ueberlaufsicher.
void checkApiFailWindow() {
  uint64_t now = uptimeMs();
  int count = 0;
  for (int i = 0; i < apiFailHistoryCount; i++) {
    if (now - apiFailTimes[i] < API_FAIL_WINDOW_MS) {
      count++;
    }
  }
  apiFailCount = count;
}

int getApiFailCount() {
  return apiFailCount;
}

// Faustregeln, keine Norm - Einzelwerte schwanken um ca. +/-3-5 dB
const char* wifiQualityText(int rssi) {
  if (rssi >= -60) return "sehr gut";
  if (rssi >= -70) return "gut";
  if (rssi >= -80) return "m\xE4\xDFig";   // Latin-1 fuer die Display-Schrift
  return "schwach";
}

