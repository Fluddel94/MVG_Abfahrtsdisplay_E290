// stats.h
// Betriebsstatistik: API-Stoerungen (gleitendes 24h-Fenster) und
// WLAN-Signalbewertung fuer den Log-Screen.
#pragma once
#include <Arduino.h>

// Neue API-Stoerung vermerken (nur beim Wechsel normal -> Fehler aufrufen,
// nicht bei jedem Retry)
void recordApiFail();

// Zaehlt die Stoerungen der letzten 24h neu. In jedem loop()-Durchlauf
// aufrufen, damit alte Stoerungen aus dem Fenster herausfallen.
void checkApiFailWindow();

// Anzahl API-Stoerungen der letzten 24h (Stand des letzten Aufrufs von
// checkApiFailWindow bzw. recordApiFail)
int getApiFailCount();

// Grobe Bewertung der WLAN-Signalstaerke ("sehr gut" ... "schwach")
const char* wifiQualityText(int rssi);

