// buttons.h
// Tastenauswertung: Entprellen, Kurz-/Lang-Druck-Erkennung.
// Was eine Taste ausloest, entscheidet der Aufrufer ueber Callbacks.
#pragma once

// Pins der Tasten konfigurieren (einmalig in setup())
void buttonsInit();

// QR-Taste auswerten. Kurzer Druck: onShortPress beim Loslassen.
// Langer Druck (>= LONG_PRESS_MS): onLongPress, sobald die Schwelle bei
// noch gehaltener Taste erreicht ist.
void handleQrButton(void (*onShortPress)(), void (*onLongPress)());

// BOOT-Taste auswerten: onPress bei Druck (entprellt). Bei gehaltener Taste
// wird nach Ablauf der Entprellzeit erneut ausgeloest.
void handleBootButton(void (*onPress)());
