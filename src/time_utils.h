// time_utils.h
// Hilfsfunktionen rund um Zeit: Laufzeit, Formatierung.
#pragma once
#include <Arduino.h>
#include <time.h>

// Laufzeit seit Boot in ms als 64-Bit-Wert (laeuft praktisch nie ueber,
// im Gegensatz zu millis() mit Ueberlauf nach ~49,7 Tagen)
uint64_t uptimeMs();

// Dauer in ms als "Xd Yh Zm" (Tage nur, wenn > 0)
String formatUptime(uint64_t ms);

// Unix-Zeitstempel (Sekunden) als "TT.MM.JJJJ HH:MM" in lokaler Zeit
String formatDateTime(time_t t);

// Aktuelle lokale Uhrzeit als "HH:MM", "--:--" falls noch keine Zeit
String getCurrentTime();

