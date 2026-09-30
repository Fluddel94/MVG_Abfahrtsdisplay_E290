// improv_serial.h
// Improv Serial: WLAN-Daten per USB aus dem Browser eingeben (Web-Installer:
// "WLAN verbinden" bzw. "WLAN aendern"). Protokoll: improv-wifi.com/serial
//
// Laeuft in einem eigenen FreeRTOS-Task, damit das Geraet auch antwortet,
// waehrend loop() durch ein Display-Update oder einen API-Abruf blockiert
// ist. Den Verbindungsversuch mit den neuen Daten macht der Task selbst;
// gespeichert und uebernommen werden sie erst in improvLoop() (loop-Kontext),
// damit appSettings nie aus zwei Tasks gleichzeitig geaendert wird.
#pragma once

// Startet den Improv-Task (einmal frueh in setup(), nach settingsLoad()).
// firmwareVersion wird fuer die Geraeteinfo gemeldet (wird kopiert).
void improvBegin(const char* firmwareVersion);

// Regelmaessig aus setup()-Warteschleifen und loop() aufrufen: speichert
// erfolgreich getestete neue WLAN-Daten und meldet sie an Erweiterungen.
// true = neue WLAN-Daten wurden gerade uebernommen (dann Portal oeffnen).
bool improvLoop();

// true, solange Improv das WLAN benutzt (Verbindungsversuch oder Netzsuche).
// Dann kein eigenes WiFi.reconnect() aufrufen.
bool improvBusy();
