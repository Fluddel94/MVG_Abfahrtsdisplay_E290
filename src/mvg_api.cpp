// mvg_api.cpp
// Abruf und Aufbereitung der Abfahrtsdaten von der MVG-API (siehe mvg_api.h).

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "../config.h"
#include "settings.h"
#include "mvg_api.h"
#include "text_utils.h"   // utf8ToLatin1(): Umlaute fuer die Display-Schriften

// Interne Rohdaten-Struktur, nur waehrend parseDepartures() genutzt,
// um Duplikate (Stoerungsmeldung + echter Ersatzzug) abzugleichen
struct RawEntry {
  String line;
  String destination;
  String actualDestination;
  int delayMin;
  bool cancelled;
  bool realtime;
  bool earlyTermination;
  bool superseded;
  bool hasWarning;
  bool isBus;
  bool hasPlatform;   // "platform" nur bei Schienenverkehr vorhanden
  int platform;
  char direction;     // 'H' oder 'R' laut lineId, '?' falls keins von beiden
  long long plannedTime;
};

// Wandelt einen API-Zeitstempel (ms seit 1970) in lokale Zeit "HH:MM" um
static String formatTime(long long timestampMs) {
  time_t seconds = timestampMs / 1000;
  struct tm timeinfo;
  localtime_r(&seconds, &timeinfo);
  char buffer[6];
  sprintf(buffer, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return String(buffer);
}

// Bekannte Fluegelzug-Paarungen mit fester Kurzform. Die Reihenfolge der
// beiden Ziele ist egal (die API liefert die Fahrten in wechselnder
// Reihenfolge). Verglichen wird der Anfang des Ziels, damit Umlaute
// ("Flughafen Muenchen") keine Rolle spielen. Weitere Paarungen hier ergaenzen.
struct SplitTrainLabel {
  const char* destA;
  const char* destB;
  const char* label;
};
static const SplitTrainLabel SPLIT_TRAIN_LABELS[] = {
  { "Flughafen", "Freising", "Flugh./Freising" },   // S1 (Fluegelung in Neufahrn)
};

// Anzeige-Ziel fuer zwei zusammengefasste Ziele: feste Kurzform aus
// SPLIT_TRAIN_LABELS, sonst beide Ziele ausgeschrieben ("A/B"). Ist das zu
// lang, kuerzt fitText() (display.cpp) beim Zeichnen automatisch.
static String splitTrainDestination(const String& a, const String& b) {
  for (const SplitTrainLabel& p : SPLIT_TRAIN_LABELS) {
    if ((a.startsWith(p.destA) && b.startsWith(p.destB)) ||
        (a.startsWith(p.destB) && b.startsWith(p.destA))) {
      return String(p.label);
    }
  }
  return a + "/" + b;
}

// Wert fuer den API-Parameter "transportTypes" aus den Verkehrsmittel-
// Einstellungen (appSettings). Ohne diesen Parameter liefert die API
// (beobachtet, nicht dokumentiert) nur SBAHN, UBAHN, TRAM und BUS -
// Regionalbusse (REGIONAL_BUS) und Regionalzuege (BAHN) fehlen dann. Die
// Werte sind empirisch ermittelt. Mindestens ein Verkehrsmittel ist immer
// eingeschaltet (settings.cpp prueft das).
static String transportTypesParam() {
  String types;
  if (appSettings.showSbahn) types += ",SBAHN";
  if (appSettings.showUbahn) types += ",UBAHN";
  if (appSettings.showTram) types += ",TRAM";
  if (appSettings.showBus) types += ",BUS,REGIONAL_BUS";
  if (appSettings.showBahn) types += ",BAHN";
  return types.substring(1);   // fuehrendes Komma weglassen
}

// Prozent-Kodierung fuer einen URL-Parameter (UTF-8 bleibt byteweise erhalten)
static String urlEncode(const String& text) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  String out;
  for (unsigned int i = 0; i < text.length(); i++) {
    uint8_t c = (uint8_t)text[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += HEX_DIGITS[c >> 4];
      out += HEX_DIGITS[c & 0x0F];
    }
  }
  return out;
}

