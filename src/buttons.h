// buttons.h
// Tastenauswertung: Entprellen, Kurz-/Lang-Druck-Erkennung.
// Ein Tastendruck wird per Interrupt sofort gemerkt - auch waehrend das
// Display zeichnet oder die API abgefragt wird - und beim naechsten Aufruf
// der handle...-Funktionen ausgewertet. Was eine Taste ausloest, entscheidet
// der Aufrufer ueber Callbacks.
#pragma once

// Pins und Interrupts der Tasten einrichten (einmalig in setup())
void buttonsInit();

// QR-Taste auswerten. Kurzer Druck: onShortPress beim Loslassen (auch wenn
// Druck und Loslassen komplett waehrend eines Display-Updates lagen).
// Langer Druck (>= LONG_PRESS_MS): onLongPress, sobald die Schwelle bei
// noch gehaltener Taste erreicht ist.
void handleQrButton(void (*onShortPress)(), void (*onLongPress)());

// BOOT-Taste auswerten: onPress einmal pro Druck. Gedrueckt halten loest
// nicht erneut aus.
void handleBootButton(void (*onPress)());
