// improv_serial.cpp
// Improv Serial (siehe improv_serial.h). Eigene kleine Umsetzung des
// Protokolls (Version 1), keine zusaetzliche Library noetig.
//
// Paketaufbau: "IMPROV" | Version (1) | Typ | Laenge | Daten | Pruefsumme
// (Summe aller vorherigen Bytes, 8 Bit). Andere Ausgaben im seriellen
// Monitor stoeren nicht - der Browser sucht nach dem Kopf "IMPROV".
// Jedes Paket wird mit einem einzigen Serial.write() gesendet, damit
// Ausgaben aus loop() es nicht zerteilen.

#include <Arduino.h>
#include <WiFi.h>
#include "settings.h"
#include "extras.h"
#include "portal.h"
#include "improv_serial.h"

// ------------------------------------------------------------
// Protokoll-Konstanten
// ------------------------------------------------------------
static const uint8_t IMPROV_VERSION = 1;

static const uint8_t TYPE_CURRENT_STATE = 0x01;
static const uint8_t TYPE_ERROR_STATE = 0x02;
static const uint8_t TYPE_RPC = 0x03;
static const uint8_t TYPE_RPC_RESULT = 0x04;

static const uint8_t STATE_READY = 0x02;         // bereit, keine Freigabe noetig
static const uint8_t STATE_PROVISIONING = 0x03;  // Verbindungsversuch laeuft
static const uint8_t STATE_PROVISIONED = 0x04;   // mit WLAN verbunden

static const uint8_t ERROR_NONE = 0x00;
static const uint8_t ERROR_INVALID_RPC = 0x01;
static const uint8_t ERROR_UNKNOWN_RPC = 0x02;
static const uint8_t ERROR_UNABLE_TO_CONNECT = 0x03;

static const uint8_t CMD_WIFI_SETTINGS = 0x01;
static const uint8_t CMD_GET_STATE = 0x02;
static const uint8_t CMD_GET_INFO = 0x03;
static const uint8_t CMD_SCAN = 0x04;

// ------------------------------------------------------------
// Einstellungen des Moduls
// ------------------------------------------------------------
#define IMPROV_CONNECT_TIMEOUT_MS 20000UL   // Verbindungsversuch mit neuen Daten
#define IMPROV_MAX_NETWORKS 20              // hoechstens so viele Netze melden
#define IMPROV_TASK_STACK 6144
#define IMPROV_POLL_MS 20

// Geraeteinfo fuer den Browser
#define IMPROV_FIRMWARE_NAME "MVG Abfahrtsdisplay"
#define IMPROV_CHIP_FAMILY "ESP32-S3"
#define IMPROV_DEVICE_NAME "Abfahrtsdisplay"

// ------------------------------------------------------------
// Zustand
// ------------------------------------------------------------
static String firmwareVersionCopy;
static volatile bool connecting = false;   // Verbindungsversuch mit neuen Daten
static volatile bool scanning = false;     // Netzsuche

// Neue, erfolgreich getestete WLAN-Daten fuer improvLoop(). Der Task
// schreibt die Texte nur, solange pendingWifi false ist.
static volatile bool pendingWifi = false;
static String pendingSsid;
static String pendingPassword;

// ------------------------------------------------------------
// Senden
// ------------------------------------------------------------
static void sendPacket(uint8_t type, const uint8_t* data, uint8_t length) {
  uint8_t buf[9 + 255 + 2];
  memcpy(buf, "IMPROV", 6);
  buf[6] = IMPROV_VERSION;
  buf[7] = type;
  buf[8] = length;
  if (length > 0) memcpy(buf + 9, data, length);

  uint8_t checksum = 0;
  for (int i = 0; i < 9 + length; i++) checksum += buf[i];
  buf[9 + length] = checksum;
  buf[10 + length] = '\n';   // nur zur Lesbarkeit im seriellen Monitor

  // Ausgaben blockieren nicht (setTxTimeoutMs(0) in setup()). Kurz
  // wiederholen, falls der Sendepuffer gerade voll oder belegt ist - der
  // Browser liest waehrend der Einrichtung mit, der Puffer leert sich schnell.
  size_t total = 11 + length;
  size_t sent = 0;
  for (int tries = 0; sent < total && tries < 100; tries++) {
    size_t n = Serial.write(buf + sent, total - sent);
    sent += n;
    if (n == 0) vTaskDelay(pdMS_TO_TICKS(2));
  }
}

