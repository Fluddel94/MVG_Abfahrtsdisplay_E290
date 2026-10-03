// line_select.h
// Linienauswahl: Linien einer Station fuer das Portal sammeln (Liste aller
// Linien plus Beispielziele je Richtungskennung H/R). Reine Datenlogik ohne
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
