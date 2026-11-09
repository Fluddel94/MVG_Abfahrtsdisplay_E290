// display.cpp
// Alles, was auf das E-Ink-Display gezeichnet wird (siehe display.h).
// heltec-eink-modules wird nur hier eingebunden.

#include <Arduino.h>
#include "../config.h"
#include <WiFi.h>
#include <time.h>
// QR-Generator aus dem ESP32-Core (ESP-IDF-Komponente esp_qrcode) - keine
// zusaetzliche Library noetig
#include <qrcode.h>
#include <heltec-eink-modules.h>
#include <Fonts/FreeSansBold12pt7b.h>   // Uhrzeit und Titel Startbildschirm, 7-Bit
// Eigene 8-Bit-Schriften (Latin-1) fuer echte Umlaute, ASCII pixelgleich zu
// den Originalen. Texte vorher mit utf8ToLatin1() umwandeln (text_utils.h).
#include "FreeSansBold9pt8b.h"
#include "FreeSans9pt8b.h"
#include "FreeSansBold8pt8b.h"   // nur Stoerungszeile (eine Groesse kleiner)
#include "text_utils.h"
#include "line_icons.h"   // nach heltec-eink-modules (braucht BLACK/WHITE)
#include "time_utils.h"
#include "stats.h"
#include "display.h"

// --- Layout Abfahrtsliste (Pixelpositionen) ---
#define COL_ICON   3
#define ICON_Y_OFFSET 3
#define COL_DEST   48
#define COL_PIPE2  199
#define COL_TIME   212
#define DEST_MARGIN 5

// Rechts von COL_TIME muessen Uhrzeit ("88:88", 45 px) plus Leerzeichen und
// eine zweistellige Verspaetung in Fettschrift (" +99", 36 px) passen.
// Groessere Werte werden auf DELAY_MAX_ABS begrenzt (sonst Zeilenumbruch).
#define TIME_TEXT_WIDTH   45
#define DELAY_TEXT_WIDTH  36
#define DELAY_MAX_ABS     99
#define DISPLAY_WIDTH_PX  296   // Vision Master E290 quer
static_assert(COL_TIME + TIME_TEXT_WIDTH + DELAY_TEXT_WIDTH <= DISPLAY_WIDTH_PX - 2,
              "Verspaetung passt nicht mehr rechts neben die Uhrzeit");

// Das Ziel beginnt immer bei COL_DEST, egal wie breit das Liniensymbol ist.
// Das breiteste Symbol (S-Bahn, LINE_ICON_MAX_WIDTH) muss davor passen.
static_assert(COL_ICON + LINE_ICON_MAX_WIDTH < COL_DEST,
              "Liniensymbol ueberlappt die Zielspalte");

// --- Pfeil im Header ---
#define ARROW_LENGTH 12
#define ARROW_HEAD_SIZE 4
#define ARROW_GAP 5
#define ARROW_THICKNESS 2

// --- QR-Screen ---
// Hoechste erlaubte QR-Version: 10 = 57x57 Module, passt mit 2 px pro Modul
// in QR_AREA_SIZE und reicht fuer lange SSIDs/Passwoerter
#define QR_MAX_VERSION 10
#define QR_AREA_SIZE 116
#define QR_MARGIN 5

// --- Header ---
#define HEADER_CLOCK_GAP 6          // Mindestabstand Header-Text -> Uhr-Box
#define PAGE2_HINT "(2/2)"          // Seitenhinweis bei gemischter Anzeige
#define HEADER_HINT_SPACE 6         // Breite des Leerzeichens vor dem Seitenhinweis
#define STATION_BOX_SIZE 17         // Kaestchen mit Stationsnummer (zwei Stationen)
#define STATION_BOX_GAP 6           // Abstand Kaestchen -> Stationsname

// --- Startbildschirm ---
#define SPLASH_TITLE    "MVG Abfahrtsdisplay"   // 7-Bit-Schrift: keine Umlaute
#define SPLASH_SUBTITLE "Heltec Vision Master E290"
#define SPLASH_BAR_HEIGHT 38   // schwarzer Titelbalken oben

// --- Warndreieck (Stoerung, Ausfall) ---
// Steht immer an derselben Stelle direkt links vom Trennstrich, das Ziel
// wird in solchen Zeilen entsprechend kuerzer
#define WARNING_ICON_WIDTH 13
#define WARNING_ICON_X     189   // Mitte des Dreiecks (Spitze), 183..195
#define WARNING_DEST_END   183   // Ziel endet davor (plus DEST_MARGIN)
static_assert(WARNING_ICON_X + WARNING_ICON_WIDTH / 2 < COL_PIPE2,
              "Warndreieck ueberlappt den Trennstrich");

