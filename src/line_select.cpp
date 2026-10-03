// line_select.cpp
// Linienauswahl (siehe line_select.h).

#include "line_select.h"

String lineKey(const char* label) {
  String key;
  if (label == nullptr) return key;
  for (const char* p = label; *p; p++) {
    if (*p != ' ') key += *p;
  }
  key.toUpperCase();
  return key;
}

char lineDirection(const char* lineId) {
  if (lineId == nullptr) return '?';
  if (strstr(lineId, ":H:") != nullptr) return 'H';
  if (strstr(lineId, ":R:") != nullptr) return 'R';
  return '?';
}

int LineCollector::find(const String& key) const {
  for (int i = 0; i < used; i++) {
    if (entries[i].key == key) return i;
  }
  return -1;
}

int LineCollector::add(const char* label, const char* transportType) {
  String key = lineKey(label);
  // Leere Labels und Sammel-Labels ("S6/8", "S8/6") sind keine waehlbaren Linien
  if (key.length() == 0 || key.indexOf('/') != -1) return -1;
  int i = find(key);
  if (i >= 0 || used >= LINE_LIST_MAX) return i;
  Entry& e = entries[used];
  e.key = key;
  e.name = label;
  e.type = transportType ? transportType : "";
  return used++;
}

void LineCollector::addLine(const char* label, const char* transportType, bool sev) {
  if (sev) return;
  add(label, transportType);
}

void LineCollector::addDeparture(const char* label, const char* transportType,
                                 const char* lineId, const char* destination,
                                 bool sev) {
  if (sev) return;
  int i = add(label, transportType);
  if (i < 0) return;
  char dir = lineDirection(lineId);
  if (dir == '?' || destination == nullptr || *destination == '\0') return;
  String* dests = entries[i].dest[dir == 'H' ? 0 : 1];
  for (int d = 0; d < LINE_DEST_MAX; d++) {
    if (dests[d].length() == 0) {
      dests[d] = destination;
      return;
    }
    if (dests[d] == destination) return;
  }
}

void LineCollector::toJson(String& out) const {
  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  for (int i = 0; i < used; i++) {
    const Entry& e = entries[i];
    JsonObject o = list.add<JsonObject>();
    o["k"] = e.key;
    o["n"] = e.name;
    o["t"] = e.type;
    for (int dir = 0; dir < 2; dir++) {
      String joined;
      for (int d = 0; d < LINE_DEST_MAX; d++) {
        if (e.dest[dir][d].length() == 0) break;
        if (joined.length()) joined += ", ";
        joined += e.dest[dir][d];
      }
      if (joined.length()) o[dir == 0 ? "H" : "R"] = joined;
    }
  }
  out = "";
  serializeJson(doc, out);
}
