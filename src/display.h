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
// dirLabel: Richtung im Header "Station -> ..." - 'Z' Zentrum, 'A'
//   Auswaerts (ggf. Kurzform), 'H'/'R' die Kennung, 0 = nur Stationsname.
// isPage2: Seitenhinweis "(2/2)" hinter dem Header.
// minutes: Abfahrtszeit als Minuten bis zur Abfahrt ("4 min", "jetzt"),
//   ab 60 Minuten Uhrzeit; false: Uhrzeit mit Verspaetung.
// incidents: Warndreieck bei Stoerung/Ausfall (links vom Trennstrich);
//   false: keins, ausgefallene Fahrten nur durchgestrichen.
// stationNumber = 1 oder 2: Nummer im schwarzen Kaestchen vor dem Namen
//   (nur bei zwei Stationen), 0 = ohne.
// fullRefresh = true: sauberer Full Refresh, false: Fast Mode (schneller,
//   doppeltes Update gegen Grauschleier).
void displayShowDepartures(const Departure departures[], int found,
                           const String& stationName, char dirLabel,
                           bool isPage2, bool minutes, bool incidents,
                           bool fullRefresh, int stationNumber = 0);

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

// Einstellungen/Info (Momentaufnahme beim Aufruf): QR-Code zum Portal,
// Version, IP-Adresse, WLAN-Name, Signal, Laufzeit. portalAddress: IP-
// Adresse ohne "http://", portalUntil: Uhrzeit "HH:MM", zu der das Portal
// schliesst ("" = unbekannt).
void displayShowInfo(const char* firmwareVersion, const char* portalAddress,
                     const char* portalUntil);

// Firmware-Upload im Portal laeuft
void displayShowUpdate();

// Ersteinrichtung (noch keine Station): QR-Code mit der Portal-Adresse plus
// Adresse im Klartext (ohne "http://")
void displayShowPortalSetup(const char* url, const char* address);
