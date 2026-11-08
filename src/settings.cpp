// settings.cpp
// Geraete-Einstellungen (siehe settings.h).
//
// Gespeichert wird im NVS-Bereich des Flash (Library Preferences aus dem
// ESP32-Boardpaket), Namensraum NVS_NAMESPACE. Der Bereich bleibt bei
// Firmware-Updates erhalten, solange das Partitionsschema gleich bleibt
// (partitions.csv im Sketch-Ordner).

#include <Arduino.h>
#include <Preferences.h>
#include "../config.h"
#include "settings.h"
#include "line_select.h"   // LineSelection: Linienauswahl pruefen

// secrets.h ist optional und wird NUR hier eingebunden (die Datei definiert
// globale Variablen). Andere Dateien greifen bei Bedarf per extern darauf zu.
#if __has_include("../secrets.h")
#include "../secrets.h"
#define HAS_SECRETS_H 1
#else
#define HAS_SECRETS_H 0
#endif

// Namensraum und Schluessel (max. 15 Zeichen). Nie umbenennen - sonst
// werden bereits gespeicherte Werte nicht mehr gefunden.
static const char* const NVS_NAMESPACE = "abfahrt";
static const char* const KEY_WIFI_SSID = "wifiSsid";
static const char* const KEY_WIFI_PASS = "wifiPass";
static const char* const KEY_STATION = "station";
static const char* const KEY_DIR_VIEW = "dirView";
static const char* const KEY_ZENTRUM_IS_H = "zentrumIsH";
static const char* const KEY_DEF_ZENTRUM = "defZentrum";
static const char* const KEY_SBAHN = "sbahn";
static const char* const KEY_UBAHN = "ubahn";
static const char* const KEY_TRAM = "tram";
static const char* const KEY_BUS = "bus";
static const char* const KEY_BAHN = "bahn";
static const char* const KEY_QR_ON = "qrOn";
static const char* const KEY_QR_TITLE = "qrTitle";
static const char* const KEY_QR_SSID = "qrSsid";
static const char* const KEY_QR_PASS = "qrPass";
// Ab 2.2.0 (Linienauswahl, zweite Station)
static const char* const KEY_LINES1 = "lines1";
static const char* const KEY_STATION2 = "station2";
static const char* const KEY_TYPES2 = "types2";
static const char* const KEY_LINES2 = "lines2";
static const char* const KEY_DIR1 = "dir1";
static const char* const KEY_DIR2 = "dir2";
// Ab 2.3.0 (Richtung je Station, Tastenbelegung, Minuten). dir1 kann jetzt
// auch 'U' sein; dirView/zentrumIsH/defZentrum/qrOn werden weiter
// geschrieben, damit eine aeltere Firmware nach einem Downgrade passt.
static const char* const KEY_ZENTRUM1 = "zentrum1";
static const char* const KEY_START1 = "start1";
static const char* const KEY_ZENTRUM2 = "zentrum2";
static const char* const KEY_KEY_LEFT = "keyLeft";
static const char* const KEY_KEY_MID = "keyMid";
static const char* const KEY_MINUTES = "minutes";
static const char* const KEY_AUTO_BACK = "autoBack";   // seit 2.4.0
// Ab 2.5.0 (Anzeige: Stoerungen, Full Refresh)
static const char* const KEY_SHOW_INC = "showInc";
static const char* const KEY_FULL_REFRESH = "fullRef";
static const char* const KEY_INC_TEXT = "incText";   // seit 2.6.0
static const char* const KEY_NAME1 = "name1";
static const char* const KEY_NAME2 = "name2";

DeviceSettings appSettings;

// Sperre fuer appSettings (rekursiv, damit verschachtelte Sperren gehen).
// Wird in settingsLoad() angelegt - vor dem Start der anderen Tasks.
static SemaphoreHandle_t settingsMutex = nullptr;

SettingsLock::SettingsLock() {
  if (settingsMutex) xSemaphoreTakeRecursive(settingsMutex, portMAX_DELAY);
}

SettingsLock::~SettingsLock() {
  if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
}

// true, wenn beim letzten settingsLoad() gespeicherte Werte gefunden wurden
static bool storedValuesFound = false;

// ------------------------------------------------------------
// Standardwerte und Pruefung
// ------------------------------------------------------------

