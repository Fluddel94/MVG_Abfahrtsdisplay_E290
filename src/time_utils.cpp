// time_utils.cpp
// Hilfsfunktionen rund um Zeit (siehe time_utils.h).

#include <Arduino.h>
#include <time.h>
#include <esp_timer.h>
#include "../config.h"
#include "time_utils.h"

// Basis ist der 64-Bit-µs-Timer des ESP32 - im Gegensatz zu millis()
// (32 Bit, Ueberlauf nach ~49,7 Tagen) laeuft dieser praktisch nie ueber.
uint64_t uptimeMs() {
  return (uint64_t)esp_timer_get_time() / 1000ULL;
}

String formatUptime(uint64_t ms) {
  uint64_t totalSeconds = ms / 1000ULL;
  unsigned long days = (unsigned long)(totalSeconds / 86400ULL);
  unsigned long hours = (unsigned long)((totalSeconds % 86400ULL) / 3600ULL);
  unsigned long minutes = (unsigned long)((totalSeconds % 3600ULL) / 60ULL);

  String result = "";
  if (days > 0) {
    result += String(days) + "d ";
  }
  result += String(hours) + "h " + String(minutes) + "m";
  return result;
}

String formatDateTime(time_t t) {
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d %02d:%02d",
           timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
           timeinfo.tm_hour, timeinfo.tm_min);
  return String(buffer);
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:--";
  }
  char buffer[6];
  sprintf(buffer, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return String(buffer);
}

