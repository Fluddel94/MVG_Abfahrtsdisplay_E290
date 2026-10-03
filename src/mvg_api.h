// mvg_api.h
// Abruf und Aufbereitung der Abfahrtsdaten von der MVG-API.
// Die API ist inoffiziell/undokumentiert - Richtungscodes (":H:"/":R:") und
// infos-Typen (INCIDENT/EARLY_TERMINATION/INFO) sind empirisch ermittelt.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "settings.h"      // StationConfig
#include "line_select.h"   // LineSelection

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

// Laedt die aktuellen Abfahrten einer Station (ein HTTPS-Abruf) in docOut -
// nur die ausgewerteten Felder, direkt aus dem Datenstrom. Die
// Verkehrsmittel werden schon per API-Parameter gefiltert: aus der
// Linienauswahl, sonst aus station.types. Mit Linienauswahl wird mehr
// abgefragt (API_DEPARTURE_LIMIT_LINES). true bei Erfolg.
bool downloadDepartures(const StationConfig& station, const LineSelection& lines,
                        JsonDocument& docOut);

// Stationssuche fuer das Portal: Treffer der MVG-Suche (nur Haltestellen,
// max. 15) als JSON-Array [{"n":Name,"p":Ort,"id":globalId,"t":"S-Bahn, ...",
// "z":Tarifzone,"lat":..,"lon":..}] in UTF-8. true bei Erfolg.
bool searchStations(const String& query, String& jsonOut);

// Fuer das Portal: Linien und Ziele der naechsten Abfahrten je
// Richtungskennung als JSON-Array [{"d":"H","l":"S2","z":"Erding"}] (UTF-8,
// jede Kombination einmal). Hilft beim Festlegen von "Zentrum = H oder R".
bool listDirections(const char* globalId, String& jsonOut);

// Fuer das Portal (Linienauswahl): alle Linien der Station mit
// Beispielzielen je Richtungskennung als JSON-Array
// [{"k":"RE80","n":"RE 80","t":"BAHN","H":"...","R":"..."}] (UTF-8, Format
// siehe line_select.h). Fuenf Abrufe (Linienliste + Abfahrten ueber rund 12
// Stunden, alle Verkehrsmittel), dauert einige Sekunden. true, wenn
// wenigstens die Linienliste abgerufen werden konnte.
bool listStationLines(const char* globalId, String& jsonOut);

// Wertet heruntergeladene Abfahrten (downloadDepartures) aus: filtert nach
// Richtung (oder alle) und Linienauswahl (leer = alle Linien), gleicht
// Stoerungs-Duplikate ab, fasst Fluegelzuege zusammen und fuellt result.
// Kein Netzwerkzugriff - kann fuer mehrere Filter auf dieselben Daten
// angewendet werden.
// Rueckgabe: Anzahl Eintraege in result.
int parseDepartures(const JsonDocument& doc, DirectionFilter filter,
                    const LineSelection& lines, Departure result[], int maxResults);
