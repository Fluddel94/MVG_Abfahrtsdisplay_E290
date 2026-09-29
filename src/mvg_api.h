// mvg_api.h
// Abruf und Aufbereitung der Abfahrtsdaten von der MVG-API.
// Die API ist inoffiziell/undokumentiert - Richtungscodes (":H:"/":R:") und
// infos-Typen (INCIDENT/EARLY_TERMINATION/INFO) sind empirisch ermittelt.
#pragma once
#include <Arduino.h>

// Eine fertig aufbereitete Abfahrt, wie sie auf dem Display erscheint
struct Departure {
  String line;          // z.B. "S2", "173", "RB56" (Leerzeichen entfernt)
  String destination;   // Ziel (bereits in Latin-1 fuer die Anzeige)
  String time;          // geplante Abfahrtszeit "HH:MM"
  int delayMin;
  bool cancelled;
  bool realtime;
  bool hasWarning;      // Warndreieck anzeigen
  bool isBus;           // laut API ein Bus -> generiertes Bus-Icon
};

// Richtungsfilter fuer parseDepartures()
enum DirectionFilter {
  DIR_FILTER_H,    // nur lineId mit ":H:"
  DIR_FILTER_R,    // nur lineId mit ":R:"
  DIR_FILTER_ALL   // alle Richtungen gemischt (FEATURE_DIRECTION_VIEW 0)
};

// Ruft den Stationsnamen ab. Bei Erfolg: true, Name (Latin-1 fuer die Anzeige) in
// nameOut. Bei Fehler: false, nameOut bleibt unveraendert.
bool fetchStationName(const char* globalId, String& nameOut);

// Laedt die aktuellen Abfahrten der Station als JSON-Rohdaten (ein
// HTTPS-Abruf). Die Verkehrsmittel werden gemaess SHOW_* (config.h) schon
// per API-Parameter gefiltert. true bei Erfolg (HTTP 200), sonst false.
bool downloadDepartures(const char* globalId, String& payloadOut);

// Wertet heruntergeladene Rohdaten aus: filtert nach Richtung (oder alle),
// gleicht Stoerungs-Duplikate ab, fasst Fluegelzuege zusammen und fuellt
// result. Kein Netzwerkzugriff - kann fuer mehrere Filter auf dieselben
// Rohdaten angewendet werden.
// Rueckgabe: Anzahl Eintraege in result, oder -1 bei JSON-Fehler.
int parseDepartures(const String& payload, DirectionFilter filter,
                    Departure result[], int maxResults);
