// config.h
// Zentrale Konfiguration. Oben: Standardwerte der Geraete-Einstellungen
// (Schritt 1-3 plus optionale Zusatzfunktionen). Darunter: Konstanten, die
// fuer alle Geraete gleich sind, und eine Pruefung der Standardwerte.
// Zugangsdaten gehoeren NICHT hierher, sondern (optional) in secrets.h.
// Layout-Konstanten (Pixelpositionen) liegen direkt in src/display.cpp.
#pragma once

// ============================================================
// STANDARDWERTE DER GERAETE-EINSTELLUNGEN
// Die Einstellungen werden im Geraet gespeichert (NVS, siehe
// src/settings.cpp). Die Werte hier gelten nur, solange dort noch nichts
// gespeichert ist - gespeicherte Werte haben immer Vorrang.
// Schritt 1-3 der Reihe nach durchgehen. WLAN-Zugangsdaten stehen nicht
// hier, sondern (optional) in secrets.h.
// ============================================================

// ------------------------------------------------------------
// SCHRITT 1: STATION
// ------------------------------------------------------------
// globalId der Haltestelle, Format "de:09162:2". Normalerweise leer lassen:
// Dann oeffnet das Geraet beim ersten Start das Einstellungsportal, dort
// wird die Station per Namenssuche gewaehlt. Wer selbst vorbelegen will:
// ID aus der Haltestellenliste haltestellen/Haltestellen_Suche_s26.csv
// (Spalte "Globale ID", siehe README Abschnitt "Station finden").

#define STATION_GLOBAL_ID ""   // z.B. "de:09162:2" = Marienplatz

// ------------------------------------------------------------
// SCHRITT 2: RICHTUNGSANZEIGE
// ------------------------------------------------------------
// 0 = gemischt: alle Richtungen in einer Liste, Header nur Stationsname.
//     Die BOOT-Taste blaettert auf Seite 2 (Abfahrt 5-8), Rueckkehr nach
//     PAGE_AUTO_RESET_MS (30 s) oder erneutem Druck.
//     -> 2a und 2b ueberspringen.
// 1 = getrennt: eine Richtung (Zentrum oder Auswaerts), Header
//     "Station -> Richtung". Die BOOT-Taste schaltet um, nach
//     DIRECTION_AUTO_RESET_MS (30 s) zurueck zur Standardansicht.
//     Sinnvoll an Stationen ausserhalb der Innenstadt.
#define FEATURE_DIRECTION_VIEW 0

// 2a) NUR bei FEATURE_DIRECTION_VIEW 1 (sonst ohne Wirkung):
//     Welcher API-Marker faehrt Richtung Zentrum?
//     1 = ":H:" ist Richtung Zentrum, 0 = ":R:" ist Richtung Zentrum.
//     Die MVG-API kennzeichnet die Fahrtrichtung in der lineId nur mit
//     ":H:"/":R:" (empirisch ermittelt, nicht offiziell dokumentiert) -
//     welche davon Richtung Zentrum ist, haengt von Linie und Station ab.
//     Pruefen: API-Antwort im Browser oeffnen und lineId mit destination
//     vergleichen (siehe README).
#define ZENTRUM_IS_H 1

// 2b) NUR bei FEATURE_DIRECTION_VIEW 1 (sonst ohne Wirkung):
//     Standardansicht: 1 = Zentrum, 0 = Auswaerts
#define DEFAULT_VIEW_ZENTRUM 1

// ------------------------------------------------------------
// SCHRITT 3: VERKEHRSMITTEL (1 = anzeigen, 0 = ausblenden)
// ------------------------------------------------------------
// Mindestens eines muss 1 sein. Gefiltert wird schon beim Abruf (API-
// Parameter transportTypes), die Liste wird durch ausgeblendete
// Verkehrsmittel also nicht kuerzer.
#define SHOW_SBAHN 1
#define SHOW_UBAHN 1
#define SHOW_TRAM  1
#define SHOW_BUS   1   // Stadtbusse (MVG) und Regionalbusse (MVV)
#define SHOW_BAHN  0   // Regionalzuege, angezeigt z.B. als "RB56" / "RE5"

// ============================================================
// ZUSATZFUNKTIONEN (optional, standardmaessig aus)
// ============================================================

