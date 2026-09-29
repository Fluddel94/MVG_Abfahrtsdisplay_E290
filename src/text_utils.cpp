// text_utils.cpp
// Textumwandlung fuer das Display (siehe text_utils.h).

#include <Arduino.h>
#include "text_utils.h"

// Ersatz fuer haeufige Zeichen ausserhalb von Latin-1 (Unicode-Codepoint)
static char replacementFor(uint32_t cp) {
  switch (cp) {
    case 0x2010: case 0x2011: case 0x2012:
    case 0x2013: case 0x2014: case 0x2212: return '-';   // Bindestriche, Minus
    case 0x2018: case 0x2019: case 0x201A: return '\'';  // einfache Anfuehrungszeichen
    case 0x201C: case 0x201D: case 0x201E: return '"';   // doppelte Anfuehrungszeichen
    case 0x2026: return '.';                             // Auslassungspunkte
    default: return '?';
  }
}

String utf8ToLatin1(const String& text) {
  String out;
  out.reserve(text.length());

  const unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len) {
    uint8_t b = (uint8_t)text[i];

    // ASCII unveraendert
    if (b < 0x80) {
      out += (char)b;
      i++;
      continue;
    }

    // Laenge der UTF-8-Sequenz aus dem Startbyte
    int extra;
    uint32_t cp;
    if ((b & 0xE0) == 0xC0)      { extra = 1; cp = b & 0x1F; }
    else if ((b & 0xF0) == 0xE0) { extra = 2; cp = b & 0x0F; }
    else if ((b & 0xF8) == 0xF0) { extra = 3; cp = b & 0x07; }
    else {
      // Ungueltiges Startbyte (z.B. einzelnes Folgebyte) -> ueberspringen
      out += '?';
      i++;
      continue;
    }

    // Folgebytes einsammeln (muessen 10xxxxxx sein)
    bool valid = (i + extra < len);
    for (int k = 1; valid && k <= extra; k++) {
      uint8_t c = (uint8_t)text[i + k];
      if ((c & 0xC0) != 0x80) valid = false;
      else cp = (cp << 6) | (c & 0x3F);
    }

    if (!valid) {
      out += '?';
      i++;
      continue;
    }

    if (cp >= 0xA0 && cp <= 0xFF) {
      out += (char)cp;              // Latin-1: direkt darstellbar
    } else {
      out += replacementFor(cp);
    }
    i += extra + 1;
  }
  return out;
}
