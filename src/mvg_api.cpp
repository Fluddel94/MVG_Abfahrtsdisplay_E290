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
#include "line_select.h"  // LineCollector: Linienliste fuers Portal

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
  bool selected;      // Linienauswahl: Fahrt anzeigen (siehe Schritt 1/5)
  bool hasWarning;
  bool isBus;
  bool hasPlatform;   // "platform" nur bei Schienenverkehr vorhanden
  int platform;
  bool multiDest;      // destination = mehrere Ziele "A/B/C"
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

// Feste Kurzform fuer zwei zusammengefasste Ziele aus SPLIT_TRAIN_LABELS,
// leer wenn keine passt
static String fixedSplitLabel(const String& a, const String& b) {
  for (const SplitTrainLabel& p : SPLIT_TRAIN_LABELS) {
    if ((a.startsWith(p.destA) && b.startsWith(p.destB)) ||
        (a.startsWith(p.destB) && b.startsWith(p.destA))) {
      return String(p.label);
    }
  }
  return "";
}

// Fluegelzuege mit verschiedenen Liniennummern (Regionalzuege): Die Zugteile
// erscheinen in der API als eigene Linien mit gleicher geplanter Zeit,
// gleichem Gleis und gleicher Richtung, aber ohne gemeinsames Merkmal
// (geprueft 03.10.2026). Zusammengefasst wird daher nur innerhalb dieser
// festen Gruppen - zufaellig gleichzeitige fremde Zuege bleiben getrennt.
// Linien ohne Leerzeichen (wie RawEntry.line), mit Komma am Anfang und Ende.
// Weitere Gruppen hier ergaenzen.
struct SplitTrainGroup {
  const char* lines;
  const char* icon;    // Liniensymbol der zusammengefassten Zeile
};
static const SplitTrainGroup SPLIT_TRAIN_GROUPS[] = {
  { ",RB55,RB56,RB57,", "RB" },   // BRB Oberland (Bayrischzell/Lenggries/Tegernsee)
  { ",RB6,RB60,",       "RB" },   // Werdenfelsbahn (Mittenwald/Pfronten)
  { ",RB65,RB66,",      "RB" },
  { ",RE80,RE89,",      "RE" },
};

// Index der Fluegelzug-Gruppe einer Linie, -1 wenn keine
static int splitTrainGroup(const String& line) {
  String key = "," + line + ",";
  int count = sizeof(SPLIT_TRAIN_GROUPS) / sizeof(SPLIT_TRAIN_GROUPS[0]);
  for (int g = 0; g < count; g++) {
    if (strstr(SPLIT_TRAIN_GROUPS[g].lines, key.c_str()) != nullptr) return g;
  }
  return -1;
}

