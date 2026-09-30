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
// Texte siehe wifi_diag.h. retrying = true: unterste Zeile "Automatischer
// Neuversuch...", false: "Warte auf Einrichtung..." (keine WLAN-Daten).
void displayShowWifiError(const char* ssid, const char* reason, const char* hint,
                          bool retrying);
void displayShowApiError();

// QR-Code + Zugangsdaten fuer ein WLAN (z.B. Gast-WLAN). title,
// qrWlanSsid und qrWlanPassword in UTF-8.
void displayShowWifiQr(const char* title, const char* qrWlanSsid,
                       const char* qrWlanPassword);

// System-Log (Momentaufnahme beim Aufruf). portalAddress: Adresse des
// Einstellungsportals ohne "http://", portalUntil: Uhrzeit "HH:MM", zu der
// es schliesst ("" = unbekannt).
void displayShowLog(const char* firmwareVersion, const char* portalAddress,
                    const char* portalUntil, int wifiDisconnects, int apiFails);

// Ersteinrichtung (noch keine Station): QR-Code mit der Portal-Adresse plus
// Adresse im Klartext (ohne "http://")
void displayShowPortalSetup(const char* url, const char* address);