static void setTransportDefaults(DeviceSettings& s) {
  s.showSbahn = SHOW_SBAHN;
  s.showUbahn = SHOW_UBAHN;
  s.showTram = SHOW_TRAM;
  s.showBus = SHOW_BUS;
  s.showBahn = SHOW_BAHN;
}

// Verkehrsmittel-Standard aus config.h als TYPE_...-Bits
static uint8_t defaultTypes() {
  return (SHOW_SBAHN ? TYPE_SBAHN : 0) | (SHOW_UBAHN ? TYPE_UBAHN : 0) |
         (SHOW_TRAM ? TYPE_TRAM : 0) | (SHOW_BUS ? TYPE_BUS : 0) |
         (SHOW_BAHN ? TYPE_BAHN : 0);
}

// Linienauswahl einlesen und normalisiert zurueckgeben (verwirft
// ungueltige Eintraege, hoechstens LINE_SELECT_MAX)
static String normalizeLines(const String& text) {
  LineSelection sel;
  sel.parse(text);
  return sel.toString();
}

// Tastenbelegung wie bis 2.2.0: links Station 2, sonst Richtung (bei
// umschaltbarer Richtung), sonst Seite 2; Mitte WLAN-QR, falls an
static void setKeyDefaults(DeviceSettings& s, bool wifiQr) {
  s.keyLeft = s.station2Id.length() ? BTN_STATION2
              : s.dir1 == 'U'       ? BTN_DIRECTION
                                    : BTN_PAGE2;
  s.keyMid = wifiQr ? BTN_WIFI_QR : BTN_PAGE2;
}

static void setDefaults(DeviceSettings& s) {
#if HAS_SECRETS_H
  s.wifiSsid = ssid;
  s.wifiPassword = password;
  s.qrSsid = qrWlanSsid;
  s.qrPassword = qrWlanPassword;
#else
  s.wifiSsid = "";
  s.wifiPassword = "";
  s.qrSsid = "";
  s.qrPassword = "";
#endif
  s.stationId = STATION_GLOBAL_ID;
  s.name1 = "";
  s.name2 = "";
  s.lines1 = "";
  s.station2Id = STATION2_GLOBAL_ID;
  s.dir1 = (FEATURE_DIRECTION_VIEW && s.station2Id.length() == 0) ? 'U' : 'B';
  s.zentrum1 = ZENTRUM_IS_H ? 'H' : 'R';
  s.start1 = (ZENTRUM_IS_H == DEFAULT_VIEW_ZENTRUM) ? 'H' : 'R';
  s.types2 = defaultTypes();
  s.lines2 = "";
  s.dir2 = 'B';
  s.zentrum2 = 'K';
  setTransportDefaults(s);
  s.qrTitle = QR_SCREEN_TITLE;
  setKeyDefaults(s, FEATURE_WIFI_QR);
  s.showMinutes = SHOW_MINUTES;
  s.viewAutoBack = VIEW_AUTO_BACK;
  s.showIncidents = SHOW_INCIDENTS;
  s.incidentText = SHOW_INCIDENT_TEXT;
  s.fullRefreshMin = FULL_REFRESH_EVERY;
}

static bool validKey(uint8_t key, bool middle) {
  return key <= BTN_PAGE2 || (middle && key == BTN_WIFI_QR);
}

// Taste mit einer Funktion, die es (nicht mehr) gibt - z.B. Station 2
// entfernt -: dann weitere Abfahrten (Seite 2)
static uint8_t usableKey(const DeviceSettings& s, uint8_t key, bool middle) {
  if (!validKey(key, middle)) return BTN_PAGE2;
  if (key == BTN_DIRECTION && s.dir1 != 'U') return BTN_PAGE2;
  if (key == BTN_STATION2 && s.station2Id.length() == 0) return BTN_PAGE2;
  return key;
}

