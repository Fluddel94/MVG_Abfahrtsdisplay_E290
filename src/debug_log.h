// debug_log.h
// Zusaetzliche Diagnose im seriellen Monitor (Antwortgroessen, freier
// Arbeitsspeicher, Einzelschritte der Linienliste). Ein- und ausschalten
// mit DEBUG_LOG in config.h. Bei 0 entfallen die Ausgaben beim
// Kompilieren ganz (kein Speicher, keine Rechenzeit). Normale Meldungen
// (WLAN, Fehler, Einstellungen) laufen weiter ueber Serial.
#pragma once
#include <Arduino.h>
#include "../config.h"

#ifndef DEBUG_LOG
#define DEBUG_LOG 0
#endif

#if DEBUG_LOG
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_PRINTLN(...) do { } while (0)
#define DBG_PRINTF(...) do { } while (0)
#endif
