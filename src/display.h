// display.h
// Alles, was auf das E-Ink-Display gezeichnet wird. Einzige Datei, die die
// Display-Library (heltec-eink-modules) einbindet.
#pragma once
#include <Arduino.h>
#include "mvg_api.h"

// Display initialisieren (einmalig in setup())
void displayInit();

// Startbildschirm mit Projektname und Firmware-Version
void displayShowSplash(const char* firmwareVersion);

// Abfahrtsansicht zeichnen.
// showDirection = true: Header "Station -> Zentrum/Auswaerts" (showZentrum
//   waehlt die Richtung, ggf. Kurzform); false: Header nur Stationsname,
//   bei isPage2 mit Seitenhinweis "(2/2)".
// fullRefresh = true: sauberer Full Refresh, false: Fast Mode (schneller,
//   doppeltes Update gegen Grauschleier).
void displayShowDepartures(const Departure departures[], int found,
                           const String& stationName, bool showDirection,
                           bool showZentrum, bool isPage2, bool fullRefresh);

// Fehlerbildschirme
// WLAN-Fehler mit vermuteter Ursache und optionalem Hinweis ("" = keiner),
// Texte siehe wifi_diag.h
void displayShowWifiError(const char* ssid, const char* reason, const char* hint);
void displayShowApiError();

// QR-Code + Zugangsdaten fuer ein WLAN (z.B. Gast-WLAN)
// (nur vorhanden, wenn FEATURE_WIFI_QR in config.h auf 1 steht)
void displayShowWifiQr(const char* qrWlanSsid, const char* qrWlanPassword);

// System-Log (Momentaufnahme beim Aufruf)
void displayShowLog(const char* firmwareVersion, uint64_t wifiConnectedSince,
                    int wifiDisconnects, int apiFails);
