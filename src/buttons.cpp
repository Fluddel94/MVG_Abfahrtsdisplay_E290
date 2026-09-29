// buttons.cpp
// Tastenauswertung (siehe buttons.h).

#include <Arduino.h>
#include "../config.h"
#include "buttons.h"

static unsigned long lastBootButtonPress = 0;
static unsigned long lastQrButtonPress = 0;

// Kurz-/Lang-Druck-Erkennung fuer die QR-Taste
static bool qrButtonHeld = false;
static unsigned long qrPressStartTime = 0;
static bool qrLongPressTriggered = false;

void buttonsInit() {
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(QR_BUTTON, INPUT_PULLUP);
}

void handleQrButton(void (*onShortPress)(), void (*onLongPress)()) {
  bool qrPressedNow = (digitalRead(QR_BUTTON) == LOW);

  if (qrPressedNow && !qrButtonHeld) {
    // Taste wurde gerade gedrueckt
    if (millis() - lastQrButtonPress > BUTTON_DEBOUNCE_MS) {
      qrButtonHeld = true;
      qrPressStartTime = millis();
      qrLongPressTriggered = false;
    }
  }

  if (qrButtonHeld && qrPressedNow && !qrLongPressTriggered) {
    if (millis() - qrPressStartTime >= LONG_PRESS_MS) {
      // Schwelle erreicht, waehrend Taste noch gehalten wird
      qrLongPressTriggered = true;
      onLongPress();
      lastQrButtonPress = millis();
    }
  }

  if (!qrPressedNow && qrButtonHeld) {
    // Taste wurde losgelassen
    qrButtonHeld = false;
    if (!qrLongPressTriggered) {
      onShortPress();
      lastQrButtonPress = millis();
    }
    qrLongPressTriggered = false;
  }
}

void handleBootButton(void (*onPress)()) {
  if (digitalRead(BOOT_BUTTON) == LOW) {
    if (millis() - lastBootButtonPress > BUTTON_DEBOUNCE_MS) {
      onPress();
      lastBootButtonPress = millis();
    }
  }
}