// Korrigiert unbrauchbare Werte (z.B. alle Verkehrsmittel aus)
static void validate(DeviceSettings& s) {
  s.stationId.trim();
  if (!(s.showSbahn || s.showUbahn || s.showTram || s.showBus || s.showBahn)) {
    Serial.println("Einstellungen: kein Verkehrsmittel gewaehlt, nutze Standardwerte aus config.h");
    setTransportDefaults(s);
  }
  s.station2Id.trim();
  if (s.stationId.length() == 0) s.name1 = "";
  if (s.station2Id.length() == 0) s.name2 = "";
  s.types2 &= TYPE_ALL;
  if (s.types2 == 0) s.types2 = defaultTypes();
  s.lines1 = normalizeLines(s.lines1);
  s.lines2 = normalizeLines(s.lines2);
  if (s.dir1 != 'H' && s.dir1 != 'R' && s.dir1 != 'U') s.dir1 = 'B';
  if (s.dir2 != 'H' && s.dir2 != 'R') s.dir2 = 'B';
  if (s.zentrum1 != 'H' && s.zentrum1 != 'R') s.zentrum1 = 'K';
  if (s.zentrum2 != 'H' && s.zentrum2 != 'R') s.zentrum2 = 'K';
  if (s.start1 != 'R') s.start1 = 'H';
  s.keyLeft = usableKey(s, s.keyLeft, false);
  s.keyMid = usableKey(s, s.keyMid, true);
  if (s.fullRefreshMin < FULL_REFRESH_MIN) s.fullRefreshMin = FULL_REFRESH_MIN;
  if (s.fullRefreshMin > FULL_REFRESH_MAX) s.fullRefreshMin = FULL_REFRESH_MAX;
}

// ------------------------------------------------------------
// Lesen
// ------------------------------------------------------------

// Liest einen Wert nur, wenn er gespeichert ist - sonst bleibt der
// Standardwert stehen (isKey vermeidet Fehlermeldungen fuer fehlende Werte)
static void readString(Preferences& prefs, const char* key, String& value) {
  if (prefs.isKey(key)) value = prefs.getString(key, value);
}

static void readBool(Preferences& prefs, const char* key, bool& value) {
  if (prefs.isKey(key)) value = prefs.getBool(key, value);
}

static void readUChar(Preferences& prefs, const char* key, uint8_t& value) {
  if (prefs.isKey(key)) value = prefs.getUChar(key, value);
}

static void readChar(Preferences& prefs, const char* key, char& value) {
  if (prefs.isKey(key)) value = (char)prefs.getUChar(key, (uint8_t)value);
}

void settingsLoad() {
  if (!settingsMutex) settingsMutex = xSemaphoreCreateRecursiveMutex();
  SettingsLock lock;
  setDefaults(appSettings);
  storedValuesFound = false;

  Preferences prefs;
  // Nur lesen. false = Namensraum existiert noch nicht (nie gespeichert)
  if (prefs.begin(NVS_NAMESPACE, true)) {
    storedValuesFound = true;
    DeviceSettings& s = appSettings;
    readString(prefs, KEY_WIFI_SSID, s.wifiSsid);
    readString(prefs, KEY_WIFI_PASS, s.wifiPassword);
    readString(prefs, KEY_STATION, s.stationId);
    // Bis 2.2.0: getrennte Ansicht als eigener Schalter (nur Station 1,
    // nur ohne Station 2), Zentrum/Standardansicht als Ja/Nein
    bool dirView = FEATURE_DIRECTION_VIEW;
    bool zentrumIsH = ZENTRUM_IS_H;
    bool defZentrum = DEFAULT_VIEW_ZENTRUM;
    bool wifiQr = FEATURE_WIFI_QR;
    readBool(prefs, KEY_DIR_VIEW, dirView);
    readBool(prefs, KEY_ZENTRUM_IS_H, zentrumIsH);
    readBool(prefs, KEY_DEF_ZENTRUM, defZentrum);
    readBool(prefs, KEY_SBAHN, s.showSbahn);
    readBool(prefs, KEY_UBAHN, s.showUbahn);
    readBool(prefs, KEY_TRAM, s.showTram);
    readBool(prefs, KEY_BUS, s.showBus);
    readBool(prefs, KEY_BAHN, s.showBahn);
    readBool(prefs, KEY_QR_ON, wifiQr);
    readString(prefs, KEY_QR_TITLE, s.qrTitle);
    readString(prefs, KEY_QR_SSID, s.qrSsid);
    readString(prefs, KEY_QR_PASS, s.qrPassword);
    // Fehlen bei Geraeten mit 2.1.0 oder aelter: dann gelten die
    // Standardwerte (keine Linienauswahl, keine Station 2)
    readString(prefs, KEY_LINES1, s.lines1);
    readString(prefs, KEY_STATION2, s.station2Id);
    readUChar(prefs, KEY_TYPES2, s.types2);
    readString(prefs, KEY_LINES2, s.lines2);
    s.dir1 = 'B';
    readChar(prefs, KEY_DIR1, s.dir1);
    readChar(prefs, KEY_DIR2, s.dir2);

    // Fehlen bei Geraeten mit 2.2.0 oder aelter: aus den alten Werten so
    // uebernehmen, dass Anzeige und Tasten gleich bleiben
    if (prefs.isKey(KEY_ZENTRUM1)) {
      if (dirView) s.dir1 = 'U';   // gespeichert als dir1 'B' + dirView
    } else {
      if (dirView && s.station2Id.length() == 0) s.dir1 = 'U';
      s.zentrum1 = s.dir1 == 'U' ? (zentrumIsH ? 'H' : 'R') : 'K';
      s.start1 = (zentrumIsH == defZentrum) ? 'H' : 'R';
      s.zentrum2 = 'K';
      setKeyDefaults(s, wifiQr);
    }
    readChar(prefs, KEY_ZENTRUM1, s.zentrum1);
    readChar(prefs, KEY_START1, s.start1);
    readChar(prefs, KEY_ZENTRUM2, s.zentrum2);
    readUChar(prefs, KEY_KEY_LEFT, s.keyLeft);
    readUChar(prefs, KEY_KEY_MID, s.keyMid);
    readBool(prefs, KEY_MINUTES, s.showMinutes);
    readBool(prefs, KEY_AUTO_BACK, s.viewAutoBack);
    readBool(prefs, KEY_SHOW_INC, s.showIncidents);
    readBool(prefs, KEY_INC_TEXT, s.incidentText);
    readUChar(prefs, KEY_FULL_REFRESH, s.fullRefreshMin);
    readString(prefs, KEY_NAME1, s.name1);
    readString(prefs, KEY_NAME2, s.name2);
    prefs.end();
  }

  validate(appSettings);
}

