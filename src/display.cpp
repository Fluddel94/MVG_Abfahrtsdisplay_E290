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

// --- Startbildschirm ---
#define SPLASH_TITLE    "MVG Abfahrtsdisplay"   // 7-Bit-Schrift: keine Umlaute
#define SPLASH_SUBTITLE "Heltec Vision Master E290"
#define SPLASH_BAR_HEIGHT 38   // schwarzer Titelbalken oben

// --- Warndreieck ---
#define WARNING_ICON_WIDTH 13
#define WARNING_ICON_GAP 6

// --- Ausfall (durchgestrichene Uhrzeit) ---
#define STRIKE_OVERHANG 2        // Ueberstand des Strichs links und rechts
#define CANCEL_ICON_SHIFT 2      // Warndreieck so weit rechts von der Verspaetungs-Position

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

// ============================================================
// Abfahrtsansicht
// ============================================================

// Zeichnet Header und Abfahrtsliste in den Displayspeicher (ohne
// display.update()). Header: "Station -> Richtung" (showDirection) oder nur
// der Stationsname, auf Seite 2 mit Hinweis "(2/2)" (isPage2).
static void drawContent(const Departure departures[], int found,
                        const String& stationName, bool showDirection,
                        bool showZentrum, bool isPage2) {
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

  if (showDirection) {
    // "Station -> Richtung" muss vor der Uhr-Box enden. Kuerzung:
    // 1. passt alles, Richtung in Langform ("Zentrum" / "Auswaerts")
    // 2. sonst Richtung in Kurzform ("Ztr." / "Ausw.")
    // 3. reicht auch das nicht, wird zusaetzlich der Stationsname gekuerzt
    int arrowTotalWidth = ARROW_GAP + ARROW_LENGTH + ARROW_HEAD_SIZE + ARROW_GAP;

    String prefix = stationName;
    String suffix = showZentrum ? LABEL_ZENTRUM : LABEL_AUSWAERTS;
    if (textWidth(prefix) + arrowTotalWidth + textWidth(suffix) > availableWidth) {
      suffix = showZentrum ? LABEL_ZENTRUM_SHORT : LABEL_AUSWAERTS_SHORT;
      prefix = fitText(prefix, availableWidth - arrowTotalWidth - textWidth(suffix));
    }

    int16_t pbx, pby;
    uint16_t pbw, pbh;
    display.getTextBounds(prefix, 0, 0, &pbx, &pby, &pbw, &pbh);

    int headerCursorY = boxCenterY - pby - pbh / 2;

    display.setCursor(5, headerCursorY);
    display.print(prefix);

    int prefixEndX = 5 + pbw;
    int arrowStartX = prefixEndX + ARROW_GAP;
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
  } else {
    // Nur der Stationsname, auf Seite 2 mit "(2/2)" dahinter. Gekuerzt wird
    // nur der Name - der Seitenhinweis bleibt immer vollstaendig sichtbar.
    int hintWidth = isPage2 ? HEADER_HINT_SPACE + textWidth(PAGE2_HINT) : 0;
    String name = fitText(stationName, availableWidth - hintWidth);

    // Vertikale Position nur am Namen ausrichten (die Klammern des
    // Seitenhinweises haben Unterlaengen und wuerden den Text verschieben)
    int16_t nbx, nby;
    uint16_t nbw, nbh;
    display.getTextBounds(name, 0, 0, &nbx, &nby, &nbw, &nbh);
    int headerCursorY = boxCenterY - nby - nbh / 2;

    display.setCursor(5, headerCursorY);
    display.print(name);

    if (isPage2) {
      display.setCursor(5 + nbw + HEADER_HINT_SPACE, headerCursorY);
      display.print(PAGE2_HINT);
    }
  }

  int separatorY = boxY + boxH + 3;
  display.fillRect(0, separatorY, display.width(), 3, BLACK);

  int rowHeight = 22;
  int yStart = separatorY + 3 + 17;

  if (found == 0) {
    display.setFont(&FreeSans9pt8b);
    display.setCursor(5, yStart);
    display.print(isPage2 ? "Keine weiteren Abfahrten" : "Keine Abfahrten gefunden");
  } else {
    for (int i = 0; i < found; i++) {
      int y = yStart + i * rowHeight;

      display.setFont(&FreeSans9pt8b);

      // Liniensymbol: S-/U-Bahn und Tram als Bitmap, Busse und unbekannte
      // Linien als generierter Rahmen mit Nummer (line_icons.h). Die Breite
      // variiert (max. 36 px), das Ziel beginnt trotzdem immer bei COL_DEST.
      drawLineIcon(display, COL_ICON, y - LINE_ICON_HEIGHT + ICON_Y_OFFSET,
                   departures[i].line, departures[i].isBus);
      // Der Text-Fallback fuer Bus-Icons setzt die Schrift zurueck
      display.setFont(&FreeSans9pt8b);
      display.setTextColor(BLACK);

      // Bei Ausfall steht das Warndreieck rechts statt der Verspaetung,
      // hinter dem Ziel dann keins (sonst doppelt)
      bool destWarning = departures[i].hasWarning && !departures[i].cancelled;

      int destMaxWidth = COL_PIPE2 - COL_DEST - DEST_MARGIN;
      if (destWarning) {
        destMaxWidth -= (WARNING_ICON_WIDTH + WARNING_ICON_GAP);
      }

      String dest = fitText(departures[i].destination, destMaxWidth);
      display.setCursor(COL_DEST, y);
      display.print(dest);

      if (destWarning) {
        int destTextWidth = textWidth(dest);
        int iconX = COL_DEST + destTextWidth + WARNING_ICON_GAP + WARNING_ICON_WIDTH / 2;
        drawWarningTriangle(iconX, y);
      }

      display.setCursor(COL_PIPE2, y);
      display.print("|");

      display.setCursor(COL_TIME, y);
      display.print(departures[i].time);

      if (departures[i].cancelled) {
        // Ausfall: Uhrzeit durchstreichen, Warndreieck an die Stelle der
        // Verspaetung (linke Kante dort, wo sonst "+x" beginnt)
        int16_t timeBx, timeBy;
        uint16_t timeBw, timeBh;
        display.getTextBounds(departures[i].time, 0, 0, &timeBx, &timeBy, &timeBw, &timeBh);
        int strikeY = y + timeBy + timeBh / 2;
        // An den tatsaechlich gezeichneten Ziffern ausrichten: die Schrift hat
        // links einen Vorlauf (timeBx), sonst sitzt der Strich zu weit links
        int strikeX0 = COL_TIME + timeBx - STRIKE_OVERHANG;
        int strikeX1 = COL_TIME + timeBx + timeBw - 1 + STRIKE_OVERHANG;
        display.drawLine(strikeX0, strikeY, strikeX1, strikeY, BLACK);

        display.print(" ");                       // Cursor steht jetzt, wo "+x" begaenne
        int delayStartX = display.getCursorX() + CANCEL_ICON_SHIFT;
        drawWarningTriangle(delayStartX + WARNING_ICON_WIDTH / 2, y);

      } else {
        // Verfruehung unter EARLY_DEPARTURE_MIN (config.h) wie puenktlich
        // behandeln - "-1" verwirrt und ist meist nur Rundung/Prognose
        int delay = departures[i].delayMin;
        if (delay < 0 && delay > -EARLY_DEPARTURE_MIN) delay = 0;

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
                           const String& stationName, bool showDirection,
                           bool showZentrum, bool isPage2, bool fullRefresh) {
  if (fullRefresh) {
    display.fastmodeOff();
    display.clearMemory();
    drawContent(departures, found, stationName, showDirection, showZentrum, isPage2);
    display.update();
  } else {
    // Fast Mode: zweimal hintereinander aktualisieren, verbessert den
    // Kontrast pro Update und wirkt Grauschleier entgegen
    display.fastmodeOn();
    display.clearMemory();
    drawContent(departures, found, stationName, showDirection, showZentrum, isPage2);
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

static void drawQrCallback(esp_qrcode_handle_t qrcode) {
  int moduleCount = esp_qrcode_get_size(qrcode);
  int scale = QR_AREA_SIZE / moduleCount;
  if (scale < 1) scale = 1;
  int qrPixelSize = moduleCount * scale;

  int qrX = QR_MARGIN;
  int qrY = (display.height() - qrPixelSize) / 2;

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

void displayShowWifiQr(const char* title, const char* qrWlanSsid,
                       const char* qrWlanPassword) {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  // QR-Code-Inhalt: Standard-Format fuers automatische Verbinden per Scan.
  // Bleibt UTF-8 (nicht utf8ToLatin1) - so erwarten es die Handys.
  String qrPayload = "WIFI:T:WPA;S:" + escapeWifiQr(qrWlanSsid) +
                     ";P:" + escapeWifiQr(qrWlanPassword) + ";;";

  // QR-Generator aus dem ESP32-Core (ESP-IDF-Komponente esp_qrcode). Die
  // Version (Groesse) waehlt er selbst, hoechstens QR_MAX_VERSION.
  esp_qrcode_config_t cfg = {};
  cfg.display_func = drawQrCallback;
  cfg.max_qrcode_version = QR_MAX_VERSION;
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;

  qrDrawnSize = 0;
  esp_err_t err = esp_qrcode_generate(&cfg, qrPayload.c_str());
  if (err != ESP_OK || qrDrawnSize == 0) {
    Serial.print("QR-Code konnte nicht erzeugt werden, Fehler: ");
    Serial.println((int)err);
    display.setFont(&FreeSans9pt8b);
    display.setCursor(QR_MARGIN, 70);
    display.print("QR-Fehler");
  }

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

void displayShowLog(const char* firmwareVersion, uint64_t wifiConnectedSince,
                    int wifiDisconnects, int apiFails) {
  display.fastmodeOff();
  display.clearMemory();
  display.setTextColor(BLACK);

  // Zeilenabstand 18px, damit 7 Zeilen auf die 128px Displayhoehe passen
  display.setFont(&FreeSansBold9pt8b);
  display.setCursor(5, 16);
  display.print("System-Log");

  // Firmware-Version rechtsbuendig in der Titelzeile
  display.setFont(&FreeSans9pt8b);
  String versionText = "v" + String(firmwareVersion);
  display.setCursor(display.width() - textWidth(versionText) - 5, 16);
  display.print(versionText);

  // Board-Startzeitpunkt = aktuelle NTP-Zeit minus Laufzeit seit Boot.
  // Nur gueltig, wenn die Uhr bereits per NTP gestellt wurde - ohne NTP
  // startet die ESP32-Uhr bei 1970, daher Plausibilitaetsgrenze.
  const time_t TIME_VALID_MIN = 1700000000; // 14.11.2023
  time_t now = time(nullptr);
  display.setCursor(5, 35);
  display.print("Board-Start: ");
  if (now >= TIME_VALID_MIN) {
    time_t bootTime = now - (time_t)(uptimeMs() / 1000ULL);
    display.print(formatDateTime(bootTime));
  } else {
    display.print("unbekannt (kein NTP)");
  }

  String wifiUptime = formatUptime(uptimeMs() - wifiConnectedSince);
  display.setCursor(5, 53);
  display.print("WLAN-Laufzeit: ");
  display.print(wifiUptime);

  // Momentaufnahme der Signalstaerke beim Oeffnen des Log-Screens
  display.setCursor(5, 71);
  display.print("WLAN-Signal: ");
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    display.print(rssi);
    display.print(" dBm (");
    display.print(wifiQualityText(rssi));
    display.print(")");
  } else {
    display.print("--");
  }

  display.setCursor(5, 89);
  display.print("WLAN-Abbr\xFC" "che: ");   // getrennt: "c" waere sonst Teil von \xFC
  display.print(wifiDisconnects);
  display.print(" (seit Start)");

  display.setCursor(5, 107);
  display.print("API-Fehler (24h): ");
  display.print(apiFails);

  display.setCursor(5, 125);
  display.print("Zur\xFC" "ck: 60s oder 3s halten");

  display.update();
}
