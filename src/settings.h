// settings.h
// Geraete-Einstellungen zur Laufzeit (WLAN, Station, Anzeige,
// Verkehrsmittel, Tasten, WLAN-QR). Gespeicherte Werte im Geraetespeicher (NVS)
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
  // Richtung Station 1: 'B' alle, 'U' umschaltbar (H und R getrennt, eine
  // Taste mit BTN_DIRECTION wechselt), 'H' nur ":H:", 'R' nur ":R:" - gilt
  // fuer alle Linien, zusaetzlich zur Linienauswahl
  char dir1;
  // Welche Kennung faehrt Richtung Zentrum: 'H', 'R' oder 'K' (keine
  // Zuordnung). Bestimmt den Richtungsnamen im Header: Zentrum/Auswaerts,
  // bei 'K' und dir1 'U' "H"/"R", sonst keiner.
  char zentrum1;
  // Startansicht bei dir1 'U': 'H' oder 'R' (Richtungskennung)
  char start1;
  // Stationsnamen wie von der MVG (UTF-8, "" = unbekannt, dann Abruf beim
  // Start). Speichert das Portal mit der Station, damit Display und Portal
  // den Namen ohne MVG-Abfrage kennen.
  String name1;
  String name2;

  // Optionale Station 2 ("" = keine), erreichbar ueber eine Taste mit
  // BTN_STATION2
  String station2Id;
  uint8_t types2;            // Verkehrsmittel Station 2 (TYPE_...-Bits)
  String lines2;             // Linienauswahl Station 2
  char dir2;                 // Richtung Station 2: 'B', 'H', 'R' (kein 'U')
  char zentrum2;             // wie zentrum1

  // Tastenbelegung (BTN_...): Jede Taste zeigt ihre Ansicht, dieselbe
  // Taste erneut oder der Auto-Reset fuehrt zur Standardansicht zurueck,
  // die andere Taste zeigt direkt ihre Ansicht. BTN_WIFI_QR nur auf der
  // mittleren Taste.
  uint8_t keyLeft;
  uint8_t keyMid;
  // Auto-Reset: Ansicht einer Taste nach 30 s beenden (sonst bleibt sie)
  bool viewAutoBack;

  // Abfahrtszeit als Minuten bis zur Abfahrt ("4 min") statt Uhrzeit
  bool showMinutes;

  // Verkehrsmittel Station 1 (mindestens eines ist true)
  bool showSbahn;
  bool showUbahn;
  bool showTram;
  bool showBus;              // Stadt- und Regionalbusse
  bool showBahn;             // Regionalzuege

  // WLAN-QR-Code (mittlere Taste mit BTN_WIFI_QR)
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

// Tastenbelegung (DeviceSettings.keyLeft/keyMid)
#define BTN_NONE      0   // keine Funktion
#define BTN_DIRECTION 1   // Richtung wechseln (nur bei dir1 'U')
#define BTN_STATION2  2   // Station 2 zeigen (nur mit Station 2)
#define BTN_PAGE2     3   // weitere Abfahrten (Seite 2 der Standardansicht)
#define BTN_WIFI_QR   4   // WLAN-QR-Code (nur mittlere Taste)

// Was fuer den Abruf einer Station gebraucht wird (Station 1 oder 2)
struct StationConfig {
  String id;       // globalId ("" = nicht eingerichtet)
  uint8_t types;   // TYPE_...-Bits, mindestens eins gesetzt
  String lines;    // Linienauswahl ("" = alle Linien)
  char dir;        // Richtung: 'B' alle, 'U' H und R getrennt (nur Station 1),
                   // 'H', 'R' (zusaetzlich zu lines)
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

// Nur die Stationsnamen speichern (nach einem Abruf beim Start, wenn noch
// keine gespeichert waren). true bei Erfolg.
bool settingsSaveStationNames(const String& name1, const String& name2);

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