// ------------------------------------------------------------
// Speichern und Loeschen
// ------------------------------------------------------------

// putString liefert die Anzahl geschriebener Zeichen (0 bei Fehler; bei
// einem leeren Text ebenfalls 0, das zaehlt als Erfolg)
static bool writeString(Preferences& prefs, const char* key, const String& value) {
  return prefs.putString(key, value) == value.length();
}

static bool writeBool(Preferences& prefs, const char* key, bool value) {
  return prefs.putBool(key, value) == 1;
}

static bool writeUChar(Preferences& prefs, const char* key, uint8_t value) {
  return prefs.putUChar(key, value) == 1;
}

bool settingsSave() {
  SettingsLock lock;
  validate(appSettings);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;

  bool ok = true;
  ok &= writeString(prefs, KEY_WIFI_SSID, appSettings.wifiSsid);
  ok &= writeString(prefs, KEY_WIFI_PASS, appSettings.wifiPassword);
  ok &= writeString(prefs, KEY_STATION, appSettings.stationId);
  // Fuer aeltere Firmware (Downgrade): getrennte Ansicht wie bis 2.2.0
  bool dirView = appSettings.dir1 == 'U';
  bool zentrumIsH = appSettings.zentrum1 != 'R';
  ok &= writeBool(prefs, KEY_DIR_VIEW, dirView);
  ok &= writeBool(prefs, KEY_ZENTRUM_IS_H, zentrumIsH);
  ok &= writeBool(prefs, KEY_DEF_ZENTRUM, (appSettings.start1 == 'H') == zentrumIsH);
  ok &= writeBool(prefs, KEY_SBAHN, appSettings.showSbahn);
  ok &= writeBool(prefs, KEY_UBAHN, appSettings.showUbahn);
  ok &= writeBool(prefs, KEY_TRAM, appSettings.showTram);
  ok &= writeBool(prefs, KEY_BUS, appSettings.showBus);
  ok &= writeBool(prefs, KEY_BAHN, appSettings.showBahn);
  ok &= writeBool(prefs, KEY_QR_ON, appSettings.keyMid == BTN_WIFI_QR);
  ok &= writeString(prefs, KEY_QR_TITLE, appSettings.qrTitle);
  ok &= writeString(prefs, KEY_QR_SSID, appSettings.qrSsid);
  ok &= writeString(prefs, KEY_QR_PASS, appSettings.qrPassword);
  ok &= writeString(prefs, KEY_LINES1, appSettings.lines1);
  ok &= writeString(prefs, KEY_STATION2, appSettings.station2Id);
  ok &= writeUChar(prefs, KEY_TYPES2, appSettings.types2);
  ok &= writeString(prefs, KEY_LINES2, appSettings.lines2);
  // 2.2.0 kennt kein 'U' (dort: dirView)
  ok &= writeUChar(prefs, KEY_DIR1, (uint8_t)(dirView ? 'B' : appSettings.dir1));
  ok &= writeUChar(prefs, KEY_DIR2, (uint8_t)appSettings.dir2);
  ok &= writeUChar(prefs, KEY_ZENTRUM1, (uint8_t)appSettings.zentrum1);
  ok &= writeUChar(prefs, KEY_START1, (uint8_t)appSettings.start1);
  ok &= writeUChar(prefs, KEY_ZENTRUM2, (uint8_t)appSettings.zentrum2);
  ok &= writeUChar(prefs, KEY_KEY_LEFT, appSettings.keyLeft);
  ok &= writeUChar(prefs, KEY_KEY_MID, appSettings.keyMid);
  ok &= writeBool(prefs, KEY_MINUTES, appSettings.showMinutes);
  ok &= writeBool(prefs, KEY_AUTO_BACK, appSettings.viewAutoBack);
  ok &= writeBool(prefs, KEY_SHOW_INC, appSettings.showIncidents);
  ok &= writeBool(prefs, KEY_INC_TEXT, appSettings.incidentText);
  ok &= writeUChar(prefs, KEY_FULL_REFRESH, appSettings.fullRefreshMin);
  ok &= writeString(prefs, KEY_NAME1, appSettings.name1);
  ok &= writeString(prefs, KEY_NAME2, appSettings.name2);
  prefs.end();

  if (ok) storedValuesFound = true;
  Serial.println(ok ? "Einstellungen gespeichert" : "Fehler beim Speichern der Einstellungen");
  return ok;
}

