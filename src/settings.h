// settings.h
// Geraete-Einstellungen zur Laufzeit (WLAN, Station, Anzeige,
// Verkehrsmittel, WLAN-QR). Gespeicherte Werte im Geraetespeicher (NVS)
// haben Vorrang. Fehlt ein Wert, gilt der Standardwert aus config.h bzw.
// secrets.h (optional).
#pragma once
#include <Arduino.h>

struct DeviceSettings {
  // WLAN, mit dem sich das Board verbindet ("" = keine WLAN-Daten)
  String wifiSsid;
  String wifiPassword;

  // Station 1 als globalId, z.B. "de:09162:2" ("" = noch keine Station)
  String stationId;
  // Linienauswahl Station 1 (Textform siehe LineSelection in
  // line_select.h, "" = alle Linien)
  String lines1;

  // Optionale Station 2 ("" = keine). Umschalten per BOOT-Taste.
  String station2Id;
  uint8_t types2;            // Verkehrsmittel Station 2 (TYPE_...-Bits)
  String lines2;             // Linienauswahl Station 2

  // Richtungsanzeige (nur Station 1 und nur ohne Station 2)
  bool directionView;        // true = getrennt nach Zentrum/Auswaerts, false = gemischt
  bool zentrumIsH;           // true = ":H:" faehrt Richtung Zentrum
  bool defaultViewZentrum;   // Standardansicht bei getrennter Anzeige

  // Verkehrsmittel Station 1 (mindestens eines ist true)
  bool showSbahn;
  bool showUbahn;
  bool showTram;
  bool showBus;              // Stadt- und Regionalbusse
  bool showBahn;             // Regionalzuege

  // WLAN-QR-Code (kurzer Druck auf die QR-Taste)
  bool wifiQr;
  String qrTitle;            // UTF-8, Umwandlung fuers Display erst beim Zeichnen
  String qrSsid;
  String qrPassword;
};

// Verkehrsmittel als Bits (StationConfig.types, DeviceSettings.types2)
#define TYPE_SBAHN 0x01
#define TYPE_UBAHN 0x02
#define TYPE_TRAM  0x04
#define TYPE_BUS   0x08   // Stadt- und Regionalbusse
#define TYPE_BAHN  0x10   // Regionalzuege
#define TYPE_ALL   0x1F

// Was fuer den Abruf einer Station gebraucht wird (Station 1 oder 2)
struct StationConfig {
  String id;       // globalId ("" = nicht eingerichtet)
  uint8_t types;   // TYPE_...-Bits, mindestens eins gesetzt
  String lines;    // Linienauswahl ("" = alle Linien)
};

// Die aktuell gueltigen Einstellungen (nach settingsLoad()). Geaendert wird
// appSettings nur im loop-Kontext (setup/loop). Andere Tasks (Portal,
// Improv) lesen nur und halten dabei eine SettingsLock.
extern DeviceSettings appSettings;

// Sperre fuer appSettings, solange das Objekt existiert (verschachtelbar):
//   { SettingsLock lock; ... appSettings lesen/aendern ... }
class SettingsLock {
 public:
  SettingsLock();
  ~SettingsLock();
  SettingsLock(const SettingsLock&) = delete;
  SettingsLock& operator=(const SettingsLock&) = delete;
};

// Standardwerte setzen und gespeicherte Werte darueber laden. Einmal ganz
// am Anfang von setup() aufrufen.
void settingsLoad();

// Alle Einstellungen aus appSettings speichern. true bei Erfolg.
bool settingsSave();

// Nur die WLAN-Daten speichern (z.B. aus der Einrichtung per Browser), die
// uebrigen Werte bleiben unveraendert. true bei Erfolg.
bool settingsSaveWifi(const String& ssid, const String& password);

// Alle gespeicherten Werte loeschen (Werkseinstellungen). Wirksam nach
// einem Neustart. true bei Erfolg.
bool settingsFactoryReset();

bool settingsHasWifi();      // WLAN-Name vorhanden
bool settingsHasStation();   // Station (1) vorhanden
bool settingsHasStation2();  // zweite Station eingerichtet

// Abrufdaten der Station index (0 = Station 1, 1 = Station 2) aus
// appSettings. Aufrufer ausserhalb des loop-Kontexts halten eine SettingsLock.
StationConfig stationConfig(int index);

// Einstellungen im seriellen Monitor ausgeben (Passwoerter nie im Klartext)
void settingsPrint();
