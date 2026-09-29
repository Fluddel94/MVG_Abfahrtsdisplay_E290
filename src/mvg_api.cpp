// mvg_api.cpp
// Abruf und Aufbereitung der Abfahrtsdaten von der MVG-API (siehe mvg_api.h).

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "../config.h"
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

// Wert fuer den API-Parameter "transportTypes" aus den SHOW_*-Schaltern
// (config.h). Ohne diesen Parameter liefert die API (beobachtet, nicht
// dokumentiert) nur SBAHN, UBAHN, TRAM und BUS - Regionalbusse
// (REGIONAL_BUS) und Regionalzuege (BAHN) fehlen dann. Die Werte sind
// empirisch ermittelt. Das fuehrende Komma wird per "+ 1" uebersprungen;
// mindestens ein Schalter ist 1 (#error in config.h).
static const char* const TRANSPORT_TYPES_PARAM = (""
#if SHOW_SBAHN
  ",SBAHN"
#endif
#if SHOW_UBAHN
  ",UBAHN"
#endif
#if SHOW_TRAM
  ",TRAM"
#endif
#if SHOW_BUS
  ",BUS,REGIONAL_BUS"
#endif
#if SHOW_BAHN
  ",BAHN"
#endif
  ) + 1;

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
  url += "&limit=20";
  url += "&transportTypes=";
  url += TRANSPORT_TYPES_PARAM;

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
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

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

  // --- Schritt 3: Fluegelzuege zusammenfassen ---
  // Ein Zug, der unterwegs geteilt wird (z.B. S1 -> Flughafen / Freising),
  // erscheint in der API als mehrere Fahrten mit gleicher Linie, gleicher
  // geplanter Zeit und gleichem Gleis, aber verschiedenem Ziel. Sie werden
  // zu einer Zeile zusammengefasst: bekannte Paarungen mit fester Kurzform
  // ("Flugh./Freising", siehe SPLIT_TRAIN_LABELS), sonst alle Ziele voll
  // ausgeschrieben mit "/" (Kuerzung beim Zeichnen per fitText()).
  // Die Richtung muss uebereinstimmen (relevant bei DIR_FILTER_ALL).
  // Bewusst NICHT zusammengefasst:
  // - Busse (kein Gleis; Verstaerkerbusse waeren sonst falsch vereint)
  // - Eintraege ohne Gleisangabe
  // - unterschiedlicher Ausfall-Status (ein Ausfall soll sichtbar bleiben)
  // - Fahrten mit vorzeitigem Ende (eigene Logik in Schritt 1/2)
  // Uebernommen wird jeweils der "schlechtere" Wert (Verspaetung, Warnung).
  for (int i = 0; i < rawCount; i++) {
    if (raw[i].superseded || raw[i].isBus || !raw[i].hasPlatform || raw[i].earlyTermination) continue;

    String firstDest = raw[i].destination;
    String secondDest;
    String allDests = firstDest;   // alle Ziele voll ausgeschrieben
    int parts = 1;
    for (int j = i + 1; j < rawCount; j++) {
      if (raw[j].superseded || raw[j].isBus || !raw[j].hasPlatform || raw[j].earlyTermination) continue;
      if (raw[j].line != raw[i].line) continue;
      if (raw[j].direction != raw[i].direction) continue;
      if (raw[j].plannedTime != raw[i].plannedTime) continue;
      if (raw[j].platform != raw[i].platform) continue;
      if (raw[j].cancelled != raw[i].cancelled) continue;
      if (raw[j].destination == raw[i].destination) continue;

      if (parts == 1) secondDest = raw[j].destination;
      allDests += "/";
      allDests += raw[j].destination;
      parts++;

      if (raw[j].delayMin > raw[i].delayMin) raw[i].delayMin = raw[j].delayMin;
      raw[i].hasWarning = raw[i].hasWarning || raw[j].hasWarning;
      raw[i].realtime = raw[i].realtime || raw[j].realtime;
      raw[j].superseded = true;
    }

    if (parts > 1) {
      // Zwei Ziele: ggf. feste Kurzform; mehr als zwei: immer ausgeschrieben
      raw[i].destination = (parts == 2) ? splitTrainDestination(firstDest, secondDest)
                                        : allDests;
      Serial.print("Fluegelzug zusammengefasst: ");
      Serial.print(raw[i].line);
      Serial.print(" ");
      Serial.println(raw[i].destination);
    }
  }

  // --- Schritt 4: finale Liste zusammenstellen ---
  int count = 0;
  for (int i = 0; i < rawCount; i++) {
    if (count >= maxResults) break;
    if (raw[i].superseded) continue;

    Departure d;
    d.line = raw[i].line;

    if (raw[i].earlyTermination) {
      // Kein Ersatzzug gefunden -> dieser Eintrag selbst wird gezeigt,
      // mit dem tatsaechlichen (verkuerzten) Ziel statt dem urspruenglichen
      d.destination = raw[i].actualDestination;
      d.cancelled = false;
      d.hasWarning = true;
    } else {
      d.destination = raw[i].destination;
      d.cancelled = raw[i].cancelled;
      d.hasWarning = raw[i].hasWarning;
    }

    d.delayMin = raw[i].delayMin;
    d.realtime = raw[i].realtime;
    d.isBus = raw[i].isBus;
    d.time = formatTime(raw[i].plannedTime);

    result[count] = d;
    count++;
  }

  return count;
}