bool settingsSaveWifi(const String& ssid, const String& password) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = writeString(prefs, KEY_WIFI_SSID, ssid) &&
            writeString(prefs, KEY_WIFI_PASS, password);
  prefs.end();

  if (ok) {
    SettingsLock lock;
    appSettings.wifiSsid = ssid;
    appSettings.wifiPassword = password;
    storedValuesFound = true;
  }
  Serial.println(ok ? "WLAN-Daten gespeichert" : "Fehler beim Speichern der WLAN-Daten");
  return ok;
}

bool settingsSaveStationNames(const String& name1, const String& name2) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = writeString(prefs, KEY_NAME1, name1) && writeString(prefs, KEY_NAME2, name2);
  prefs.end();
  if (ok) {
    SettingsLock lock;
    appSettings.name1 = name1;
    appSettings.name2 = name2;
  }
  Serial.println(ok ? "Stationsnamen gespeichert" : "Fehler beim Speichern der Stationsnamen");
  return ok;
}

bool settingsFactoryReset() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.clear();
  prefs.end();
  Serial.println(ok ? "Werkseinstellungen: gespeicherte Einstellungen geloescht"
                    : "Fehler beim Loeschen der Einstellungen");
  return ok;
}

// ------------------------------------------------------------
// Abfragen und Ausgabe
// ------------------------------------------------------------

bool settingsHasWifi() {
  return appSettings.wifiSsid.length() > 0;
}

bool settingsHasStation() {
  return appSettings.stationId.length() > 0;
}

bool settingsHasStation2() {
  return appSettings.station2Id.length() > 0;
}

StationConfig stationConfig(int index) {
  const DeviceSettings& s = appSettings;
  StationConfig c;
  if (index == 1) {
    c.id = s.station2Id;
    c.types = s.types2;
    c.lines = s.lines2;
    c.dir = s.dir2;
  } else {
    c.id = s.stationId;
    c.types = (s.showSbahn ? TYPE_SBAHN : 0) | (s.showUbahn ? TYPE_UBAHN : 0) |
              (s.showTram ? TYPE_TRAM : 0) | (s.showBus ? TYPE_BUS : 0) |
              (s.showBahn ? TYPE_BAHN : 0);
    c.lines = s.lines1;
    c.dir = s.dir1;
  }
  return c;
}