// Wert fuer den API-Parameter "transportTypes": aus den gewaehlten Linien
// (z.B. nur S2 -> "SBAHN"), sonst aus den Verkehrsmittel-Einstellungen der
// Station. Ohne diesen Parameter liefert die API (beobachtet, nicht
// dokumentiert) nur SBAHN, UBAHN, TRAM und BUS - Regionalbusse
// (REGIONAL_BUS) und Regionalzuege (BAHN) fehlen dann. Die Werte sind
// empirisch ermittelt. Mindestens ein Verkehrsmittel ist immer
// eingeschaltet (settings.cpp prueft das). Ersatzbusse (SEV) kommen beim
// Filter des ersetzten Verkehrsmittels mit.
static String transportTypesParam(const StationConfig& station,
                                  const LineSelection& lines) {
  if (!lines.empty()) return lines.transportTypes();
  String types;
  if (station.types & TYPE_SBAHN) types += ",SBAHN";
  if (station.types & TYPE_UBAHN) types += ",UBAHN";
  if (station.types & TYPE_TRAM) types += ",TRAM";
  if (station.types & TYPE_BUS) types += ",BUS,REGIONAL_BUS";
  if (station.types & TYPE_BAHN) types += ",BAHN";
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
  // Verkehrsmittel wie bei Station 1, aber ohne Linienauswahl
  StationConfig station;
  {
    SettingsLock lock;
    station = stationConfig(0);
  }
  station.id = globalId;
  station.lines = "";
  LineSelection noLines;
  JsonDocument doc;
  if (!downloadDepartures(station, noLines, doc)) return false;

  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (JsonObject dep : doc.as<JsonArray>()) {
    char dirCode = lineDirection(dep["lineId"]);
    const char* dir = dirCode == 'H' ? "H" : (dirCode == 'R' ? "R" : "?");
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

// Alle Verkehrsmittel fuer Abrufe, die unabhaengig von den Einstellungen
// sein muessen (Linienliste)
static const char* ALL_TRANSPORT_TYPES = "SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN";

// Versatz (Minuten) der Abfahrtsabrufe fuer die Beispielziele der
// Linienliste: deckt rund 12 Stunden ab, auch seltene Linien
static const int LINE_SAMPLE_OFFSETS[] = { 0, 120, 360, 720 };
// Abfahrten je Abruf (die API liefert hoechstens 100)
#define LINE_SAMPLE_LIMIT 100

// Freien Heap im seriellen Monitor ausgeben (Messung fuer die Linienliste)
static void logHeap(const char* step) {
  Serial.printf("Heap %s: frei %u, groesster Block %u, Minimum %u\n", step,
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)ESP.getMinFreeHeap());
}

// Lesestrom fuer den Antwortinhalt eines HTTP-Abrufs: wartet bis zu
// timeoutMs auf weitere Daten (die Antwort kommt in mehreren TLS-Bloecken)
// und entfernt die Blocklaengen, falls der Server trotz HTTP/1.0 in
// Bloecken ("chunked") antwortet. Zaehlt die gelesenen Bytes.
class HttpBodyStream : public Stream {
 public:
  HttpBodyStream(Client& client, bool chunked, unsigned long timeoutMs)
      : in(client), isChunked(chunked), timeout(timeoutMs) {}

  int read() override {
    int c = peek();
    peeked = -1;
    if (c >= 0) bytesRead++;
    return c;
  }
  int peek() override {
    if (peeked < 0 && !finished) peeked = nextBodyByte();
    return peeked;
  }
  int available() override { return (peeked >= 0 || !finished) ? 1 : 0; }
  size_t write(uint8_t) override { return 0; }
  size_t count() const { return bytesRead; }
  bool timedOut() const { return hitTimeout; }

 private:
  Client& in;
  bool isChunked;
  unsigned long timeout;
  long chunkLeft = 0;      // Restbytes im aktuellen Block
  bool finished = false;
  bool hitTimeout = false;
  int peeked = -1;
  size_t bytesRead = 0;

  // Naechstes Byte vom Netz, -1 bei Ende oder Zeitueberschreitung
  int rawByte() {
    unsigned long start = millis();
    while (true) {
      int c = in.read();
      if (c >= 0) return c;
      if (!in.connected() && in.available() <= 0) return -1;
      if (millis() - start >= timeout) {
        hitTimeout = true;
        return -1;
      }
      delay(1);
    }
  }

  int nextBodyByte() {
    if (!isChunked) {
      int c = rawByte();
      if (c < 0) finished = true;
      return c;
    }
    if (chunkLeft == 0) {
      // Blockkopf: Laenge hexadezimal, ggf. ";Erweiterung", dann CRLF
      long size = 0;
      bool digits = true;
      while (true) {
        int c = rawByte();
        if (c < 0) { finished = true; return -1; }
        if (c == '\n') break;
        if (c == ';' || c == '\r') digits = false;
        if (!digits) continue;
        if (isxdigit(c)) size = size * 16 + (isdigit(c) ? c - '0' : (tolower(c) - 'a' + 10));
      }
      if (size == 0) { finished = true; return -1; }
      chunkLeft = size;
    }
    int c = rawByte();
    if (c < 0) { finished = true; return -1; }
    if (--chunkLeft == 0) {
      rawByte();   // CR
      rawByte();   // LF
    }
    return c;
  }
};

// GET-Abruf und Auswertung direkt aus dem Datenstrom (ohne die ganze
// Antwort als String zu halten). HTTP/1.0, damit der Server moeglichst
// nicht in Bloecken antwortet (falls doch, entfernt HttpBodyStream die
// Blocklaengen). true bei HTTP 200 und gueltigem JSON.
static bool fetchJsonStream(const String& url, JsonDocument& doc,
                            const JsonDocument& filter) {
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);
  const char* headerKeys[] = { "Transfer-Encoding" };
  http.collectHeaders(headerKeys, 1);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.print("Abruf fehlgeschlagen, HTTP-Code: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }
  bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  int size = http.getSize();   // -1 = Laenge nicht angegeben
  HttpBodyStream body(http.getStream(), chunked, HTTP_TIMEOUT_MS);
  DeserializationError error = deserializeJson(doc, body,
                                               DeserializationOption::Filter(filter));
  http.end();
  Serial.printf("Antwort: Laenge %d, chunked %d, gelesen %u Bytes%s\n", size,
                chunked ? 1 : 0, (unsigned)body.count(),
                body.timedOut() ? ", Zeitueberschreitung" : "");
  if (error) {
    Serial.print("JSON-Fehler: ");
    Serial.println(error.c_str());
    return false;
  }
  return true;
}

bool listStationLines(const char* globalId, String& jsonOut) {
  unsigned long start = millis();
  logHeap("vor Linienliste");

  // Gross (ca. 80 Linien), daher auf dem Heap statt auf dem Task-Stack
  LineCollector* lines = new LineCollector();
  if (lines == nullptr) return false;

  // 1. Vollstaendige Linienliste der Station
  JsonDocument filter;
  filter[0]["label"] = true;
  filter[0]["transportType"] = true;
  filter[0]["sev"] = true;
  JsonDocument doc;
  String url = "https://www.mvg.de/api/bgw-pt/v3/lines/";
  url += globalId;
  if (!fetchJsonStream(url, doc, filter)) {
    delete lines;
    return false;
  }
  for (JsonObject l : doc.as<JsonArray>()) {
    lines->addLine(l["label"], l["transportType"], l["sev"] | false);
  }
  Serial.printf("Linienliste: %d Linien\n", lines->count());
  logHeap("nach lines");

  // 2. Beispielziele je Kennung H/R aus Abfahrten (ca. 12 Stunden).
  // Fehlschlaege einzelner Abrufe sind egal - dann fehlen nur Ziele.
  filter.clear();
  filter[0]["label"] = true;
  filter[0]["transportType"] = true;
  filter[0]["lineId"] = true;
  filter[0]["destination"] = true;
  filter[0]["sev"] = true;
  for (int offset : LINE_SAMPLE_OFFSETS) {
    url = "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=";
    url += globalId;
    url += "&limit=";
    url += LINE_SAMPLE_LIMIT;
    url += "&offsetInMinutes=";
    url += offset;
    url += "&transportTypes=";
    url += ALL_TRANSPORT_TYPES;
    doc.clear();
    if (!fetchJsonStream(url, doc, filter)) continue;
    for (JsonObject d : doc.as<JsonArray>()) {
      lines->addDeparture(d["label"], d["transportType"], d["lineId"],
                          d["destination"], d["sev"] | false);
    }
    Serial.printf("Abfahrten ab +%d Min.: %d\n", offset, (int)doc.size());
    logHeap("nach Abfahrten");
  }

  lines->toJson(jsonOut);
  Serial.printf("Linienliste fertig: %d Linien, %u Bytes JSON, %lu ms\n",
                lines->count(), (unsigned)jsonOut.length(), millis() - start);
  delete lines;
  logHeap("Ende Linienliste");
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

bool downloadDepartures(const StationConfig& station, const LineSelection& lines,
                        JsonDocument& docOut) {
  // Nur die ausgewerteten Felder einlesen: Die Antwort enthaelt je Fahrt
  // viele weitere Felder; ohne Filter braucht die Auswertung ein Vielfaches
  // an Speicher. Gelesen wird direkt aus dem Datenstrom (fetchJsonStream).
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
  f["sev"] = true;
  f["infos"][0]["type"] = true;
  f["infos"][0]["message"] = true;

  String url = "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=";
  url += station.id;
  url += "&limit=";
  url += lines.empty() ? API_DEPARTURE_LIMIT : API_DEPARTURE_LIMIT_LINES;
  url += "&transportTypes=";
  url += transportTypesParam(station, lines);

  // Negative HTTP-Codes im seriellen Monitor kommen vom ESP32-HTTPClient
  // selbst (z.B. -1 Verbindung fehlgeschlagen, -11 keine Antwort innerhalb
  // HTTP_TIMEOUT_MS)
  docOut.clear();
  return fetchJsonStream(url, docOut, fields);
}

int parseDepartures(const JsonDocument& doc, DirectionFilter filter,
                    const LineSelection& lines, Departure result[], int maxResults) {
  // --- Schritt 1: Rohdaten einlesen, relevante infos-Typen erkennen ---
  RawEntry raw[MAX_RAW_ENTRIES];
  int rawCount = 0;

  JsonArrayConst departures = doc.as<JsonArrayConst>();
  for (JsonObjectConst dep : departures) {
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

    const char* transportType = dep["transportType"];
    if (transportType == nullptr) transportType = dep["product"];
    bool sev = dep["sev"] | false;

    // Linienauswahl: nur gewaehlte Linien (mit Richtung). Ersatzbusse fuer
    // S-Bahn/Tram tragen die Linie als Label und passen direkt. Ersatzbusse
    // fuer Regionalzuege tragen eine Zugnummer - die API sagt nicht, welche
    // Linie ersetzt wird; sie erscheinen, sobald irgendein Regionalzug
    // gewaehlt ist. Teile gekoppelter Regionalzuege (SPLIT_TRAIN_GROUPS)
    // bleiben vorerst drin: Ist ein anderer Zugteil gewaehlt, zeigt
    // Schritt 4 die ganze Zeile, sonst faellt die Fahrt in Schritt 5 weg.
    String key = lineKey(dep["label"]);
    bool selected = true;
    if (!lines.empty()) {
      selected = lines.matches(key, direction) ||
                 (sev && transportType != nullptr && strcmp(transportType, "BAHN") == 0 &&
                  lines.hasType('Z'));
      if (!selected && splitTrainGroup(key) < 0) continue;
    }

    RawEntry r;
    r.selected = selected;
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
    r.multiDest = false;

    // Verkehrsmittel erkennen (fuer das Liniensymbol). Der Feldname ist
    // nicht offiziell dokumentiert: "transportType" (bgw-pt/v3), als
    // Rueckfall "product" (aeltere API). Beobachtete Werte: SBAHN, UBAHN,
    // TRAM, BUS, REGIONAL_BUS, BAHN. Fehlt beides, wird das Icon nur
    // anhand des Labels gewaehlt.
    r.isBus = (transportType != nullptr && strstr(transportType, "BUS") != nullptr);

    // Regionalzuege: Label kommt als "RB 56" / "RE 5" - ohne Leerzeichen
    // passt es in die Pixelschrift der generierten Icons ("RB56")
    if (transportType != nullptr && strcmp(transportType, "BAHN") == 0) {
      r.line.replace(" ", "");
    }

    // Schienenersatzverkehr ("sev": true, beobachtet 03.10.2026): Ersatzbusse
    // fuer Zug, S-Bahn oder Tram. Das Label ist je nach Fall die Zugnummer
    // ("67116", Typ BAHN), die ersetzte Linie ("S2", Typ BUS) oder die
    // Tramnummer ("25", Typ BUS) - einheitlich als "SEV" im Bus-Rahmen.
    // Die API liefert Ersatzbusse beim ersetzten Verkehrsmittel mit
    // (transportTypes=SBAHN enthaelt den S2-Ersatzbus, TRAM den der Tram)
    if (sev) {
      r.line = "SEV";
      r.isBus = true;
    }

    // Gleis (nur S-/U-Bahn und Zuege) - fuer die Erkennung von Fluegelzuegen
    r.hasPlatform = dep["platform"].is<int>();
    r.platform = dep["platform"] | -1;

    // Nur diese beiden Typen gelten als fahrtrelevant und loesen ein
    // Warndreieck aus. Andere Typen (z.B. "INFO" - Tarifhinweise etc.)
    // werden bewusst ignoriert. Neue fahrtrelevante Typen hier ergaenzen.
    JsonArrayConst infos = dep["infos"];
    for (JsonObjectConst info : infos) {
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
  // ("Flugh./Freising", siehe SPLIT_TRAIN_LABELS), sonst alle Ziele nach
  // Linie sortiert mit "/" (beim Zeichnen gleichmaessig gekuerzt).
  // Regionalzuege aus SPLIT_TRAIN_GROUPS werden auch bei verschiedener
  // Linie zusammengefasst; die Zeile zeigt dann das Symbol ohne Nummer ("RB").
  // Verspaetung: die kleinste der Fahrten mit Echtzeit - die Teile fahren
  // gemeinsam ab, so kommt niemand zu spaet. Unterschiede sind meist nur
  // kurzzeitig (Echtzeit kommt je Zugteil zeitversetzt, geprueft 03.10.2026).
  // Haben mehrere Fahrten auch dasselbe Ziel (z.B. beide Zugteile einer S1
  // bei einer Stoerung nicht vereinigt, beobachtet 30.09.2026), bleibt nur
  // eine Zeile uebrig.
  // Die Richtung muss uebereinstimmen (relevant bei DIR_FILTER_ALL).
  // Bewusst NICHT zusammengefasst:
  // - Busse (kein Gleis; Verstaerkerbusse waeren sonst falsch vereint)
  // - Eintraege ohne Gleisangabe
  // - unterschiedlicher Ausfall-Status: faellt nur ein Zugteil aus, bleibt
  //   dieser als eigene, durchgestrichene Zeile sichtbar
  // Eine Warnung irgendeiner Fahrt gilt fuer die ganze Zeile.
  const int MAX_PARTS = 4;
  const int NO_DELAY = 10000;
  for (int i = 0; i < rawCount; i++) {
    if (raw[i].superseded || raw[i].isBus || !raw[i].hasPlatform) continue;

    int group = splitTrainGroup(raw[i].line);
    String partLine[MAX_PARTS];
    String partDest[MAX_PARTS];
    partLine[0] = raw[i].line;
    partDest[0] = raw[i].destination;
    int parts = 1;
    int duplicates = 0;
    bool mixedLines = false;
    int minRealtimeDelay = raw[i].realtime ? raw[i].delayMin : NO_DELAY;
    for (int j = i + 1; j < rawCount; j++) {
      if (raw[j].superseded || raw[j].isBus || !raw[j].hasPlatform) continue;
      bool sameLine = (raw[j].line == raw[i].line);
      if (!sameLine && (group < 0 || splitTrainGroup(raw[j].line) != group)) continue;
      if (raw[j].direction != raw[i].direction) continue;
      if (raw[j].plannedTime != raw[i].plannedTime) continue;
      if (raw[j].platform != raw[i].platform) continue;
      if (raw[j].cancelled != raw[i].cancelled) continue;

      // Ziel schon enthalten -> doppelte Fahrt, sonst weiterer Zugteil
      bool known = false;
      for (int k = 0; k < parts; k++) {
        if (partDest[k] == raw[j].destination) known = true;
      }
      if (known) {
        duplicates++;
      } else if (parts < MAX_PARTS) {
        partLine[parts] = raw[j].line;
        partDest[parts] = raw[j].destination;
        parts++;
      }

      if (!sameLine) mixedLines = true;
      if (raw[j].realtime && raw[j].delayMin < minRealtimeDelay) minRealtimeDelay = raw[j].delayMin;
      raw[i].hasWarning = raw[i].hasWarning || raw[j].hasWarning;
      raw[i].selected = raw[i].selected || raw[j].selected;
      raw[i].realtime = raw[i].realtime || raw[j].realtime;
      raw[j].superseded = true;
    }

    if (mixedLines) raw[i].line = SPLIT_TRAIN_GROUPS[group].icon;
    if (minRealtimeDelay != NO_DELAY) raw[i].delayMin = minRealtimeDelay;

    if (parts > 1) {
      // Ziele nach Linie sortieren (stabil), damit die Reihenfolge nicht
      // mit der Reihenfolge in der API-Antwort wechselt
      for (int a = 1; a < parts; a++) {
        for (int b = a; b > 0 && partLine[b] < partLine[b - 1]; b--) {
          String tmp = partLine[b]; partLine[b] = partLine[b - 1]; partLine[b - 1] = tmp;
          tmp = partDest[b]; partDest[b] = partDest[b - 1]; partDest[b - 1] = tmp;
        }
      }
      String fixed = (parts == 2) ? fixedSplitLabel(partDest[0], partDest[1]) : "";
      if (fixed.length() > 0) {
        raw[i].destination = fixed;
      } else {
        raw[i].destination = partDest[0];
        for (int k = 1; k < parts; k++) {
          raw[i].destination += "/";
          raw[i].destination += partDest[k];
        }
        raw[i].multiDest = true;
      }
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
    if (raw[i].superseded || !raw[i].selected) continue;

    Departure d;
    d.line = raw[i].line;
    d.destination = raw[i].destination;
    d.cancelled = raw[i].cancelled;
    d.hasWarning = raw[i].hasWarning;
    d.delayMin = raw[i].delayMin;
    d.realtime = raw[i].realtime;
    d.isBus = raw[i].isBus;
    d.multiDest = raw[i].multiDest;
    d.time = formatTime(raw[i].plannedTime);

    result[count] = d;
    count++;
  }

  return count;
}
