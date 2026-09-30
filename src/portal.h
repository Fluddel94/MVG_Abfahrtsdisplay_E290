// portal.h
// Einstellungsportal: Webseite im Heimnetz (http://<IP-Adresse>/) zum
// Aendern der Einstellungen, mit Stationssuche und Richtungsanzeige.
//
// Das Portal ist nur auf Abruf offen (Systemlog, Ersteinrichtung, nach
// "WLAN verbinden" im Web-Installer) und schliesst sich nach
// PORTAL_DURATION_MS von selbst. Es laeuft in einem eigenen Task, damit die
// Seite auch waehrend Display-Updates sofort reagiert. Gespeicherte
// Einstellungen uebernimmt portalLoop() im loop-Kontext.
#pragma once
#include <Arduino.h>

// Firmware-Version fuer die Info auf der Seite (einmal in setup())
void portalBegin(const char* firmwareVersion);

// Portal oeffnen bzw. die Laufzeit neu starten (PORTAL_DURATION_MS).
// Nur bei bestehender WLAN-Verbindung aufrufen.
void portalOpen();

bool portalIsOpen();

// Portal-Adresse, z.B. "http://192.168.178.45/"
String portalUrl();

// Uhrzeit, zu der das Portal schliesst ("HH:MM"), "" wenn unbekannt
String portalClosingTime();

// Regelmaessig in loop() aufrufen. Uebernimmt gespeicherte Einstellungen
// (true = Einstellungen wurden geaendert, Anzeige neu aufbauen) und fuehrt
// "Werkseinstellungen" aus (Neustart, kehrt dann nicht zurueck).
bool portalLoop();