static void sendState(uint8_t state) {
  sendPacket(TYPE_CURRENT_STATE, &state, 1);
}

static void sendError(uint8_t error) {
  sendPacket(TYPE_ERROR_STATE, &error, 1);
}

// RPC-Ergebnis: Befehl, Laenge, dann Texte mit je einem Laengenbyte davor.
// Passt ein Text nicht mehr ins Paket, wird er (und alle folgenden) weggelassen.
static void sendResult(uint8_t command, const String* texts, int count) {
  uint8_t data[255];
  int pos = 2;
  for (int i = 0; i < count; i++) {
    int len = texts[i].length();
    if (len > 255 || pos + 1 + len > 255) break;
    data[pos++] = (uint8_t)len;
    memcpy(data + pos, texts[i].c_str(), len);
    pos += len;
  }
  data[0] = command;
  data[1] = (uint8_t)(pos - 2);
  sendPacket(TYPE_RPC_RESULT, data, (uint8_t)pos);
}

// ------------------------------------------------------------
// Befehle
// ------------------------------------------------------------
static uint8_t currentState() {
  if (connecting) return STATE_PROVISIONING;
  if (settingsHasWifi() && WiFi.status() == WL_CONNECTED) return STATE_PROVISIONED;
  return STATE_READY;
}

// Mit den gueltigen WLAN-Daten neu verbinden (nach Fehlversuch/Netzsuche):
// neue, noch nicht uebernommene Daten aus Improv, sonst die gespeicherten
static void restoreSavedWifi() {
  WiFi.disconnect();
  if (pendingWifi) {
    WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
  } else if (settingsHasWifi()) {
    String ssid, password;
    {
      SettingsLock lock;
      ssid = appSettings.wifiSsid;
      password = appSettings.wifiPassword;
    }
    WiFi.begin(ssid.c_str(), password.c_str());
  }
}

// Daten: SSID-Laenge, SSID, Passwort-Laenge, Passwort
static void handleWifiSettings(const uint8_t* data, uint8_t length) {
  if (length < 2 || pendingWifi) {
    sendError(ERROR_INVALID_RPC);
    return;
  }
  uint8_t ssidLen = data[0];
  if (ssidLen == 0 || 1 + ssidLen + 1 > length) {
    sendError(ERROR_INVALID_RPC);
    return;
  }
  uint8_t passLen = data[1 + ssidLen];
  if (1 + ssidLen + 1 + passLen > length) {
    sendError(ERROR_INVALID_RPC);
    return;
  }
  String ssid((const char*)data + 1, ssidLen);
  String password((const char*)data + 2 + ssidLen, passLen);

  connecting = true;
  sendError(ERROR_NONE);
  sendState(STATE_PROVISIONING);
  Serial.print("Improv: neue WLAN-Daten, verbinde mit ");
  Serial.println(ssid);

  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < IMPROV_CONNECT_TIMEOUT_MS) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Improv: verbunden, WLAN-Daten werden gespeichert");
    pendingSsid = ssid;
    pendingPassword = password;
    pendingWifi = true;
    connecting = false;
    sendState(STATE_PROVISIONED);
    // Adresse des Einstellungsportals ("Geraet oeffnen" im Browser). Das
    // Portal oeffnet improvLoop() gleich danach.
    String url = portalUrl();
    sendResult(CMD_WIFI_SETTINGS, &url, 1);
  } else {
    Serial.println("Improv: Verbindung fehlgeschlagen, alte WLAN-Daten bleiben");
    restoreSavedWifi();
    connecting = false;
    sendError(ERROR_UNABLE_TO_CONNECT);
    sendState(STATE_READY);
  }
}

static void handleGetState() {
  uint8_t state = currentState();
  sendState(state);
  if (state == STATE_PROVISIONED) {
    // Adresse nur, solange das Portal offen ist
    String url = portalUrl();
    sendResult(CMD_GET_STATE, &url, portalIsOpen() ? 1 : 0);
  }
}

static void handleGetInfo() {
  String texts[4] = {IMPROV_FIRMWARE_NAME, firmwareVersionCopy,
                     IMPROV_CHIP_FAMILY, IMPROV_DEVICE_NAME};
  sendResult(CMD_GET_INFO, texts, 4);
}