// --- Stoerungszeile (erste Zeile: Dreieck, "S2:", Stoerungstext) ---
#define INCIDENT_ICON_X      10   // Mitte des Dreiecks (2..18)
#define INCIDENT_TEXT_X      23   // Linie fett ab hier
#define INCIDENT_LABEL_GAP    4   // Abstand Linie -> Text
#define INCIDENT_RIGHT_MARGIN 3   // Text endet so weit vor dem rechten Rand

// --- Ausfall (durchgestrichene Uhrzeit) ---
#define STRIKE_OVERHANG 2        // Ueberstand des Strichs links und rechts

// --- Einstellungen/Info (mittlere Taste 3 s) ---
#define INFO_QR_SCALE 3          // Pixel je QR-Modul (kleiner als beim WLAN-QR)
#define INFO_QR_TOP 6
#define INFO_TEXT_GAP 6          // Abstand QR-Code -> Textspalte
#define INFO_LINE_HEIGHT 18

static EInkDisplay_VisionMasterE290 display;

// ============================================================
// Text-Hilfsfunktionen
// ============================================================

// Breite eines Textes in Pixeln mit der aktuell gesetzten Schrift
static uint16_t textWidth(String text) {
  int16_t bx, by;
  uint16_t bw, bh;
  display.getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
  return bw;
}

// Kuerzt einen Text mit "." am Ende, bis er in maxWidth passt. Ein
// Leerzeichen direkt vor dem "." wird entfernt ("Grosshesseloher.").
static String fitText(String text, int maxWidth) {
  String result = text;
  if (textWidth(result) <= maxWidth) return result;

  while (result.length() > 1 && textWidth(result + ".") > maxWidth) {
    result = result.substring(0, result.length() - 1);
  }
  result.trim();
  return result + ".";
}

// Wie fitText, aber mit "..." am Ende (Stoerungstext)
static String fitEllipsis(String text, int maxWidth) {
  if (textWidth(text) <= maxWidth) return text;
  while (text.length() > 1 && textWidth(text + "...") > maxWidth) {
    text = text.substring(0, text.length() - 1);
  }
  text.trim();
  return text + "...";
}

// Kuerzt mehrere Ziele "A/B/C" (Fluegelzug) gleichmaessig: Jedes Ziel wird
// auf hoechstens k Zeichen gekuerzt ("." am Ende), k sinkt, bis alles in
// maxWidth passt. Kurze Ziele bleiben so lange vollstaendig.
static String fitEvenly(const String& joined, int maxWidth) {
  if (textWidth(joined) <= maxWidth) return joined;

  const int MAX_PARTS = 4;
  String parts[MAX_PARTS];
  int count = 0;
  int start = 0;
  while (count < MAX_PARTS) {
    int slash = joined.indexOf('/', start);
    if (slash < 0 || count == MAX_PARTS - 1) {
      parts[count++] = joined.substring(start);
      break;
    }
    parts[count++] = joined.substring(start, slash);
    start = slash + 1;
  }

  int maxLen = 0;
  for (int i = 0; i < count; i++) {
    if ((int)parts[i].length() > maxLen) maxLen = parts[i].length();
  }

  for (int k = maxLen - 1; k >= 1; k--) {
    String result;
    for (int i = 0; i < count; i++) {
      String p = parts[i];
      if ((int)p.length() > k) {
        p = p.substring(0, k);
        p.trim();
        p += ".";
      }
      if (i > 0) result += "/";
      result += p;
    }
    if (textWidth(result) <= maxWidth) return result;
  }
  return fitText(joined, maxWidth);
}

// ============================================================
// Grafik-Elemente
// ============================================================

// Zeichnet einen Text waagerecht zentriert (aktuell gesetzte Schrift)
static void printCentered(const String& text, int yBaseline) {
  int16_t bx, by;
  uint16_t bw, bh;
  display.getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((display.width() - bw) / 2 - bx, yBaseline);
  display.print(text);
}

// Zeichnet ein kleines Warndreieck mit Ausrufezeichen.
// x = horizontale Mitte des Dreiecks, yBaseline = untere Kante (Textgrundlinie)
static void drawWarningTriangle(int x, int yBaseline) {
  int height = 10;
  int halfWidth = 6;
  int topY = yBaseline - height;
  int bottomY = yBaseline;

  display.fillTriangle(x, topY, x - halfWidth, bottomY, x + halfWidth, bottomY, BLACK);
  display.drawLine(x, topY + 3, x, bottomY - 3, WHITE);
  display.drawPixel(x, bottomY - 1, WHITE);
}

