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

  // Station als globalId, z.B. "de:09162:2" ("" = noch keine Station)
  String stationId;

  // Richtungsanzeige
  bool directionView;        // true = getrennt nach Zentrum/Auswaerts, false = gemischt
  bool zentrumIsH;           // true = ":H:" faehrt Richtung Zentrum
  bool defaultViewZentrum;   // Standardansicht bei getrennter Anzeige

  // Verkehrsmittel (mindestens eines ist true)
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
bool settingsHasStation();   // Station vorhanden

// Einstellungen im seriellen Monitor ausgeben (Passwoerter nie im Klartext)
void settingsPrint();