// --- WLAN-QR-Code ---
// 1 = kurzer Druck auf die QR-Taste zeigt 60 s lang einen QR-Code zum
//     Verbinden mit einem WLAN (z.B. Gast-WLAN) plus SSID/Passwort.
//     Zugangsdaten (Standardwerte) in secrets.h (qrWlanSsid,
//     qrWlanPassword).
// 0 = aus: kurzer Druck ohne Funktion. Der Log-Screen (langer Druck)
//     bleibt verfuegbar.
#define FEATURE_WIFI_QR 0

// Ueberschrift neben dem QR-Code. Passt bis ca. 13 Zeichen in Fettschrift.
// Normaler Text, Umlaute direkt schreiben (UTF-8, z.B. "Gäste-WLAN").
#define QR_SCREEN_TITLE "WLAN"

// ============================================================
// AB HIER NICHTS AENDERN - gemeinsame Konstanten fuer alle Geraete
// ============================================================

// --- Pins ---
#define BOOT_BUTTON 0     // Richtung umschalten bzw. blaettern
#define QR_BUTTON 21      // kurz: WLAN-QR, lang: Log-Screen

// --- Abfahrten ---
#define MAX_DEPARTURES_SHOWN 4
#define MAX_RAW_ENTRIES 20
// Verfruehte Abfahrten erst ab so vielen Minuten anzeigen ("-3"). Kleinere
// Werte (-1, -2) entstehen meist durch Rundung/Prognose und werden wie
// puenktlich behandelt.
#define EARLY_DEPARTURE_MIN 3

// --- Richtungsnamen im Header ---
// Kurzform, falls die Langform nicht vor die Uhr passt (siehe display.cpp).
// Umlaute als Latin-1-Escape (\xE4 = ae), passend zu den Display-Schriften
// (feste Texte im Code; Texte aus den Einstellungen sind dagegen UTF-8).
#define LABEL_ZENTRUM "Zentrum"
#define LABEL_ZENTRUM_SHORT "Ztr."
#define LABEL_AUSWAERTS "Ausw\xE4rts"
#define LABEL_AUSWAERTS_SHORT "Ausw."

// --- Zeitsteuerung ---
#define UPDATE_TARGET_SECOND 1
#define FULL_REFRESH_EVERY 10
#define BUTTON_DEBOUNCE_MS 500UL       // Mindestabstand zwischen zwei gezaehlten Druecken
#define BUTTON_EDGE_STABLE_MS 30UL     // Entprellung: Taste vorher so lange stabil
#define BUTTON_LATCH_MAX_AGE_MS 15000UL // aeltere gemerkte Druecke verwerfen
#define ERROR_RETRY_INTERVAL_MS 10000UL
// Startbildschirm (Projektname + Version) mindestens so lange zeigen.
// WLAN, Uhrzeit und erster Abruf laufen im Hintergrund weiter.
#define SPLASH_DURATION_MS 5000UL
// WLAN-Fehlerbildschirm erst, wenn so lange keine Verbindung besteht
// (beim Start und im Betrieb) - kurze Aussetzer bleiben unsichtbar
#define WIFI_ERROR_SCREEN_DELAY_MS 20000UL
#define HTTP_TIMEOUT_MS 10000        // 5000 fuehrte zu Fehlercode -11 (TLS-Handshake)
#define DIRECTION_AUTO_RESET_MS 30000UL
#define PAGE_AUTO_RESET_MS 30000UL   // Seite 2 bei FEATURE_DIRECTION_VIEW 0
#define QR_DISPLAY_DURATION_MS 60000UL
#define LOG_DISPLAY_DURATION_MS 60000UL
#define LONG_PRESS_MS 3000UL
// Einstellungsportal: so lange offen nach dem Oeffnen (Systemlog, nach
// "WLAN verbinden"); ohne gespeicherte Station bleibt es dauerhaft offen
#define PORTAL_DURATION_MS 1800000UL   // 30 Minuten

// --- API-Stoerungsstatistik (gleitendes 24h-Fenster, siehe stats.cpp) ---
#define API_FAIL_WINDOW_MS 86400000UL
// Mehr als 50 Stoerungen in 24h werden als 50 angezeigt.
#define API_FAIL_HISTORY_SIZE 50

// --- Zeitzone ---
// Deutschland inkl. Sommerzeit (POSIX-Format), verwendet in setup().
#define TIMEZONE_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================
// PRUEFUNG DER STANDARDWERTE (Fehler schon beim Kompilieren)
// ============================================================
#if !(SHOW_SBAHN || SHOW_UBAHN || SHOW_TRAM || SHOW_BUS || SHOW_BAHN)
#error "config.h Schritt 3: mindestens ein Verkehrsmittel (SHOW_...) muss 1 sein"
#endif