// Grosses Warndreieck fuer die Stoerungszeile (14 px hoch, 17 px breit,
// Ausrufezeichen 2 px dick). x = Mitte, yBaseline = untere Kante.
static void drawIncidentTriangle(int x, int yBaseline) {
  int height = 14;
  int halfWidth = 8;
  display.fillTriangle(x, yBaseline - height, x - halfWidth, yBaseline,
                       x + halfWidth, yBaseline, BLACK);
  display.fillRect(x - 1, yBaseline - 9, 2, 5, WHITE);   // Strich
  display.fillRect(x - 1, yBaseline - 3, 2, 2, WHITE);   // Punkt
}

// ============================================================
// Abfahrtsansicht
// ============================================================

// Zeichnet Header und Abfahrtsliste in den Displayspeicher (ohne
// display.update()). Header: Stationsname, bei dirLabel mit "-> Richtung"
// ('Z' Zentrum, 'A' Auswaerts, 'H'/'R' Kennung, 0 = keine), auf Seite 2
// mit Hinweis "(2/2)" (isPage2). Bei stationNumber 1/2 steht die Nummer
// weiss in einem schwarzen Kaestchen vor dem Namen. minutes: Abfahrtszeit
// als Minuten bis zur Abfahrt. incidents: Warndreieck bei Stoerung/Ausfall.
// incidentDep: Abfahrt, deren Stoerungstext die erste Zeile zeigt (die
// Abfahrten folgen dann ab Zeile 2), nullptr = keine Stoerungszeile.
static void drawContent(const Departure departures[], int found,
                        const String& stationName, char dirLabel,
                        bool isPage2, bool minutes, bool incidents,
                        int stationNumber, const Departure* incidentDep) {
  display.setTextColor(BLACK);

  display.setFont(&FreeSansBold12pt7b);

  String time = getCurrentTime();

  int16_t rbx, rby;
  uint16_t rbw, rbh;
  display.getTextBounds("88:88", 0, 0, &rbx, &rby, &rbw, &rbh);

  int boxPaddingX = 8;
  int boxPaddingY = 6;
  int boxW = rbw + boxPaddingX * 2;
  int boxH = rbh + boxPaddingY * 2;
  int boxX = display.width() - boxW - 5;
  int boxY = 2;

  display.drawRect(boxX, boxY, boxW, boxH, BLACK);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(time, 0, 0, &tbx, &tby, &tbw, &tbh);

  int timeCursorX = boxX + (boxW - tbw) / 2;
  int timeCursorY = boxY + boxPaddingY - tby;
  display.setCursor(timeCursorX, timeCursorY);
  display.print(time);

  int boxCenterY = boxY + boxH / 2;

  display.setFont(&FreeSansBold9pt8b);

  int headerRightLimit = boxX - HEADER_CLOCK_GAP;
  int availableWidth = headerRightLimit - 5;   // Header-Text beginnt bei x = 5

  // Header: [Nummer] Stationsname [-> Richtung] [(2/2)], muss vor der
  // Uhr-Box enden. Kuerzung:
  // 1. passt alles, Richtung in Langform ("Zentrum" / "Auswaerts")
  // 2. sonst Richtung in Kurzform ("Ztr." / "Ausw.")
  // 3. reicht auch das nicht, wird zusaetzlich der Stationsname gekuerzt
  // Nummer und Seitenhinweis bleiben immer vollstaendig sichtbar.
  int nameX = 5;
  if (stationNumber > 0) {
    // Kaestchen mit Nummer, vertikal mittig zur Uhr-Box
    int boxTop = boxCenterY - STATION_BOX_SIZE / 2;
    display.fillRect(5, boxTop, STATION_BOX_SIZE, STATION_BOX_SIZE, BLACK);
    String number(stationNumber);
    int16_t dbx, dby;
    uint16_t dbw, dbh;
    display.getTextBounds(number, 0, 0, &dbx, &dby, &dbw, &dbh);
    display.setTextColor(WHITE);
    display.setCursor(5 + (STATION_BOX_SIZE - dbw) / 2 - dbx,
                      boxTop + (STATION_BOX_SIZE - dbh) / 2 - dby);
    display.print(number);
    display.setTextColor(BLACK);
    nameX += STATION_BOX_SIZE + STATION_BOX_GAP;
    availableWidth -= STATION_BOX_SIZE + STATION_BOX_GAP;
  }

  if (isPage2) availableWidth -= HEADER_HINT_SPACE + textWidth(PAGE2_HINT);

  String suffix;
  if (dirLabel == 'Z') suffix = LABEL_ZENTRUM;
  else if (dirLabel == 'A') suffix = LABEL_AUSWAERTS;
  else if (dirLabel == 'H' || dirLabel == 'R') suffix = String(dirLabel);
  int arrowTotalWidth = ARROW_GAP + ARROW_LENGTH + ARROW_HEAD_SIZE + ARROW_GAP;

  String name = stationName;
  if (suffix.length() > 0) {
    if (textWidth(name) + arrowTotalWidth + textWidth(suffix) > availableWidth) {
      if (dirLabel == 'Z') suffix = LABEL_ZENTRUM_SHORT;
      if (dirLabel == 'A') suffix = LABEL_AUSWAERTS_SHORT;
    }
    name = fitText(name, availableWidth - arrowTotalWidth - textWidth(suffix));
  } else {
    name = fitText(name, availableWidth);
  }

  // Vertikale Position nur am Namen ausrichten (Klammern des Seitenhinweises
  // haben Unterlaengen und wuerden den Text verschieben)
  int16_t nbx, nby;
  uint16_t nbw, nbh;
  display.getTextBounds(name, 0, 0, &nbx, &nby, &nbw, &nbh);
  int headerCursorY = boxCenterY - nby - nbh / 2;

  display.setCursor(nameX, headerCursorY);
  display.print(name);
  int headerX = nameX + nbw;

  if (suffix.length() > 0) {
    int arrowStartX = headerX + ARROW_GAP;
    int arrowEndX = arrowStartX + ARROW_LENGTH;
    display.fillRect(arrowStartX, boxCenterY - ARROW_THICKNESS / 2, ARROW_LENGTH, ARROW_THICKNESS, BLACK);
    display.fillTriangle(
      arrowEndX, boxCenterY - ARROW_HEAD_SIZE,
      arrowEndX, boxCenterY + ARROW_HEAD_SIZE,
      arrowEndX + ARROW_HEAD_SIZE, boxCenterY,
      BLACK
    );
    int suffixStartX = arrowEndX + ARROW_HEAD_SIZE + ARROW_GAP;
    display.setCursor(suffixStartX, headerCursorY);
    display.print(suffix);
    headerX = suffixStartX + textWidth(suffix);
  }

  if (isPage2) {
    display.setCursor(headerX + HEADER_HINT_SPACE, headerCursorY);
    display.print(PAGE2_HINT);
  }

  int separatorY = boxY + boxH + 3;
  display.fillRect(0, separatorY, display.width(), 3, BLACK);

  int rowHeight = 22;
  int yStart = separatorY + 3 + 17;

  // Aktuelle Minute seit 1970 fuer die Minuten-Anzeige (0 = Uhrzeit zeigen,
  // auch solange die Zeit noch nicht abgeglichen ist)
  long nowMinute = 0;
  if (minutes) {
    time_t now = ::time(nullptr);   // "time" ist oben die Uhrzeit (String)
    if (now > 1700000000) nowMinute = (long)(now / 60);
  }

  // Stoerungszeile: Dreieck, Linie und Text in Fett 8 pt (Text mit "..."
  // gekuerzt). Die Abfahrten ruecken eine Zeile nach unten.
  int firstRow = 0;
  if (incidentDep != nullptr && found > 0) {
    int y = yStart;
    drawIncidentTriangle(INCIDENT_ICON_X, y);
    display.setFont(&FreeSansBold8pt8b);
    display.setCursor(INCIDENT_TEXT_X, y);
    display.print(incidentDep->line + ":");
    int textX = display.getCursorX() + INCIDENT_LABEL_GAP;
    display.setCursor(textX, y);
    display.print(fitEllipsis(incidentDep->incident,
                              display.width() - INCIDENT_RIGHT_MARGIN - textX));
    display.drawLine(0, y + 6, display.width(), y + 6, BLACK);
    firstRow = 1;
  }

  if (found == 0) {
    display.setFont(&FreeSans9pt8b);
    display.setCursor(5, yStart);
    display.print(isPage2 ? "Keine weiteren Abfahrten" : "Keine Abfahrten gefunden");
  } else {
    for (int i = 0; i < found; i++) {
      int y = yStart + (i + firstRow) * rowHeight;

      display.setFont(&FreeSans9pt8b);

      // Liniensymbol: S-/U-Bahn und Tram als Bitmap, Busse und unbekannte
      // Linien als generierter Rahmen mit Nummer (line_icons.h). Die Breite
      // variiert (max. 36 px), das Ziel beginnt trotzdem immer bei COL_DEST.
      drawLineIcon(display, COL_ICON, y - LINE_ICON_HEIGHT + ICON_Y_OFFSET,
                   departures[i].line, departures[i].isBus);
      // Der Text-Fallback fuer Bus-Icons setzt die Schrift zurueck
      display.setFont(&FreeSans9pt8b);
      display.setTextColor(BLACK);

      // Warndreieck bei Stoerung oder Ausfall (abschaltbar) an fester
      // Stelle links vom Trennstrich - das Ziel endet dann davor
      bool warning = incidents &&
                     (departures[i].hasWarning || departures[i].cancelled);

      int destMaxWidth = (warning ? WARNING_DEST_END : COL_PIPE2) - COL_DEST - DEST_MARGIN;

      String dest = departures[i].multiDest
                        ? fitEvenly(departures[i].destination, destMaxWidth)
                        : fitText(departures[i].destination, destMaxWidth);
      display.setCursor(COL_DEST, y);
      display.print(dest);

      if (warning) drawWarningTriangle(WARNING_ICON_X, y);

      display.setCursor(COL_PIPE2, y);
      display.print("|");

      // Verfruehung unter EARLY_DEPARTURE_MIN (config.h) wie puenktlich
      // behandeln - "-1" verwirrt und ist meist nur Rundung/Prognose
      int delay = departures[i].delayMin;
      if (delay < 0 && delay > -EARLY_DEPARTURE_MIN) delay = 0;

      // Minuten-Anzeige: Minuten bis zur tatsaechlichen Abfahrt (geplant +
      // Verspaetung), gerechnet als Differenz der Minuten wie die Uhr im
      // Header (14:07 -> 14:09 = "2 min"). Ab 60 Minuten Uhrzeit wie sonst.
      // Minuten fett, " min" normal ("4 min"), "jetzt" fett
      String timeText = departures[i].time;
      String timeSuffix = "";
      bool asMinutes = false;
      if (nowMinute > 0) {
        long diff = (long)((departures[i].planned + (time_t)delay * 60) / 60) - nowMinute;
        if (diff < 60) {
          asMinutes = true;
          timeText = diff <= 0 ? String("jetzt") : String(diff);
          if (diff > 0) timeSuffix = " min";
        }
      }

      // Gezeichnete Ausdehnung des Textes merken (fuer den Strich bei Ausfall)
      int16_t tbx, tby;
      uint16_t tbw, tbh;
      if (asMinutes) display.setFont(&FreeSansBold9pt8b);
      display.getTextBounds(timeText, COL_TIME, y, &tbx, &tby, &tbw, &tbh);
      int textLeft = tbx;
      int textRight = tbx + tbw - 1;
      display.setCursor(COL_TIME, y);
      display.print(timeText);
      display.setFont(&FreeSans9pt8b);
      if (timeSuffix.length() > 0) {
        display.getTextBounds(timeSuffix, display.getCursorX(), y, &tbx, &tby, &tbw, &tbh);
        textRight = tbx + tbw - 1;
        display.print(timeSuffix);
      }

      if (departures[i].cancelled) {
        // Ausfall: Uhrzeit bzw. Minuten durchstreichen (Warndreieck steht
        // links vom Trennstrich, siehe oben). Strich an den tatsaechlich
        // gezeichneten Zeichen ausrichten (die Schrift hat links einen
        // Vorlauf), Hoehe: Mitte der Ziffern
        display.getTextBounds("0", COL_TIME, y, &tbx, &tby, &tbw, &tbh);
        int strikeY = tby + tbh / 2;
        int strikeX0 = textLeft - STRIKE_OVERHANG;
        int strikeX1 = textRight + STRIKE_OVERHANG;
        display.drawLine(strikeX0, strikeY, strikeX1, strikeY, BLACK);

      } else if (!asMinutes) {
        // Abweichung 0 (auch eine auf 0 gesetzte -1/-2) nie anzeigen: Die API
        // meldet puenktliche Abfahrten nie als realtime (beobachtet), "+0"
        // kam daher nur noch durch die Verfruehungs-Regel zustande
        bool showDelay = (delay != 0);

        if (showDelay) {
          // Verspaetung ohne Einheit: "+3", verfruehte Abfahrt "-3" (Minus
          // steckt schon in der Zahl). Auf +/-DELAY_MAX_ABS begrenzt, damit
          // der Text rechts nicht umbricht.
          if (delay > DELAY_MAX_ABS) delay = DELAY_MAX_ABS;
          if (delay < -DELAY_MAX_ABS) delay = -DELAY_MAX_ABS;

          display.print(" ");
          display.setFont(&FreeSansBold9pt8b);
          if (delay >= 0) {
            display.print("+");
          }
          display.print(delay);
          display.setFont(&FreeSans9pt8b);
        }
      }

      if (i < found - 1) {
        int lineY = y + 6;
        display.drawLine(0, lineY, display.width(), lineY, BLACK);
      }
    }
  }
}

