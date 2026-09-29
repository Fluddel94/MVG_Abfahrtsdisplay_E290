// secrets_example.h
// VORLAGE fuer secrets.h - enthaelt nur Platzhalter.
// Einrichtung: diese Datei kopieren, die Kopie in "secrets.h" umbenennen
// und die Werte eintragen. secrets.h NIEMALS teilen oder hochladen
// (sie steht deshalb in .gitignore).
// Werte fuer abgeschaltete Zusatzfunktionen (config.h) werden ignoriert,
// die Zeilen muessen aber stehen bleiben (Platzhalter genuegt).
#pragma once

// --- WLAN, mit dem sich das Board verbindet (2,4 GHz) ---
const char* ssid = "WLAN-NAME";
const char* password = "WLAN-PASSWORT";

// --- Optional: WLAN fuer den QR-Screen, z.B. Gast-WLAN (nur bei FEATURE_WIFI_QR 1) ---
const char* qrWlanSsid = "QR-WLAN-NAME";
const char* qrWlanPassword = "QR-WLAN-PASSWORT";

// --- Optional: Firmware-Update per WLAN (nur bei FEATURE_OTA 1) ---
// Starkes, einzigartiges Passwort verwenden
const char* otaPassword = "OTA-PASSWORT";