// Anzeigenamen der Verkehrsmittel fuer die Trefferliste der Suche
static const char* transportTypeName(const char* type) {
  if (strcmp(type, "SBAHN") == 0) return "S-Bahn";
  if (strcmp(type, "UBAHN") == 0) return "U-Bahn";
  if (strcmp(type, "TRAM") == 0) return "Tram";
  if (strcmp(type, "BUS") == 0) return "Bus";
  if (strcmp(type, "REGIONAL_BUS") == 0) return "Regionalbus";
  if (strcmp(type, "BAHN") == 0) return "Regionalzug";
  return type;
}

bool searchStations(const String& query, String& jsonOut) {
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  // Antwort (beobachtet): Array mit type STATION/ADDRESS/POI, name, place,
  // globalId, transportTypes
  String url = "https://www.mvg.de/api/bgw-pt/v3/locations?query=";
  url += urlEncode(query);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.print("Stationssuche fehlgeschlagen, HTTP-Code: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  JsonDocument filter;
  filter[0]["type"] = true;
  filter[0]["name"] = true;
  filter[0]["place"] = true;
  filter[0]["globalId"] = true;
  filter[0]["transportTypes"] = true;
  filter[0]["tariffZones"] = true;
  filter[0]["latitude"] = true;
  filter[0]["longitude"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) return false;

  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (JsonObject loc : doc.as<JsonArray>()) {
    const char* type = loc["type"];
    const char* id = loc["globalId"];
    if (type == nullptr || id == nullptr || strcmp(type, "STATION") != 0) continue;
    String types;
    JsonArray typeList = loc["transportTypes"];
    for (const char* t : typeList) {
      if (types.length()) types += ", ";
      types += transportTypeName(t);
    }
    JsonObject entry = list.add<JsonObject>();
    entry["n"] = loc["name"] | "";
    entry["p"] = loc["place"] | "";
    entry["id"] = id;
    entry["t"] = types;
    entry["z"] = loc["tariffZones"] | "";
    if (loc["latitude"].is<float>() && loc["longitude"].is<float>()) {
      entry["lat"] = loc["latitude"];
      entry["lon"] = loc["longitude"];
    }
    if (list.size() >= 15) break;
  }
  serializeJson(out, jsonOut);
  return true;
}

bool listDirections(const char* globalId, String& jsonOut) {
  String payload;
  if (!downloadDepartures(globalId, payload)) return false;

  JsonDocument filter;
  filter[0]["lineId"] = true;
  filter[0]["label"] = true;
  filter[0]["destination"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) return false;

  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (JsonObject dep : doc.as<JsonArray>()) {
    String lineId = dep["lineId"] | "";
    const char* dir = lineId.indexOf(":H:") != -1 ? "H" : (lineId.indexOf(":R:") != -1 ? "R" : "?");
    const char* line = dep["label"] | "";
    const char* dest = dep["destination"] | "";
    bool known = false;
    for (JsonObject e : list) {
      if (strcmp(e["d"], dir) == 0 && strcmp(e["l"], line) == 0 && strcmp(e["z"], dest) == 0) {
        known = true;
        break;
      }
    }
    if (known) continue;
    JsonObject entry = list.add<JsonObject>();
    entry["d"] = dir;
    entry["l"] = line;
    entry["z"] = dest;
  }
  serializeJson(out, jsonOut);
  return true;
}

bool fetchStationName(const char* globalId, String& nameOut) {
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  String url = "https://www.mvg.de/.rest/zdm/stations/";
  url += globalId;

  http.begin(url);
  int httpCode = http.GET();
  bool success = false;

  if (httpCode == 200) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["name"].is<const char*>()) {
      nameOut = utf8ToLatin1(String((const char*)doc["name"]));
      success = true;
      Serial.print("Stationsname ermittelt: ");
      Serial.println(nameOut);
    } else {
      Serial.println("Stationsname konnte nicht geparst werden, nutze Fallback.");
    }
  } else {
    Serial.print("Stationsname-Abruf fehlgeschlagen, HTTP-Code: ");
    Serial.println(httpCode);
  }

  http.end();
  return success;
}

