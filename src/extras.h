// extras.h
// Andockstellen fuer eigene Zusatzfunktionen.
//
// Was das ist: feste Stellen im Programmablauf (Start, Hauptschleife,
// Fehlerzustand usw.), an denen eine eigene Variante des Projekts zusaetzlichen
// Code ausfuehren kann - z.B. eine Status-LED ansteuern oder einen Zusatz an
// die Versionsnummer haengen. So bleibt der gemeinsame Code unveraendert und
// Updates lassen sich ohne Konflikte uebernehmen. Der Autor nutzt das selbst
// fuer eine private Variante mit Zusatzfunktionen fuer den Eigengebrauch.
//
// Was das NICHT ist: keine Fernsteuerung und keine Datenuebertragung. In
// dieser Version tun alle Funktionen nichts (leere Standardfassungen in
// extras.cpp, als "weak" markiert). Aktiv wird eine Erweiterung nur, wenn
// jemand eigenen Code in src/ ablegt und die Firmware selbst kompiliert -
// nicht nachtraeglich und nicht von aussen.
//
// Eigene Erweiterung: eine eigene .cpp in src/ anlegen und dieselben
// Funktionen dort neu definieren - der Linker nimmt dann deren Fassung.
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