// ============================================================
// Oeffentliche Funktionen
// ============================================================

void displayInit() {
  display.landscape();
}

// Startbildschirm: Projektname, Board, Firmware-Version (inkl. Zusatz einer
// Erweiterung). Bleibt stehen, bis das .ino den ersten richtigen Bildschirm
// zeichnet (siehe SPLASH_DURATION_MS in config.h).
void displayShowSplash(const char* firmwareVersion) {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  // Titel weiss auf schwarzem Balken
  display.fillRect(0, 0, display.width(), SPLASH_BAR_HEIGHT, BLACK);
  display.setTextColor(WHITE);
  display.setFont(&FreeSansBold12pt7b);
  printCentered(SPLASH_TITLE, 27);
  display.setTextColor(BLACK);

  display.setFont(&FreeSans9pt8b);
  printCentered(SPLASH_SUBTITLE, 66);

  display.setFont(&FreeSansBold9pt8b);
  printCentered("Version " + String(firmwareVersion), 96);

  display.update();
}

void displayShowDepartures(const Departure departures[], int found,
                           const String& stationName, char dirLabel,
                           bool isPage2, bool minutes, bool incidents,
                           bool fullRefresh, int stationNumber,
                           const Departure* incidentDep) {
  if (fullRefresh) {
    display.fastmodeOff();
    display.clearMemory();
    drawContent(departures, found, stationName, dirLabel, isPage2, minutes, incidents,
                stationNumber, incidentDep);
    display.update();
  } else {
    // Fast Mode: zweimal hintereinander aktualisieren, verbessert den
    // Kontrast pro Update und wirkt Grauschleier entgegen
    display.fastmodeOn();
    display.clearMemory();
    drawContent(departures, found, stationName, dirLabel, isPage2, minutes, incidents,
                stationNumber, incidentDep);
    display.update();
    display.update();
  }
}

