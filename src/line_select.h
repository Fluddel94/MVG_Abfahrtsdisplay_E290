// line_select.h
// Linienauswahl: Linien einer Station fuer das Portal sammeln (Liste aller
// Linien plus Beispielziele je Richtungskennung H/R) und die gespeicherte
// Auswahl auswerten (Filter fuer die Anzeige). Reine Datenlogik ohne
// Netzwerkzugriff - die Abrufe macht mvg_api.cpp.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Hoechstzahl Linien, die fuer eine Station gesammelt werden (Hbf ca. 45)
#define LINE_LIST_MAX 80
// Hoechstzahl verschiedener Beispielziele je Linie und Kennung
#define LINE_DEST_MAX 3

// Vergleichsform eines Linien-Labels: ohne Leerzeichen, Grossbuchstaben
// ("RE 80" -> "RE80", "Lufthansa Express Bus" -> "LUFTHANSAEXPRESSBUS")
String lineKey(const char* label);

// Richtungskennung einer lineId: 'H' (":H:"), 'R' (":R:") oder '?'
char lineDirection(const char* lineId);

// Kurzzeichen eines API-Verkehrsmittels fuer die gespeicherte Auswahl:
// S = SBAHN, U = UBAHN, T = TRAM, B = BUS, R = REGIONAL_BUS, Z = BAHN
// (Regionalzug), '?' = unbekannt. lineTypeName() ist die Umkehrung.
char lineTypeCode(const char* transportType);
const char* lineTypeName(char code);

// Hoechstzahl gewaehlter Linien je Station
#define LINE_SELECT_MAX 16

// Gespeicherte Linienauswahl einer Station. Textform (NVS, Portal):
// Eintraege "Linie:Richtung:Typ" mit Komma getrennt, z.B.
// "S2:H:S,RE80:B:Z" - Linie als lineKey(), Richtung H, R oder B (beide),
// Typ als lineTypeCode(). Leer = keine Auswahl (alle Linien anzeigen).
class LineSelection {
 public:
  // Text einlesen. Ungueltige Eintraege und Doppelte werden verworfen,
  // mehr als LINE_SELECT_MAX Eintraege abgeschnitten.
  void parse(const String& text);
  // Normalisierte Textform (fuer NVS und Portal)
  String toString() const;

  bool empty() const { return count == 0; }
  int size() const { return count; }

  // Linie (lineKey) mit Richtungskennung gewaehlt? dir '?' passt nur zu B.
  bool matches(const String& key, char dir) const;
  // Ist eine Linie dieses Verkehrsmittels (lineTypeCode) gewaehlt?
  bool hasType(char typeCode) const;

  // Wert fuer den API-Parameter "transportTypes" aus den Typen der
  // gewaehlten Linien (z.B. nur S2 -> "SBAHN"). "" bei leerer Auswahl.
  String transportTypes() const;

 private:
  struct Choice {
    String key;
    char dir;
    char type;
  };
  Choice items[LINE_SELECT_MAX];
  int count = 0;
};

// Sammelt die Linien einer Station: zuerst die vollstaendige Linienliste
// (API "lines"), dann Abfahrten fuer die Beispielziele.
class LineCollector {
 public:
  // Eintrag aus der Linienliste. sev = Ersatzverkehr: wird uebersprungen,
  // ebenso Sammel-Labels wie "S6/8".
  void addLine(const char* label, const char* transportType, bool sev);

  // Abfahrt: Ziel je Kennung merken. Unbekannte Linien (nicht in der
  // Linienliste) werden ergaenzt, Ersatzverkehr wird uebersprungen.
  void addDeparture(const char* label, const char* transportType,
                    const char* lineId, const char* destination, bool sev);

  int count() const { return used; }

  // JSON-Array fuer das Portal (UTF-8):
  // [{"k":"RE80","n":"RE 80","t":"BAHN","H":"Wuerzburg Hbf","R":"..."}]
  // "H"/"R" fehlen, wenn fuer die Kennung keine Fahrt gefunden wurde.
  void toJson(String& out) const;

 private:
  struct Entry {
    String key;
    String name;
    String type;
    String dest[2][LINE_DEST_MAX];   // [0] = H, [1] = R
  };
  Entry entries[LINE_LIST_MAX];
  int used = 0;

  int find(const String& key) const;
  int add(const char* label, const char* transportType);
};
