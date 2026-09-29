// text_utils.h
// Textumwandlung fuer das Display. Die Display-Schriften (FreeSans9pt8b,
// FreeSansBold9pt8b) sind Latin-1-kodiert (1 Byte pro Zeichen, 0x20-0xFF),
// Texte aus der MVG-API und aus secrets.h sind UTF-8.
#pragma once
#include <Arduino.h>

// Wandelt UTF-8 in Latin-1 um: Umlaute, ss, Akzente usw. werden zu einem
// Byte (z.B. "ü" -> 0xFC). Typografische Striche/Anfuehrungszeichen werden
// durch ASCII ersetzt, alle anderen nicht darstellbaren Zeichen durch "?".
// Ergebnis nur fuer die Anzeige verwenden - im seriellen Monitor erscheinen
// Nicht-ASCII-Zeichen danach als Ersatzzeichen.
String utf8ToLatin1(const String& text);
