// wifi_diag.h
// WLAN-Diagnose: merkt sich den Grund des letzten WLAN-Abbruchs (Reason-Code
// des ESP32) und liefert daraus einen kurzen Text fuer den Fehlerbildschirm.
#pragma once

// Einmalig in setup() VOR WiFi.begin() aufrufen (registriert den
// Event-Handler fuer Verbindungsabbrueche).
void wifiDiagInit();

// Vermutete Ursache als Display-Text (Latin-1), z.B. "Passwort falsch?".
// Liefert immer einen der festen Texte - ein Vergleich der Zeiger genuegt,
// um eine Aenderung zu erkennen.
const char* wifiDiagReasonText();

// Hinweis zur Ursache (Latin-1) oder "" wenn es keinen gibt.
const char* wifiDiagHintText();

// Letzter Reason-Code (0 = seit der letzten Verbindung kein Abbruch), fuer
// den seriellen Monitor.
int wifiDiagLastReason();
