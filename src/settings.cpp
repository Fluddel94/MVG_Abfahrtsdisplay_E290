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

DeviceSettings appSettings;

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
  s.directionView = FEATURE_DIRECTION_VIEW;
  s.zentrumIsH = ZENTRUM_IS_H;
  s.defaultViewZentrum = DEFAULT_VIEW_ZENTRUM;
  setTransportDefaults(s);
  s.wifiQr = FEATURE_WIFI_QR;
  s.qrTitle = QR_SCREEN_TITLE;
}

// Korrigiert unbrauchbare Werte (z.B. alle Verkehrsmittel aus)
static void validate(DeviceSettings& s) {
  s.stationId.trim();
  if (!(s.showSbahn || s.showUbahn || s.showTram || s.showBus || s.showBahn)) {
    Serial.println("Einstellungen: kein Verkehrsmittel gewaehlt, nutze Standardwerte aus config.h");
    setTransportDefaults(s);
  }
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

void settingsLoad() {
  setDefaults(appSettings);
  storedValuesFound = false;

  Preferences prefs;
  // Nur lesen. false = Namensraum existiert noch nicht (nie gespeichert)
  if (prefs.begin(NVS_NAMESPACE, true)) {
    storedValuesFound = true;
    readString(prefs, KEY_WIFI_SSID, appSettings.wifiSsid);
    readString(prefs, KEY_WIFI_PASS, appSettings.wifiPassword);
    readString(prefs, KEY_STATION, appSettings.stationId);
    readBool(prefs, KEY_DIR_VIEW, appSettings.directionView);
    readBool(prefs, KEY_ZENTRUM_IS_H, appSettings.zentrumIsH);
    readBool(prefs, KEY_DEF_ZENTRUM, appSettings.defaultViewZentrum);
    readBool(prefs, KEY_SBAHN, appSettings.showSbahn);
    readBool(prefs, KEY_UBAHN, appSettings.showUbahn);
    readBool(prefs, KEY_TRAM, appSettings.showTram);
    readBool(prefs, KEY_BUS, appSettings.showBus);
    readBool(prefs, KEY_BAHN, appSettings.showBahn);
    readBool(prefs, KEY_QR_ON, appSettings.wifiQr);
    readString(prefs, KEY_QR_TITLE, appSettings.qrTitle);
    readString(prefs, KEY_QR_SSID, appSettings.qrSsid);
    readString(prefs, KEY_QR_PASS, appSettings.qrPassword);
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

bool settingsSave() {
  validate(appSettings);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;

  bool ok = true;
  ok &= writeString(prefs, KEY_WIFI_SSID, appSettings.wifiSsid);
  ok &= writeString(prefs, KEY_WIFI_PASS, appSettings.wifiPassword);
  ok &= writeString(prefs, KEY_STATION, appSettings.stationId);
  ok &= writeBool(prefs, KEY_DIR_VIEW, appSettings.directionView);
  ok &= writeBool(prefs, KEY_ZENTRUM_IS_H, appSettings.zentrumIsH);
  ok &= writeBool(prefs, KEY_DEF_ZENTRUM, appSettings.defaultViewZentrum);
  ok &= writeBool(prefs, KEY_SBAHN, appSettings.showSbahn);
  ok &= writeBool(prefs, KEY_UBAHN, appSettings.showUbahn);
  ok &= writeBool(prefs, KEY_TRAM, appSettings.showTram);
  ok &= writeBool(prefs, KEY_BUS, appSettings.showBus);
  ok &= writeBool(prefs, KEY_BAHN, appSettings.showBahn);
  ok &= writeBool(prefs, KEY_QR_ON, appSettings.wifiQr);
  ok &= writeString(prefs, KEY_QR_TITLE, appSettings.qrTitle);
  ok &= writeString(prefs, KEY_QR_SSID, appSettings.qrSsid);
  ok &= writeString(prefs, KEY_QR_PASS, appSettings.qrPassword);
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
    appSettings.wifiSsid = ssid;
    appSettings.wifiPassword = password;
    storedValuesFound = true;
  }
  Serial.println(ok ? "WLAN-Daten gespeichert" : "Fehler beim Speichern der WLAN-Daten");
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
  Serial.println(s.stationId.length() ? s.stationId : String("(keine)"));
  Serial.print("Anzeige: ");
  if (s.directionView) {
    Serial.print("getrennt, Zentrum = ");
    Serial.print(s.zentrumIsH ? ":H:" : ":R:");
    Serial.print(", Standardansicht ");
    Serial.println(s.defaultViewZentrum ? "Zentrum" : "Auswaerts");
  } else {
    Serial.println("gemischt");
  }
  Serial.print("Verkehrsmittel:");
  if (s.showSbahn) Serial.print(" S-Bahn");
  if (s.showUbahn) Serial.print(" U-Bahn");
  if (s.showTram) Serial.print(" Tram");
  if (s.showBus) Serial.print(" Bus");
  if (s.showBahn) Serial.print(" Regionalzug");
  Serial.println();
  Serial.print("WLAN-QR: ");
  if (s.wifiQr) {
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