// WLAN-Fehlerbildschirm: Netzname, vermutete Ursache und ggf. ein Hinweis
// (Texte aus wifi_diag.cpp, bereits Latin-1). hint "" = keine Hinweiszeile.
void displayShowWifiError(const char* ssid, const char* reason, const char* hint,
                          bool retrying) {
  display.fastmodeOff();
  display.clearMemory();

  display.setTextColor(BLACK);
  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(5, 30);
  display.print("Kein WLAN");

  int maxTextWidth = display.width() - 10;

  display.setFont(&FreeSans9pt8b);
  display.setCursor(5, 55);
  display.print(fitText("Netz: " + utf8ToLatin1(String(ssid)), maxTextWidth));

  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(5, 75);
  display.print(fitText(reason, maxTextWidth));

  display.setFont(&FreeSans9pt8b);
  if (hint != nullptr && hint[0] != '\0') {
    display.setCursor(5, 95);
    display.print(fitText(hint, maxTextWidth));
  }

  display.setCursor(5, 118);
  display.print(retrying ? "Automatischer Neuversuch..." : "Warte auf Einrichtung...");

  display.update();
}

void displayShowApiError() {
  display.fastmodeOff();
  display.clearMemory();

  display.setTextColor(BLACK);
  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(5, 30);
  display.print("API nicht erreichbar");

  display.setFont(&FreeSans9pt8b);
  display.setCursor(5, 55);
  display.print("MVG-Abfahrten aktuell");
  display.setCursor(5, 75);
  display.print("nicht abrufbar.");

  display.setCursor(5, 105);
  display.print("Erneuter Versuch alle 10s...");

  display.update();
}

