// wifi_conn.h
// WLAN-Verbindung aufbauen und nach Abbruechen wiederherstellen.
// - Sucht auf allen Kanaelen und waehlt den Zugangspunkt mit dem
//   staerksten Signal (wichtig bei Mesh/Repeatern; Standard des ESP32 ist
//   der erste gefundene).
// - Startet einen neuen Versuch erst, wenn der laufende beendet ist
//   (Abbruch gemeldet oder Zeitueberschreitung), mit wachsenden Pausen.
//   Nach mehreren Fehlschlaegen wird das WLAN kurz ganz ab- und wieder
//   eingeschaltet.
// - Mit DEBUG_LOG 1 (config.h): meldet im seriellen Monitor jeden Abbruch
//   sofort mit Reason-Code, jede neue Verbindung und einmal pro Minute
//   Zugangspunkt (BSSID), Kanal und Signal.
#pragma once

// Einmalig in setup() nach WiFi.mode(WIFI_STA), vor dem ersten Verbinden.
// ownReconnect = false: eine Erweiterung stellt die Verbindung selbst
// wieder her (extrasHandlesWifiReconnect()) - dann bleibt das automatische
// Wiederverbinden des ESP32 eingeschaltet.
void wifiConnInit(bool ownReconnect);

// Verbindungsversuch starten. Alle WiFi.begin() des Sketches laufen hier
// durch, damit wifiConnLoop() weiss, dass gerade ein Versuch laeuft.
void wifiConnBegin(const char* ssid, const char* password);

// Solange keine Verbindung besteht regelmaessig aufrufen (setup()-Warte-
// schleife und loop()): startet bei Bedarf einen neuen Versuch mit den
// gespeicherten WLAN-Daten. Blockiert nicht (ausser kurz beim Neustart
// des WLAN).
void wifiConnLoop();

// Signal, Kanal und Zugangspunkt der bestehenden Verbindung in den
// seriellen Monitor (einmal pro Minute aus loop(), nur mit DEBUG_LOG 1).
void wifiConnLogStatus();

// Neustart mit sauberer Abmeldung vom WLAN (Router gibt die Verbindung
// sofort frei, statt auf eine Zeitueberschreitung zu warten). Alle
// Neustarts des Sketches laufen hier durch. Kehrt nicht zurueck.
void restartCleanly();
