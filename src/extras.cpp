// extras.cpp
// Leere Standardfassungen der Andockstellen (siehe extras.h). "weak":
// Definiert eine andere Datei dieselbe Funktion, wird deren Fassung
// verwendet. Diese Datei nicht fuer eigene Erweiterungen aendern - sonst
// gibt es bei Updates Konflikte. Stattdessen eine eigene .cpp in src/ anlegen.

#include "extras.h"

__attribute__((weak)) void extrasBegin() {}
__attribute__((weak)) void extrasSetup(const char* firmwareVersion) { (void)firmwareVersion; }
__attribute__((weak)) void extrasLoop() {}
__attribute__((weak)) void extrasStatus(bool hasError) { (void)hasError; }
__attribute__((weak)) bool extrasHandlesWifiReconnect() { return false; }
__attribute__((weak)) const char* extrasVersionSuffix() { return ""; }