static const char* yesNo(bool value) {
  return value ? "ja" : "nein";
}

void settingsPrint() {
  const DeviceSettings& s = appSettings;
  Serial.println("--- Einstellungen ---");
  Serial.print("Gespeicherte Werte im Geraet: ");
  Serial.println(storedValuesFound ? "ja (haben Vorrang)" : "nein (Standardwerte)");
#if HAS_SECRETS_H
  // Dieser Text steht nur in Firmware, die mit secrets.h kompiliert wurde.
  // Vor dem Veroeffentlichen einer .bin danach suchen (darf nicht vorkommen).
  Serial.println("Hinweis: secrets.h eingebunden (Vorbelegung WLAN/WLAN-QR)");
#endif
  Serial.print("WLAN: ");
  Serial.print(s.wifiSsid.length() ? s.wifiSsid : String("(keins)"));
  Serial.print(", Passwort gesetzt: ");
  Serial.println(yesNo(s.wifiPassword.length() > 0));
  Serial.print("Station: ");
  Serial.print(s.stationId.length() ? s.stationId : String("(keine)"));
  if (s.name1.length()) {
    Serial.print(" (");
    Serial.print(s.name1);
    Serial.print(")");
  }
  Serial.println();
  Serial.print("Richtung: ");
  if (s.dir1 == 'U') {
    Serial.print("umschaltbar, Start ");
    Serial.print(s.start1);
  } else if (s.dir1 != 'B') {
    Serial.print("nur ");
    Serial.print(s.dir1);
  } else {
    Serial.print("alle");
  }
  Serial.print(", Zentrum: ");
  Serial.println(s.zentrum1 == 'K' ? String("keine Angabe") : String(s.zentrum1));
  Serial.print("Verkehrsmittel:");
  if (s.showSbahn) Serial.print(" S-Bahn");
  if (s.showUbahn) Serial.print(" U-Bahn");
  if (s.showTram) Serial.print(" Tram");
  if (s.showBus) Serial.print(" Bus");
  if (s.showBahn) Serial.print(" Regionalzug");
  Serial.println();
  Serial.print("Linien: ");
  Serial.println(s.lines1.length() ? s.lines1 : String("(alle)"));
  Serial.print("Station 2: ");
  if (s.station2Id.length()) {
    Serial.print(s.station2Id);
    if (s.name2.length()) {
      Serial.print(" (");
      Serial.print(s.name2);
      Serial.print(")");
    }
    Serial.print(", Verkehrsmittel-Bits 0x");
    Serial.print(s.types2, HEX);
    Serial.print(", Richtung ");
    Serial.print(s.dir2 == 'B' ? String("alle") : String(s.dir2));
    Serial.print(", Zentrum ");
    Serial.print(s.zentrum2 == 'K' ? String("keine Angabe") : String(s.zentrum2));
    Serial.print(", Linien: ");
    Serial.println(s.lines2.length() ? s.lines2 : String("(alle)"));
  } else {
    Serial.println("(keine)");
  }
  static const char* const KEY_NAMES[] = { "keine", "Richtung", "Station 2", "Seite 2", "WLAN-QR" };
  Serial.print("Tasten: links ");
  Serial.print(KEY_NAMES[s.keyLeft]);
  Serial.print(", Mitte ");
  Serial.print(KEY_NAMES[s.keyMid]);
  Serial.println(s.viewAutoBack ? ", Ansicht endet nach 30 s" : ", Ansicht bleibt");
  Serial.print("Abfahrtszeit: ");
  Serial.print(s.showMinutes ? "Minuten" : "Uhrzeit");
  Serial.print(!s.showIncidents ? ", Stoerungen aus"
               : s.incidentText ? ", Stoerungen mit Text" : ", Stoerungen als Symbol");
  Serial.print(", Full Refresh alle ");
  Serial.print(s.fullRefreshMin);
  Serial.println(" Min.");
  Serial.print("WLAN-QR: ");
  if (s.keyMid == BTN_WIFI_QR) {
    Serial.print("an, Titel \"");
    Serial.print(s.qrTitle);
    Serial.print("\", Netz ");
    Serial.print(s.qrSsid);
    Serial.print(", Passwort gesetzt: ");
    Serial.println(yesNo(s.qrPassword.length() > 0));
  } else {
    Serial.println("aus");
  }
  Serial.println("---------------------");
}