// Sichtbare Netze: je Netz ein Ergebnis (Name, Signal in dBm, "YES" wenn
// mit Passwort), staerkstes zuerst, jeder Name nur einmal. Ein leeres
// Ergebnis beendet die Liste.
static void handleScan() {
  scanning = true;
  int count = WiFi.scanNetworks();
  bool wasDisconnected = false;
  if (count < 0 && WiFi.status() != WL_CONNECTED) {
    // Suche scheitert, solange ein Verbindungsversuch laeuft (z.B. falsches
    // Passwort gespeichert) - dafuer kurz trennen
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    wasDisconnected = true;
    count = WiFi.scanNetworks();
  }

  if (count > 0) {
    // Reihenfolge nach Signalstaerke (einfache Auswahl, max. einige Dutzend Netze)
    bool* used = new bool[count]();
    int sent = 0;
    while (sent < IMPROV_MAX_NETWORKS) {
      int best = -1;
      for (int i = 0; i < count; i++) {
        if (!used[i] && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) best = i;
      }
      if (best < 0) break;
      used[best] = true;
      String ssid = WiFi.SSID(best);
      if (ssid.length() == 0) continue;   // verstecktes Netz
      bool duplicate = false;
      for (int i = 0; i < count; i++) {
        if (i != best && used[i] && WiFi.SSID(i) == ssid && WiFi.RSSI(i) >= WiFi.RSSI(best)) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;
      String texts[3] = {ssid, String(WiFi.RSSI(best)),
                         WiFi.encryptionType(best) == WIFI_AUTH_OPEN ? "NO" : "YES"};
      sendResult(CMD_SCAN, texts, 3);
      sent++;
    }
    delete[] used;
  }
  sendResult(CMD_SCAN, nullptr, 0);
  WiFi.scanDelete();

  if (wasDisconnected) restoreSavedWifi();
  scanning = false;
}

// ------------------------------------------------------------
// Empfangen
// ------------------------------------------------------------
static void handlePacket(const uint8_t* buf, uint8_t length) {
  uint8_t checksum = 0;
  for (int i = 0; i < 9 + length; i++) checksum += buf[i];
  if (checksum != buf[9 + length]) {
    sendError(ERROR_INVALID_RPC);
    return;
  }
  if (buf[7] != TYPE_RPC) return;   // andere Pakettypen sendet nur das Geraet

  const uint8_t* data = buf + 9;
  if (length < 2 || 2 + data[1] > length) {
    sendError(ERROR_INVALID_RPC);
    return;
  }
  uint8_t command = data[0];
  const uint8_t* args = data + 2;
  uint8_t argsLen = data[1];

  switch (command) {
    case CMD_WIFI_SETTINGS: handleWifiSettings(args, argsLen); break;
    case CMD_GET_STATE:     handleGetState(); break;
    case CMD_GET_INFO:      handleGetInfo(); break;
    case CMD_SCAN:          handleScan(); break;
    default:                sendError(ERROR_UNKNOWN_RPC); break;
  }
}

static void improvTask(void* param) {
  (void)param;
  static const char HEADER[] = "IMPROV";
  uint8_t buf[9 + 255 + 1];
  int pos = 0;

  for (;;) {
    while (Serial.available() > 0) {
      uint8_t b = (uint8_t)Serial.read();

      // Kopf "IMPROV" Zeichen fuer Zeichen pruefen
      if (pos < 6) {
        if (b == (uint8_t)HEADER[pos]) {
          buf[pos++] = b;
        } else {
          pos = 0;
          if (b == (uint8_t)HEADER[0]) buf[pos++] = b;
        }
        continue;
      }

      buf[pos++] = b;
      if (pos == 7 && b != IMPROV_VERSION) {
        pos = 0;
        continue;
      }
      if (pos >= 9 && pos == 9 + buf[8] + 1) {
        handlePacket(buf, buf[8]);
        pos = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(IMPROV_POLL_MS));
  }
}

// ------------------------------------------------------------
// Oeffentliche Funktionen
// ------------------------------------------------------------
void improvBegin(const char* firmwareVersion) {
  firmwareVersionCopy = firmwareVersion;
  xTaskCreate(improvTask, "improv", IMPROV_TASK_STACK, nullptr, 1, nullptr);
}

bool improvLoop() {
  if (!pendingWifi) return false;
  settingsSaveWifi(pendingSsid, pendingPassword);
  pendingWifi = false;
  extrasWifiChanged();
  return true;
}

bool improvBusy() {
  return connecting || scanning;
}
