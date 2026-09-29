// buttons.cpp
// Tastenauswertung (siehe buttons.h).
//
// Warum Interrupts: loop() ist waehrend eines E-Ink-Updates (1-3 s) oder
// eines API-Abrufs blockiert. Wurde die Taste in dieser Zeit kurz gedrueckt
// und wieder losgelassen, hat die reine Abfrage in loop() den Druck
// verpasst. Der Interrupt merkt sich den Druck, ausgewertet wird er beim
// naechsten loop()-Durchlauf.
//
// Entprellen: Ein Druck zaehlt nur, wenn die Taste vorher mindestens
// BUTTON_EDGE_STABLE_MS losgelassen war. Das Prellen beim Druecken und
// beim Loslassen erzeugt so keine zusaetzlichen Druecke.

#include <Arduino.h>
#include "../config.h"
#include "buttons.h"

// --- Vom Interrupt geschrieben ---
static volatile bool bootPressLatched = false;
static volatile unsigned long bootPressAt = 0;     // Zeitpunkt des Drucks
static volatile unsigned long bootLastEdge = 0;    // letzte Flanke (fuer Entprellung)

static volatile bool qrPressLatched = false;
static volatile unsigned long qrPressAt = 0;
static volatile unsigned long qrLastEdge = 0;

// --- Nur in loop() verwendet ---
static unsigned long bootLastAccepted = 0;
static bool bootAcceptedOnce = false;

static bool qrButtonHeld = false;
static unsigned long qrPressStartTime = 0;
static bool qrLongPressTriggered = false;
static unsigned long qrLastAccepted = 0;
static bool qrAcceptedOnce = false;

// Wird bei jeder Flanke (Druecken und Loslassen) aufgerufen. Ein Druck
// (LOW) zaehlt nur, wenn die Taste davor stabil losgelassen war.
static void IRAM_ATTR onBootEdge() {
  unsigned long now = millis();
  if (digitalRead(BOOT_BUTTON) == LOW && now - bootLastEdge >= BUTTON_EDGE_STABLE_MS) {
    bootPressLatched = true;
    bootPressAt = now;
  }
  bootLastEdge = now;
}

static void IRAM_ATTR onQrEdge() {
  unsigned long now = millis();
  if (digitalRead(QR_BUTTON) == LOW && now - qrLastEdge >= BUTTON_EDGE_STABLE_MS) {
    qrPressLatched = true;
    qrPressAt = now;
  }
  qrLastEdge = now;
}

// Holt einen gemerkten Druck ab. false, wenn keiner vorliegt, er zu alt ist
// (z.B. waehrend QR-/Log-Screen gedrueckt, die BOOT-Taste wird dort nicht
// ausgewertet) oder zu kurz nach dem letzten gezaehlten Druck kam.
static bool takeLatchedPress(volatile bool& latched, volatile unsigned long& pressAt,
                             unsigned long& lastAccepted, bool& acceptedOnce,
                             unsigned long& pressTimeOut) {
  noInterrupts();
  bool has = latched;
  unsigned long at = pressAt;
  latched = false;
  interrupts();

  if (!has) return false;
  if (millis() - at > BUTTON_LATCH_MAX_AGE_MS) return false;
  if (acceptedOnce && at - lastAccepted < BUTTON_DEBOUNCE_MS) return false;

  lastAccepted = at;
  acceptedOnce = true;
  pressTimeOut = at;
  return true;
}

void buttonsInit() {
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(QR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), onBootEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(QR_BUTTON), onQrEdge, CHANGE);
}

void handleQrButton(void (*onShortPress)(), void (*onLongPress)()) {
  unsigned long pressAt;
  if (!qrButtonHeld &&
      takeLatchedPress(qrPressLatched, qrPressAt, qrLastAccepted, qrAcceptedOnce, pressAt)) {
    qrButtonHeld = true;
    qrPressStartTime = pressAt;   // Haltedauer ab dem echten Druck messen
    qrLongPressTriggered = false;
  }
  if (!qrButtonHeld) return;

  bool pressedNow = (digitalRead(QR_BUTTON) == LOW);

  if (pressedNow && !qrLongPressTriggered &&
      millis() - qrPressStartTime >= LONG_PRESS_MS) {
    // Schwelle erreicht, waehrend die Taste noch gehalten wird
    qrLongPressTriggered = true;
    onLongPress();
  }

  // Losgelassen erst, wenn der Pin stabil HIGH ist (nicht mitten im Prellen)
  noInterrupts();
  unsigned long lastEdge = qrLastEdge;
  interrupts();
  bool releasedStable = !pressedNow && millis() - lastEdge >= BUTTON_EDGE_STABLE_MS;

  if (releasedStable) {
    qrButtonHeld = false;
    if (!qrLongPressTriggered) {
      onShortPress();
    }
    qrLongPressTriggered = false;
  }
}

void handleBootButton(void (*onPress)()) {
  unsigned long pressAt;
  if (takeLatchedPress(bootPressLatched, bootPressAt, bootLastAccepted, bootAcceptedOnce, pressAt)) {
    onPress();
  }
}
