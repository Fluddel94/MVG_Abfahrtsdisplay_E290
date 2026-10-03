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
  bool multiDest;       // mehrere Ziele "A/B/C" (Fluegelzug) -> gleichmaessig kuerzen
};

// Richtungsfilter fuer parseDepartures()
enum DirectionFilter {
  DIR_FILTER_H,    // nur lineId mit ":H:"
  DIR_FILTER_R,    // nur lineId mit ":R:"
  DIR_FILTER_ALL   // alle Richtungen gemischt (gemischte Anzeige)
};

// Ruft den Stationsnamen ab. Bei Erfolg: true, Name (Latin-1 fuer die Anzeige) in
// nameOut. Bei Fehler: false, nameOut bleibt unveraendert.
bool fetchStationName(const char* globalId, String& nameOut);

// Laedt die aktuellen Abfahrten der Station als JSON-Rohdaten (ein
// HTTPS-Abruf). Die Verkehrsmittel werden gemaess den Einstellungen
// (appSettings, settings.h) schon per API-Parameter gefiltert. true bei
// Erfolg (HTTP 200), sonst false.
bool downloadDepartures(const char* globalId, String& payloadOut);

// Stationssuche fuer das Portal: Treffer der MVG-Suche (nur Haltestellen,
// max. 15) als JSON-Array [{"n":Name,"p":Ort,"id":globalId,"t":"S-Bahn, ...",
// "z":Tarifzone,"lat":..,"lon":..}] in UTF-8. true bei Erfolg.
bool searchStations(const String& query, String& jsonOut);

// Fuer das Portal: Linien und Ziele der naechsten Abfahrten je
// Richtungskennung als JSON-Array [{"d":"H","l":"S2","z":"Erding"}] (UTF-8,
// jede Kombination einmal). Hilft beim Festlegen von "Zentrum = H oder R".
bool listDirections(const char* globalId, String& jsonOut);

// Wertet heruntergeladene Rohdaten aus: filtert nach Richtung (oder alle),
// gleicht Stoerungs-Duplikate ab, fasst Fluegelzuege zusammen und fuellt
// result. Kein Netzwerkzugriff - kann fuer mehrere Filter auf dieselben
// Rohdaten angewendet werden.
// Rueckgabe: Anzahl Eintraege in result, oder -1 bei JSON-Fehler.
int parseDepartures(const String& payload, DirectionFilter filter,
                    Departure result[], int maxResults);