bool downloadDepartures(const char* globalId, String& payloadOut) {
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  String url = "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=";
  url += globalId;
  url += "&limit=";
  url += API_DEPARTURE_LIMIT;
  url += "&transportTypes=";
  url += transportTypesParam();

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    // Negative Codes kommen vom ESP32-HTTPClient selbst (z.B. -1 Verbindung
    // fehlgeschlagen, -11 keine Antwort innerhalb HTTP_TIMEOUT_MS)
    Serial.print("Fehler beim Abruf, HTTP-Code: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  payloadOut = http.getString();
  http.end();
  return true;
}

int parseDepartures(const String& payload, DirectionFilter filter,
                    Departure result[], int maxResults) {
  // Nur die ausgewerteten Felder einlesen: Die Antwort enthaelt je Fahrt
  // viele weitere Felder; ohne Filter braucht die Auswertung bei
  // API_DEPARTURE_LIMIT Fahrten ein Vielfaches an Speicher.
  JsonDocument fields;
  JsonObject f = fields[0].to<JsonObject>();
  f["lineId"] = true;
  f["label"] = true;
  f["destination"] = true;
  f["delayInMinutes"] = true;
  f["cancelled"] = true;
  f["realtime"] = true;
  f["plannedDepartureTime"] = true;
  f["transportType"] = true;
  f["product"] = true;
  f["platform"] = true;
  f["infos"][0]["type"] = true;
  f["infos"][0]["message"] = true;

  JsonDocument doc;
  DeserializationError error =
      deserializeJson(doc, payload, DeserializationOption::Filter(fields));

  if (error) {
    Serial.print("JSON-Parsing fehlgeschlagen: ");
    Serial.println(error.c_str());
    return -1;
  }

  // --- Schritt 1: Rohdaten einlesen, relevante infos-Typen erkennen ---
  RawEntry raw[MAX_RAW_ENTRIES];
  int rawCount = 0;

  JsonArray departures = doc.as<JsonArray>();
  for (JsonObject dep : departures) {
    if (rawCount >= MAX_RAW_ENTRIES) break;

    // Richtung aus der lineId (":H:" / ":R:") - bei DIR_FILTER_H/_R wird
    // danach gefiltert, bei DIR_FILTER_ALL nur fuer die Abgleiche gemerkt
    const char* lineIdRaw = dep["lineId"];
    String lineId = String(lineIdRaw);
    char direction = '?';
    if (lineId.indexOf(":H:") != -1) direction = 'H';
    else if (lineId.indexOf(":R:") != -1) direction = 'R';

    if (filter == DIR_FILTER_H && direction != 'H') continue;
    if (filter == DIR_FILTER_R && direction != 'R') continue;

    RawEntry r;
    r.direction = direction;
    r.line = String((const char*)dep["label"]);
    r.destination = utf8ToLatin1(String((const char*)dep["destination"]));
    r.actualDestination = r.destination;
    r.delayMin = dep["delayInMinutes"] | 0;
    r.cancelled = dep["cancelled"] | false;
    r.realtime = dep["realtime"] | false;
    r.plannedTime = dep["plannedDepartureTime"];
    r.earlyTermination = false;
    r.superseded = false;
    r.hasWarning = false;

    // Verkehrsmittel erkennen (fuer das Liniensymbol). Der Feldname ist
    // nicht offiziell dokumentiert: "transportType" (bgw-pt/v3), als
    // Rueckfall "product" (aeltere API). Beobachtete Werte: SBAHN, UBAHN,
    // TRAM, BUS, REGIONAL_BUS, BAHN. Fehlt beides, wird das Icon nur
    // anhand des Labels gewaehlt.
    const char* transportType = dep["transportType"];
    if (transportType == nullptr) transportType = dep["product"];
    r.isBus = (transportType != nullptr && strstr(transportType, "BUS") != nullptr);

    // Regionalzuege: Label kommt als "RB 56" / "RE 5" - ohne Leerzeichen
    // passt es in die Pixelschrift der generierten Icons ("RB56")
    if (transportType != nullptr && strcmp(transportType, "BAHN") == 0) {
      r.line.replace(" ", "");
    }

    // Gleis (nur S-/U-Bahn und Zuege) - fuer die Erkennung von Fluegelzuegen
    r.hasPlatform = dep["platform"].is<int>();
    r.platform = dep["platform"] | -1;

    // Nur diese beiden Typen gelten als fahrtrelevant und loesen ein
    // Warndreieck aus. Andere Typen (z.B. "INFO" - Tarifhinweise etc.)
    // werden bewusst ignoriert. Neue fahrtrelevante Typen hier ergaenzen.
    JsonArray infos = dep["infos"];
    for (JsonObject info : infos) {
      const char* type = info["type"];
      if (type == nullptr) continue;
      String typeStr = String(type);

      if (typeStr == "INCIDENT" || typeStr == "EARLY_TERMINATION") {
        r.hasWarning = true;
      }

      if (typeStr == "EARLY_TERMINATION") {
        r.earlyTermination = true;
        const char* msg = info["message"];
        if (msg != nullptr) {
          String m = utf8ToLatin1(String(msg));
          int idx = m.indexOf("bis ");
          if (idx != -1) {
            r.actualDestination = m.substring(idx + 4);
          }
        }
      }
    }

    raw[rawCount] = r;
    rawCount++;
  }

  // --- Schritt 2: Duplikate abgleichen (Stoerung + echter Ersatzzug) ---
  // Gleiche Linie und Richtung werden mitgeprueft, damit bei gemischten
  // Richtungen kein Zug der Gegenrichtung zur selben Minute erwischt wird.
  for (int i = 0; i < rawCount; i++) {
    if (!raw[i].cancelled || !raw[i].earlyTermination || raw[i].superseded) continue;

    for (int j = 0; j < rawCount; j++) {
      if (i == j) continue;
      if (raw[j].cancelled || raw[j].superseded) continue;
      if (raw[j].plannedTime != raw[i].plannedTime) continue;
      if (raw[j].line != raw[i].line) continue;
      if (raw[j].direction != raw[i].direction) continue;

      // Passenden, echten Ersatzzug gefunden
      raw[j].hasWarning = true;
      raw[i].superseded = true;
      break;
    }
  }

  // --- Schritt 3: vorzeitiges Fahrtende ohne Ersatzzug aufloesen ---
  // Ob der Zug hier noch haelt, entscheidet die Ausfall-Markierung der API
  // (beobachtet 30.09.2026: endet die Fahrt vor oder an dieser Station, ist
  // sie hier "cancelled"). Die Lage des neuen Endhalts wird NICHT aus dem
  // Text abgeleitet. Danach steht in destination das angezeigte Ziel, damit
  // Schritt 4 solche Fahrten wie alle anderen zusammenfassen kann.
  for (int i = 0; i < rawCount; i++) {
    if (raw[i].superseded || !raw[i].earlyTermination) continue;

    // Diagnose, damit abweichende Faelle bei Stoerungen auffallen
    Serial.print("Vorzeitiges Fahrtende: ");
    Serial.print(raw[i].line);
    Serial.print(" ");
    Serial.print(formatTime(raw[i].plannedTime));
    Serial.print(" Ziel=");
    Serial.print(raw[i].destination);
    Serial.print(" neu=");
    Serial.print(raw[i].actualDestination);
    Serial.println(raw[i].cancelled ? " -> hier ausgefallen" : " -> haelt hier");

    // Ausfall: urspruengliches Ziel bleiben lassen.
    // Haelt hier, endet aber frueher: tatsaechliches Ziel anzeigen.
    if (!raw[i].cancelled) raw[i].destination = raw[i].actualDestination;
    raw[i].hasWarning = true;
  }

  // --- Schritt 4: Fluegelzuege und doppelte Fahrten zusammenfassen ---
  // Ein Zug, der unterwegs geteilt wird (z.B. S1 -> Flughafen / Freising),
  // erscheint in der API als mehrere Fahrten mit gleicher Linie, gleicher
  // geplanter Zeit und gleichem Gleis, aber verschiedenem Ziel. Sie werden
  // zu einer Zeile zusammengefasst: bekannte Paarungen mit fester Kurzform
  // ("Flugh./Freising", siehe SPLIT_TRAIN_LABELS), sonst alle Ziele voll
  // ausgeschrieben mit "/" (Kuerzung beim Zeichnen per fitText()).
  // Haben mehrere Fahrten auch dasselbe Ziel (z.B. beide Zugteile einer S1
  // bei einer Stoerung nicht vereinigt, beobachtet 30.09.2026), bleibt nur
  // eine Zeile uebrig.
  // Die Richtung muss uebereinstimmen (relevant bei DIR_FILTER_ALL).
  // Bewusst NICHT zusammengefasst:
  // - Busse (kein Gleis; Verstaerkerbusse waeren sonst falsch vereint)
  // - Eintraege ohne Gleisangabe
  // - unterschiedlicher Ausfall-Status: faellt nur ein Zugteil aus, bleibt
  //   dieser als eigene, durchgestrichene Zeile sichtbar
  // Uebernommen wird jeweils der "schlechtere" Wert (Verspaetung, Warnung).
  for (int i = 0; i < rawCount; i++) {
    if (raw[i].superseded || raw[i].isBus || !raw[i].hasPlatform) continue;

    String firstDest = raw[i].destination;
    String secondDest;
    String allDests = firstDest;   // alle Ziele voll ausgeschrieben
    int parts = 1;
    int duplicates = 0;
    for (int j = i + 1; j < rawCount; j++) {
      if (raw[j].superseded || raw[j].isBus || !raw[j].hasPlatform) continue;
      if (raw[j].line != raw[i].line) continue;
      if (raw[j].direction != raw[i].direction) continue;
      if (raw[j].plannedTime != raw[i].plannedTime) continue;
      if (raw[j].platform != raw[i].platform) continue;
      if (raw[j].cancelled != raw[i].cancelled) continue;

      // Ziel schon enthalten -> doppelte Fahrt, sonst weiterer Zugteil
      String wrapped = "/" + allDests + "/";
      if (wrapped.indexOf("/" + raw[j].destination + "/") != -1) {
        duplicates++;
      } else {
        if (parts == 1) secondDest = raw[j].destination;
        allDests += "/";
        allDests += raw[j].destination;
        parts++;
      }

      if (raw[j].delayMin > raw[i].delayMin) raw[i].delayMin = raw[j].delayMin;
      raw[i].hasWarning = raw[i].hasWarning || raw[j].hasWarning;
      raw[i].realtime = raw[i].realtime || raw[j].realtime;
      raw[j].superseded = true;
    }

    if (parts > 1) {
      // Zwei Ziele: ggf. feste Kurzform; mehr als zwei: immer ausgeschrieben
      raw[i].destination = (parts == 2) ? splitTrainDestination(firstDest, secondDest)
                                        : allDests;
    }
    if (parts > 1 || duplicates > 0) {
      Serial.print(parts > 1 ? "Fluegelzug zusammengefasst: " : "Doppelte Fahrt zusammengefasst: ");
      Serial.print(raw[i].line);
      Serial.print(" ");
      Serial.print(raw[i].destination);
      if (duplicates > 0) {
        Serial.print(" (");
        Serial.print(duplicates);
        Serial.print(" doppelt)");
      }
      Serial.println(raw[i].cancelled ? " - Ausfall" : "");
    }
  }

  // --- Schritt 5: finale Liste zusammenstellen ---
  int count = 0;
  for (int i = 0; i < rawCount; i++) {
    if (count >= maxResults) break;
    if (raw[i].superseded) continue;

    Departure d;
    d.line = raw[i].line;
    d.destination = raw[i].destination;
    d.cancelled = raw[i].cancelled;
    d.hasWarning = raw[i].hasWarning;
    d.delayMin = raw[i].delayMin;
    d.realtime = raw[i].realtime;
    d.isBus = raw[i].isBus;
    d.time = formatTime(raw[i].plannedTime);

    result[count] = d;
    count++;
  }

  return count;
}