// Zeichnet den QR-Code. Wird von esp_qrcode_generate() WAEHREND der
// Erzeugung aufgerufen - die QR-Daten sind nur innerhalb dieses Callbacks
// gueltig. Die gezeichnete Kantenlaenge landet in qrDrawnSize (0 = Fehler).
static int qrDrawnSize = 0;
// Feste Modulgroesse und obere Kante (0 bzw. -1 = passend zu QR_AREA_SIZE,
// senkrecht mittig). Setzt drawQr() vor der Erzeugung.
static int qrFixedScale = 0;
static int qrFixedTop = -1;

static void drawQrCallback(esp_qrcode_handle_t qrcode) {
  int moduleCount = esp_qrcode_get_size(qrcode);
  int scale = qrFixedScale > 0 ? qrFixedScale : QR_AREA_SIZE / moduleCount;
  if (scale < 1) scale = 1;
  int qrPixelSize = moduleCount * scale;

  int qrX = QR_MARGIN;
  int qrY = qrFixedTop >= 0 ? qrFixedTop : (display.height() - qrPixelSize) / 2;

  for (int y = 0; y < moduleCount; y++) {
    for (int x = 0; x < moduleCount; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        display.fillRect(qrX + x * scale, qrY + y * scale, scale, scale, BLACK);
      }
    }
  }
  qrDrawnSize = qrPixelSize;
}

