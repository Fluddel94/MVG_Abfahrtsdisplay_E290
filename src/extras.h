// extras.h
// Andockstellen fuer optionale Erweiterungen (z.B. eine Status-LED in einer
// eigenen Variante des Projekts).
//
// Im Projekt selbst tun diese Funktionen nichts (leere Standardfassungen in
// extras.cpp, als "weak" markiert). Eine Erweiterung legt eine eigene .cpp
// in src/ an und definiert dieselben Funktionen neu - der Linker nimmt dann
// deren Fassung. Am uebrigen Code muss dafuer nichts geaendert werden.
#pragma once

// Ganz am Anfang von setup(), vor dem WLAN-Aufbau (z.B. Pins einrichten)
void extrasBegin();

// Am Ende von setup(): WLAN, Uhrzeit und Stationsname stehen bereits,
// die ersten Abfahrten werden direkt danach abgerufen.
// firmwareVersion = FW_VERSION inkl. extrasVersionSuffix()
void extrasSetup(const char* firmwareVersion);

// Am Anfang jedes loop()-Durchlaufs
void extrasLoop();

// Aktueller Zustand, in jedem Durchlauf von loop() und waehrend des
// WLAN-Aufbaus in setup(): true = WLAN- oder API-Fehler
void extrasStatus(bool hasError);

// true = eine Erweiterung stellt die WLAN-Verbindung nach einem Abbruch
// selbst wieder her; loop() ruft dann kein eigenes WiFi.reconnect() auf
bool extrasHandlesWifiReconnect();

// Neue WLAN-Daten wurden gespeichert (z.B. per Web-Installer), das Board ist
// bereits mit dem neuen WLAN verbunden. Fuer Erweiterungen, die sich die
// WLAN-Daten beim Start merken (z.B. fuer einen eigenen Reconnect).
void extrasWifiChanged();

// Zusatz zur Versionsnummer, z.B. "-variante" (Standard: "").
// Erscheint im seriellen Monitor und im System-Log.
const char* extrasVersionSuffix();
