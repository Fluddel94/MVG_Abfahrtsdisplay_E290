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

struct TypeCode {
  char code;
  const char* name;
};
static const TypeCode TYPE_CODES[] = {
  { 'S', "SBAHN" }, { 'U', "UBAHN" }, { 'T', "TRAM" },
  { 'B', "BUS" }, { 'R', "REGIONAL_BUS" }, { 'Z', "BAHN" },
};

char lineTypeCode(const char* transportType) {
  if (transportType == nullptr) return '?';
  for (const TypeCode& t : TYPE_CODES) {
    if (strcmp(t.name, transportType) == 0) return t.code;
  }
  return '?';
}

const char* lineTypeName(char code) {
  for (const TypeCode& t : TYPE_CODES) {
    if (t.code == code) return t.name;
  }
  return nullptr;
}

void LineSelection::parse(const String& text) {
  count = 0;
  int start = 0;
  int len = text.length();
  while (start < len && count < LINE_SELECT_MAX) {
    int end = text.indexOf(',', start);
    if (end < 0) end = len;
    String entry = text.substring(start, end);
    start = end + 1;

    // "Linie:Richtung:Typ"
    int c1 = entry.indexOf(':');
    int c2 = c1 < 0 ? -1 : entry.indexOf(':', c1 + 1);
    if (c1 <= 0 || c2 != c1 + 2 || (int)entry.length() != c2 + 2) continue;
    String key = lineKey(entry.substring(0, c1).c_str());
    char dir = toupper((unsigned char)entry[c1 + 1]);
    char type = toupper((unsigned char)entry[c2 + 1]);
    if (key.length() == 0 || key.length() > 24 || key.indexOf(',') >= 0) continue;
    if (dir != 'H' && dir != 'R' && dir != 'B') continue;
    if (lineTypeName(type) == nullptr) continue;
    bool known = false;
    for (int i = 0; i < count; i++) {
      if (items[i].key == key) known = true;
    }
    if (known) continue;
    items[count].key = key;
    items[count].dir = dir;
    items[count].type = type;
    count++;
  }
}

String LineSelection::toString() const {
  String text;
  for (int i = 0; i < count; i++) {
    if (i) text += ',';
    text += items[i].key;
    text += ':';
    text += items[i].dir;
    text += ':';
    text += items[i].type;
  }
  return text;
}

bool LineSelection::matches(const String& key, char dir) const {
  for (int i = 0; i < count; i++) {
    if (items[i].key == key) return items[i].dir == 'B' || items[i].dir == dir;
  }
  return false;
}

bool LineSelection::hasType(char typeCode) const {
  for (int i = 0; i < count; i++) {
    if (items[i].type == typeCode) return true;
  }
  return false;
}

String LineSelection::transportTypes() const {
  String types;
  for (const TypeCode& t : TYPE_CODES) {
    if (!hasType(t.code)) continue;
    if (types.length()) types += ',';
    types += t.name;
  }
  return types;
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
