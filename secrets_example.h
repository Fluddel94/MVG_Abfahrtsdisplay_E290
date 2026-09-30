// secrets_example.h
// VORLAGE fuer secrets.h - enthaelt nur Platzhalter.
//
// secrets.h ist OPTIONAL: Normalerweise werden die WLAN-Daten bei der
// Einrichtung eingegeben und im Geraet gespeichert. Wer selbst kompiliert,
// kann hier Standardwerte vorbelegen - gespeicherte Werte haben Vorrang.
// Einrichtung: diese Datei kopieren, die Kopie in "secrets.h" umbenennen
// und die Werte eintragen. Ist secrets.h vorhanden, muessen alle vier
// Zeilen stehen bleiben (Platzhalter genuegt fuer nicht genutzte Werte).
//
// secrets.h NIEMALS teilen oder hochladen (steht deshalb in .gitignore).
// Firmware, die mit secrets.h kompiliert wurde, enthaelt diese Werte -
// eine solche .bin daher nie weitergeben.
#pragma once

// --- WLAN, mit dem sich das Board verbindet (2,4 GHz) ---
const char* ssid = "WLAN-NAME";
const char* password = "WLAN-PASSWORT";

// --- WLAN fuer den QR-Screen, z.B. Gast-WLAN (nur bei FEATURE_WIFI_QR 1) ---
const char* qrWlanSsid = "QR-WLAN-NAME";
const char* qrWlanPassword = "QR-WLAN-PASSWORT";