// Maskiert Sonderzeichen fuer das WLAN-QR-Format (\ ; , : " mit "\" davor),
// sonst werden SSIDs/Passwoerter mit diesen Zeichen falsch gelesen
static String escapeWifiQr(const char* text) {
  String out;
  for (const char* p = text; *p; p++) {
    if (*p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"') {
      out += '\\';
    }
    out += *p;
  }
  return out;
}

// QR-Code links auf dem Display zeichnen (QR-Generator aus dem ESP32-Core,
// ESP-IDF-Komponente esp_qrcode; die Version waehlt er selbst, hoechstens
// QR_MAX_VERSION). scale/top: feste Modulgroesse und obere Kante (0 bzw.
// -1 = so gross wie QR_AREA_SIZE erlaubt, senkrecht mittig). Rueckgabe:
// gezeichnete Kantenlaenge in Pixeln, 0 = Fehler (dann steht "QR-Fehler" an
// seiner Stelle).
static int drawQr(const String& payload, int scale = 0, int top = -1) {
  qrFixedScale = scale;
  qrFixedTop = top;
  esp_qrcode_config_t cfg = {};
  cfg.display_func = drawQrCallback;
  cfg.max_qrcode_version = QR_MAX_VERSION;
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;

  qrDrawnSize = 0;
  esp_err_t err = esp_qrcode_generate(&cfg, payload.c_str());
  if (err != ESP_OK || qrDrawnSize == 0) {
    Serial.print("QR-Code konnte nicht erzeugt werden, Fehler: ");
    Serial.println((int)err);
    display.setFont(&FreeSans9pt8b);
    display.setCursor(QR_MARGIN, 70);
    display.print("QR-Fehler");
    return 0;
  }
  return qrDrawnSize;
}

void displayShowWifiQr(const char* title, const char* qrWlanSsid,
                       const char* qrWlanPassword) {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  // QR-Code-Inhalt: Standard-Format fuers automatische Verbinden per Scan.
  // Bleibt UTF-8 (nicht utf8ToLatin1) - so erwarten es die Handys.
  String qrPayload = "WIFI:T:WPA;S:" + escapeWifiQr(qrWlanSsid) +
                     ";P:" + escapeWifiQr(qrWlanPassword) + ";;";

  drawQr(qrPayload);

  // Text rechts neben dem QR-Bereich (feste Position, unabhaengig von der
  // gewaehlten QR-Version)
  int textX = QR_MARGIN + QR_AREA_SIZE + 12;
  int maxTextWidth = display.width() - textX - 5;

  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(textX, 20);
  display.print(fitText(utf8ToLatin1(String(title)), maxTextWidth));

  display.setFont(&FreeSans9pt8b);

  display.setCursor(textX, 45);
  display.print("SSID:");
  display.setCursor(textX, 62);
  display.print(fitText(utf8ToLatin1(String(qrWlanSsid)), maxTextWidth));

  display.setCursor(textX, 85);
  display.print("Passwort:");
  display.setCursor(textX, 102);
  display.print(fitText(utf8ToLatin1(String(qrWlanPassword)), maxTextWidth));

  display.update();
}

// Einstellungen/Info: kleiner QR-Code zum Portal links oben, Version
// darunter; rechts IP-Adresse, bis wann das Portal offen ist, WLAN-Name,
// Signal und Laufzeit (Momentaufnahme beim Aufruf).
void displayShowInfo(const char* firmwareVersion, const char* portalAddress,
                     const char* portalUntil) {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  String url = "http://" + String(portalAddress) + "/";
  int qrSize = drawQr(url, INFO_QR_SCALE, INFO_QR_TOP);
  if (qrSize == 0) qrSize = QR_AREA_SIZE / 2;   // "QR-Fehler" steht dort
  int textX = QR_MARGIN + qrSize + INFO_TEXT_GAP;
  int maxTextWidth = display.width() - textX - 2;

  // Version zentriert unter dem QR-Code. Passt sie nicht in die Spalte
  // (z.B. mit Zusatz "-privat"), Zusatz ab "-" in eine zweite Zeile.
  display.setFont(&FreeSans9pt8b);
  String version = "v" + String(firmwareVersion);
  String version2 = "";
  int dash = version.indexOf('-');
  if (textWidth(version) > qrSize && dash > 0) {
    version2 = version.substring(dash + 1);
    version = version.substring(0, dash);
  }
  int versionY = INFO_QR_TOP + qrSize + 20;
  int columnWidth = textX - QR_MARGIN - 2;
  version = fitText(version, columnWidth);
  display.setCursor(QR_MARGIN + max(0, (qrSize - (int)textWidth(version)) / 2), versionY);
  display.print(version);
  if (version2.length() > 0) {
    version2 = fitText(version2, columnWidth);
    display.setCursor(QR_MARGIN + max(0, (qrSize - (int)textWidth(version2)) / 2),
                      versionY + INFO_LINE_HEIGHT);
    display.print(version2);
  }

  // Titel und IP-Adresse fett
  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(textX, 16);
  display.print("Einstellungen/Info");
  display.setCursor(textX, 38);
  display.print(portalAddress);

  display.setFont(&FreeSans9pt8b);
  int y = 56;
  display.setCursor(textX, y);
  if (portalUntil != nullptr && portalUntil[0] != '\0') {
    display.print("Portal offen bis ");
    display.print(portalUntil);
  } else {
    display.print("Portal 30 Min. offen");
  }

  y += INFO_LINE_HEIGHT;
  display.setCursor(textX, y);
  bool connected = WiFi.status() == WL_CONNECTED;
  String ssid = connected ? utf8ToLatin1(WiFi.SSID()) : String("--");
  display.print(fitText("WLAN: " + ssid, maxTextWidth));

  // Bewertung und Wert, z.B. "Signal: gut, -64 dBm"
  y += INFO_LINE_HEIGHT;
  display.setCursor(textX, y);
  String signal = "Signal: --";
  if (connected) {
    int rssi = WiFi.RSSI();
    signal = "Signal: " + String(wifiQualityText(rssi)) + ", " + String(rssi) + " dBm";
  }
  display.print(fitText(signal, maxTextWidth));

  // Laufzeit seit dem Start: "3 T 4 h", unter einem Tag "5 h 12 min"
  y += INFO_LINE_HEIGHT;
  display.setCursor(textX, y);
  unsigned long minutesUp = (unsigned long)(uptimeMs() / 60000ULL);
  unsigned long days = minutesUp / 1440;
  unsigned long hours = (minutesUp / 60) % 24;
  String uptime;
  if (days > 0) uptime = String(days) + " T " + String(hours) + " h";
  else uptime = String(hours) + " h " + String(minutesUp % 60) + " min";
  display.print("Laufzeit: " + uptime);

  display.update();
}

// Ersteinrichtung: QR-Code links, rechts Hinweis und Adresse. Die
// Textspalte beginnt direkt neben dem tatsaechlich gezeichneten QR-Code.
void displayShowPortalSetup(const char* url, const char* address) {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  int qrSize = drawQr(String(url));
  int textX = QR_MARGIN + (qrSize > 0 ? qrSize : QR_AREA_SIZE) + 12;

  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(textX, 24);
  display.print("Einrichtung");

  display.setFont(&FreeSans9pt8b);
  display.setCursor(textX, 50);
  display.print("QR-Code scannen");
  display.setCursor(textX, 68);
  display.print("oder im Browser");
  display.setCursor(textX, 86);
  display.print("(gleiches WLAN):");

  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(textX, 108);
  display.print(address);

  display.update();
}

// Firmware-Upload im Portal laeuft (bleibt bis zum Neustart stehen)
void displayShowUpdate() {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(5, 30);
  display.print("Firmware-Update l\xE4" "uft");

  display.setFont(&FreeSans9pt8b);
  display.setCursor(5, 58);
  display.print("Bitte nicht vom Strom trennen.");
  display.setCursor(5, 84);
  display.print("Danach startet das Display");
  display.setCursor(5, 102);
  display.print("von selbst neu.");

  display.update();
}
