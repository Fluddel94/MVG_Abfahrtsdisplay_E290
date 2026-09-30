// portal.cpp
// Einstellungsportal (siehe portal.h).
//
// Seiten: "/" Formular, "/speichern" (POST), "/werkseinstellungen" (POST),
// "/suche?q=" Stationssuche (JSON), "/richtungen?id=" Linien je
// Richtungskennung (JSON), "/update" (POST) Firmware-Upload. Die aktuellen Werte bekommt die Seite als
// JSON-Objekt S, das Formular fuellt sich per JavaScript.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_app_format.h>   // ESP_CHIP_ID_ESP32S3
#include <time.h>
#include "../config.h"
#include "settings.h"
#include "mvg_api.h"
#include "portal.h"

// Link zur Anleitung (Hilfe zu den Einstellungen)
#define PORTAL_HELP_URL "https://github.com/Fluddel94/MVG_Abfahrtsdisplay_E290#readme"
#define PORTAL_TASK_STACK 8192

// Kennung dieser Firmware: Hochgeladene Dateien muessen diesen Text
// enthalten (er steht durch den Vergleich selbst in jeder Firmware ab
// 2.0.0). Das erste Zeichen darf im Text nicht noch einmal vorkommen (siehe
// scanMarker()). Nie aendern - sonst lehnen aeltere Geraete neue Firmware ab.
static const char FIRMWARE_MARKER[] = "MVG_Abfahrtsdisplay_E290/firmware";

// ------------------------------------------------------------
// Seite (HTML, CSS, JavaScript)
// ------------------------------------------------------------
// Zwischen PAGE_HEAD und PAGE_SCRIPT steht "<script>const S={...};</script>"
static const char PAGE_HEAD[] = R"HTML(<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Abfahrtsdisplay</title><style>
body{font-family:system-ui,sans-serif;margin:0;background:#f2f2f2;color:#111}
main{max-width:560px;margin:auto;padding:12px}
h1{font-size:1.4em;margin:8px 0}
section{background:#fff;border-radius:8px;padding:12px;margin:12px 0}
h2{font-size:1.1em;margin:0 0 8px}
label{display:block;margin:8px 0 4px}
input[type=text],select{width:100%;box-sizing:border-box;padding:8px;font-size:1em}
.row{display:flex;gap:8px}.row input{flex:1}
button{padding:9px 14px;font-size:1em;border-radius:6px;border:1px solid #888;background:#eee}
button.main{background:#0a5;color:#fff;border-color:#0a5;width:100%;padding:12px}
button.danger{background:#fff;color:#b00;border-color:#b00}
.hint{color:#555;font-size:.9em}
.cb{display:flex;align-items:center;gap:8px;margin:6px 0}.cb input{width:20px;height:20px}
.hit{border:1px solid #bbb;border-radius:6px;padding:8px;margin:6px 0;background:#fff;color:#111;font-size:1rem}
.hit .acts{display:flex;gap:12px;align-items:center;margin-top:6px}
table{border-collapse:collapse;width:100%;font-size:.9em}td,th{border-bottom:1px solid #ddd;padding:4px;text-align:left}
</style></head><body><main>
<h1>Abfahrtsdisplay</h1>
<p class="hint">Hilfe zu allen Einstellungen: <a id="help" href="#" target="_blank">Anleitung (README)</a></p>
<form method="post" action="/speichern" onsubmit="return check()">
<section><h2>Station</h2>
<label for="q">Station suchen</label>
<div class="row"><input type="text" id="q" placeholder="z.B. Marienplatz"><button type="button" onclick="search()">Suchen</button></div>
<div id="results" class="hint"></div>
<label for="station">Station-ID (globalId)</label>
<input type="text" id="station" name="station" placeholder="z.B. de:09162:2">
<p class="hint" id="stationName"></p>
<p class="hint">Die Suche nutzt die inoffizielle MVG-API. Klappt sie nicht, die ID aus der Haltestellenliste eintragen (siehe Anleitung).</p>
</section>
<section><h2>Anzeige</h2>
<label for="dirView">Richtungen</label>
<select id="dirView" name="dirView" onchange="upd()"><option value="0">gemischt (alle Richtungen, Seite 2 per Taste)</option><option value="1">getrennt nach Zentrum / Ausw&auml;rts</option></select>
<div id="dirOpts">
<label for="zentrum">Richtung Zentrum hat die Kennung</label>
<select id="zentrum" name="zentrum"><option value="H">H</option><option value="R">R</option></select>
<p><button type="button" onclick="dirs()">Richtungen anzeigen</button></p>
<div id="dirList" class="hint"></div>
<label for="defView">Standardansicht</label>
<select id="defView" name="defView"><option value="Z">Zentrum</option><option value="A">Ausw&auml;rts</option></select>
</div></section>
<section><h2>Verkehrsmittel</h2>
<div class="cb"><input type="checkbox" id="sbahn" name="sbahn"><label for="sbahn">S-Bahn</label></div>
<div class="cb"><input type="checkbox" id="ubahn" name="ubahn"><label for="ubahn">U-Bahn</label></div>
<div class="cb"><input type="checkbox" id="tram" name="tram"><label for="tram">Tram</label></div>
<div class="cb"><input type="checkbox" id="bus" name="bus"><label for="bus">Bus (Stadt- und Regionalbus)</label></div>
<div class="cb"><input type="checkbox" id="bahn" name="bahn"><label for="bahn">Regionalzug</label></div>
</section>
<section><h2>WLAN-QR-Code</h2>
<div class="cb"><input type="checkbox" id="qrOn" name="qrOn" onchange="upd()"><label for="qrOn">Kurzer Tastendruck zeigt einen QR-Code f&uuml;r ein WLAN (z.B. G&auml;ste-WLAN)</label></div>
<div id="qrOpts">
<label for="qrTitle">&Uuml;berschrift</label><input type="text" id="qrTitle" name="qrTitle" maxlength="40">
<label for="qrSsid">WLAN-Name</label><input type="text" id="qrSsid" name="qrSsid" maxlength="32">
<label for="qrPass">Passwort</label><input type="text" id="qrPass" name="qrPass" maxlength="63" autocomplete="off">
<p class="hint">Das Passwort steht f&uuml;r alle lesbar auf dem Display. Leer lassen = unver&auml;ndert.</p>
</div></section>
<button class="main" type="submit">Speichern</button>
</form>
<section><h2>Ger&auml;t</h2>
<p>WLAN: <b id="wifi"></b> (Signal <span id="rssi"></span> dBm)<br>
<span class="hint">&Auml;ndern: im Web-Installer &bdquo;WLAN &auml;ndern&ldquo;.</span></p>
<p>Firmware <span id="version"></span> &middot; IP <span id="ip"></span></p>
<p id="closes" class="hint"></p>
<p><b>Firmware aktualisieren</b><br><span class="hint">Firmware-Datei (.bin) aus dem Release auf GitHub. Die Einstellungen bleiben erhalten.</span></p>
<p><input type="file" id="fw" accept=".bin"></p>
<p><button type="button" id="fwBtn" onclick="upload()">Hochladen</button></p>
<progress id="fwBar" max="100" value="0" style="width:100%;display:none"></progress>
<p id="fwMsg"></p>
<form method="post" action="/werkseinstellungen" onsubmit="return confirm('Alle Einstellungen inkl. WLAN löschen und neu starten?')">
<button class="danger" type="submit">Werkseinstellungen</button></form>
</section></main>
)HTML";
static const char PAGE_SCRIPT[] = R"HTML(<script>
const $=i=>document.getElementById(i);
function esc(t){return String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function upd(){$('dirOpts').style.display=$('dirView').value=='1'?'':'none';$('qrOpts').style.display=$('qrOn').checked?'':'none';}
function check(){
 if(!$('station').value.trim()){alert('Bitte eine Station wählen.');return false;}
 if(!['sbahn','ubahn','tram','bus','bahn'].some(i=>$(i).checked)){alert('Mindestens ein Verkehrsmittel wählen.');return false;}
 if($('qrOn').checked&&!$('qrSsid').value.trim()){alert('WLAN-Name für den QR-Code fehlt.');return false;}
 return true;}
function search(){
 const q=$('q').value.trim(); if(q.length<2)return;
 $('results').textContent='Suche ...';
 fetch('/suche?q='+encodeURIComponent(q)).then(r=>r.json()).then(l=>{
  if(!l.length){$('results').textContent='Keine Station gefunden.';return;}
  $('results').innerHTML='';
  l.forEach(s=>{const d=document.createElement('div');d.className='hit';
   let h='<b>'+esc(s.n)+'</b><br>Ort: '+esc(s.p)+(s.z?' &middot; Zone '+esc(s.z.toUpperCase()):'')+
    '<br>'+esc(s.t)+'<br><span class="hint">ID '+esc(s.id)+'</span><div class="acts"><button type="button">Ausw&auml;hlen</button>';
   if(s.lat&&s.lon)h+='<a target="_blank" href="https://www.openstreetmap.org/?mlat='+s.lat+'&mlon='+s.lon+'#map=17/'+s.lat+'/'+s.lon+'">Auf Karte zeigen</a>';
   d.innerHTML=h+'</div>';
   d.querySelector('button').onclick=()=>{$('station').value=s.id;$('stationName').textContent='Gewählt: '+s.n+', '+s.p+' ('+s.id+')';$('results').innerHTML='';};
   $('results').appendChild(d);});
 }).catch(()=>{$('results').textContent='Suche fehlgeschlagen (MVG-API nicht erreichbar).';});}
function dirs(){
 const id=$('station').value.trim(); if(!id){alert('Erst eine Station wählen.');return;}
 $('dirList').textContent='Lade Abfahrten ...';
 fetch('/richtungen?id='+encodeURIComponent(id)).then(r=>r.json()).then(l=>{
  if(!l.length){$('dirList').textContent='Keine Abfahrten gefunden.';return;}
  let h='<table><tr><th>Kennung</th><th>Linie</th><th>Ziel</th></tr>';
  l.forEach(d=>{h+='<tr><td>'+esc(d.d)+'</td><td>'+esc(d.l)+'</td><td>'+esc(d.z)+'</td></tr>';});
  $('dirList').innerHTML=h+'</table><p>F&auml;hrt H Richtung Innenstadt, &bdquo;H&ldquo; w&auml;hlen, sonst &bdquo;R&ldquo;.</p>';
 }).catch(()=>{$('dirList').textContent='Abruf fehlgeschlagen.';});}
function upload(){
 const f=$('fw').files[0]; if(!f){alert('Bitte eine .bin-Datei wählen.');return;}
 if(f.size>S.fwMax){alert('Die Datei ist zu groß. Bitte die Firmware-Datei nehmen, nicht das Gesamtabbild (merged).');return;}
 if(!confirm('Firmware „'+f.name+'“ aufspielen? Das Display startet danach neu.'))return;
 const fd=new FormData();fd.append('firmware',f,f.name);
 const x=new XMLHttpRequest();x.open('POST','/update');
 x.upload.onprogress=e=>{if(e.lengthComputable)$('fwBar').value=e.loaded*100/e.total;};
 x.onload=()=>{if(x.status==200){$('fwBar').value=100;$('fwMsg').textContent='Erfolgreich. Das Display startet neu, die Einstellungen bleiben erhalten. Diese Seite ist danach wieder über den System-Log erreichbar.';}
  else{$('fwMsg').textContent='Fehler: '+x.responseText+' Die bisherige Firmware läuft weiter.';$('fwBtn').disabled=false;}};
 x.onerror=()=>{$('fwMsg').textContent='Verbindung abgebrochen. Die bisherige Firmware läuft weiter.';$('fwBtn').disabled=false;};
 $('fwBtn').disabled=true;$('fwBar').style.display='';$('fwBar').value=0;$('fwMsg').textContent='Lade hoch ...';x.send(fd);}
$('help').href=S.help;$('station').value=S.station;
$('dirView').value=S.dirView?'1':'0';$('zentrum').value=S.zentrumIsH?'H':'R';$('defView').value=S.defZentrum?'Z':'A';
['sbahn','ubahn','tram','bus','bahn','qrOn'].forEach(i=>$(i).checked=S[i]);
$('qrTitle').value=S.qrTitle;$('qrSsid').value=S.qrSsid;if(S.qrPassSet)$('qrPass').placeholder='unverändert';
$('wifi').textContent=S.wifi;$('rssi').textContent=S.rssi;$('version').textContent=S.version;$('ip').textContent=S.ip;
$('closes').textContent=S.closes?'Diese Seite ist bis '+S.closes+' Uhr erreichbar (erneut öffnen: Taste 3 s halten).':'';
$('q').addEventListener('keydown',e=>{if(e.key=='Enter'){e.preventDefault();search();}});
upd();
</script></body></html>)HTML";

// ------------------------------------------------------------
// Zustand
// ------------------------------------------------------------
static WebServer* server = nullptr;
static volatile bool running = false;
static volatile unsigned long openStart = 0;
static String firmwareVersionCopy;

// Vom Portal-Task an portalLoop() uebergeben
static volatile bool pendingSave = false;
static volatile bool pendingReset = false;
static DeviceSettings pendingSettings;

// Firmware-Upload (Portal-Task), Neustart danach in portalLoop()
static volatile bool updateRunning = false;
static volatile bool pendingRestart = false;
static String updateError;       // "" = bisher kein Fehler
static bool updateHeaderChecked = false;
static bool markerFound = false;
static size_t markerPos = 0;

// ------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------

// Kurze Antwortseite mit Link zurueck
static void sendMessage(int code, const char* message) {
  String page = F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                  "<title>Abfahrtsdisplay</title></head>"
                  "<body style=\"font-family:system-ui,sans-serif;max-width:560px;margin:auto;padding:12px\">"
                  "<h1>Abfahrtsdisplay</h1><p>");
  page += message;
  page += F("</p><p><a href=\"/\">Zur&uuml;ck zu den Einstellungen</a></p></body></html>");
  server->send(code, "text/html; charset=utf-8", page);
}

// Erlaubte Zeichen einer Station-ID (sie landet in der API-Adresse)
static bool validStationId(const String& id) {
  if (id.length() == 0 || id.length() > 40) return false;
  for (unsigned int i = 0; i < id.length(); i++) {
    char c = id[i];
    if (!isalnum((unsigned char)c) && c != ':' && c != '_' && c != '-') return false;
  }
  return true;
}

// Formularfeld, gekuerzt auf maxLen Zeichen
static String formArg(const char* name, unsigned int maxLen) {
  String value = server->arg(name);
  value.trim();
  if (value.length() > maxLen) value = value.substring(0, maxLen);
  return value;
}

// ------------------------------------------------------------
// Seiten
// ------------------------------------------------------------
static void handleRoot() {
  JsonDocument doc;
  {
    SettingsLock lock;
    const DeviceSettings& s = appSettings;
    doc["station"] = s.stationId;
    doc["dirView"] = s.directionView;
    doc["zentrumIsH"] = s.zentrumIsH;
    doc["defZentrum"] = s.defaultViewZentrum;
    doc["sbahn"] = s.showSbahn;
    doc["ubahn"] = s.showUbahn;
    doc["tram"] = s.showTram;
    doc["bus"] = s.showBus;
    doc["bahn"] = s.showBahn;
    doc["qrOn"] = s.wifiQr;
    doc["qrTitle"] = s.qrTitle;
    doc["qrSsid"] = s.qrSsid;
    doc["qrPassSet"] = s.qrPassword.length() > 0;   // Passwoerter nie zurueckgeben
    doc["wifi"] = s.wifiSsid;
  }
  doc["rssi"] = WiFi.RSSI();
  doc["version"] = firmwareVersionCopy;
  doc["ip"] = WiFi.localIP().toString();
  doc["closes"] = portalClosingTime();
  doc["help"] = PORTAL_HELP_URL;
  doc["fwMax"] = ESP.getFreeSketchSpace();   // Groesse des freien Programmbereichs

  String json;
  serializeJson(doc, json);
  json.replace("<", "\\u003c");   // kein "</script>" aus Nutzereingaben

  String page;
  page.reserve(sizeof(PAGE_HEAD) + sizeof(PAGE_SCRIPT) + json.length() + 40);
  page += PAGE_HEAD;
  page += "<script>const S=";
  page += json;
  page += ";</script>\n";
  page += PAGE_SCRIPT;
  server->send(200, "text/html; charset=utf-8", page);
}

static void handleSave() {
  if (pendingSave) {
    sendMessage(503, "Die letzte &Auml;nderung wird noch &uuml;bernommen, bitte kurz warten.");
    return;
  }

  DeviceSettings s;
  {
    SettingsLock lock;
    s = appSettings;
  }

  String station = formArg("station", 40);
  if (!validStationId(station)) {
    sendMessage(400, "Bitte eine g&uuml;ltige Station w&auml;hlen.");
    return;
  }
  s.stationId = station;
  s.directionView = server->arg("dirView") == "1";
  s.zentrumIsH = server->arg("zentrum") != "R";
  s.defaultViewZentrum = server->arg("defView") != "A";
  s.showSbahn = server->hasArg("sbahn");
  s.showUbahn = server->hasArg("ubahn");
  s.showTram = server->hasArg("tram");
  s.showBus = server->hasArg("bus");
  s.showBahn = server->hasArg("bahn");
  if (!(s.showSbahn || s.showUbahn || s.showTram || s.showBus || s.showBahn)) {
    sendMessage(400, "Mindestens ein Verkehrsmittel w&auml;hlen.");
    return;
  }

  s.wifiQr = server->hasArg("qrOn");
  s.qrTitle = formArg("qrTitle", 40);
  s.qrSsid = formArg("qrSsid", 32);
  String qrPass = server->arg("qrPass");   // Leerzeichen gehoeren zum Passwort
  if (qrPass.length() > 63) qrPass = qrPass.substring(0, 63);
  if (qrPass.length() > 0) s.qrPassword = qrPass;   // leer = unveraendert
  if (s.wifiQr && s.qrSsid.length() == 0) {
    sendMessage(400, "WLAN-Name f&uuml;r den QR-Code fehlt.");
    return;
  }

  pendingSettings = s;
  pendingSave = true;
  Serial.println("Portal: Einstellungen empfangen");
  sendMessage(200, "Gespeichert. Das Display &uuml;bernimmt die Einstellungen in wenigen Sekunden.");
}

static void handleFactoryReset() {
  pendingReset = true;
  sendMessage(200, "Werkseinstellungen: Alle Einstellungen werden gel&ouml;scht, das Ger&auml;t startet neu. "
                   "Danach das WLAN &uuml;ber den Web-Installer neu einrichten.");
}

static void handleSearch() {
  String query = formArg("q", 50);
  String json;
  if (query.length() < 2) {
    json = "[]";
  } else if (!searchStations(query, json)) {
    server->send(502, "application/json", "{\"error\":\"MVG-API nicht erreichbar\"}");
    return;
  }
  server->send(200, "application/json; charset=utf-8", json);
}

static void handleDirections() {
  String id = formArg("id", 40);
  String json;
  if (!validStationId(id) || !listDirections(id.c_str(), json)) {
    server->send(502, "application/json", "{\"error\":\"Abruf fehlgeschlagen\"}");
    return;
  }
  server->send(200, "application/json; charset=utf-8", json);
}

// ------------------------------------------------------------
// Firmware-Upload
// ------------------------------------------------------------

// Sucht FIRMWARE_MARKER im Datenstrom (auch ueber Blockgrenzen hinweg).
// Einfache Zustandssuche - korrekt, weil das erste Zeichen des Markers im
// Rest nicht noch einmal vorkommt.
static void scanMarker(const uint8_t* data, size_t length) {
  const size_t markerLength = sizeof(FIRMWARE_MARKER) - 1;
  for (size_t i = 0; i < length && !markerFound; i++) {
    char c = (char)data[i];
    if (c == FIRMWARE_MARKER[markerPos]) {
      markerPos++;
      if (markerPos == markerLength) markerFound = true;
    } else {
      markerPos = (c == FIRMWARE_MARKER[0]) ? 1 : 0;
    }
  }
}

// Fehler merken und das halb geschriebene Update verwerfen (die bisherige
// Firmware bleibt aktiv)
static void failUpdate(const char* message) {
  if (updateError.length() == 0) updateError = message;
  Update.abort();
}

// Prueft den Kopf der Datei: ESP32-Programm (0xE9) fuer den ESP32-S3.
// Bootloader, Partitionstabelle oder Firmware fuer andere Chips fallen hier
// bzw. spaetestens an der fehlenden Kennung durch.
static bool checkImageHeader(const uint8_t* data, size_t length) {
  if (length < 16 || data[0] != 0xE9) return false;
  uint16_t chipId = data[12] | (data[13] << 8);
  return chipId == ESP_CHIP_ID_ESP32S3;
}

// Wird fuer jeden empfangenen Block der Datei aufgerufen
static void handleUpdateUpload() {
  HTTPUpload& upload = server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    openStart = millis();   // Portal waehrend des Uploads nicht schliessen
    updateError = "";
    updateHeaderChecked = false;
    markerFound = false;
    markerPos = 0;
    updateRunning = true;
    Serial.print("Portal: Firmware-Upload ");
    Serial.println(upload.filename);
    // Groesse unbekannt: begrenzt auf den freien Programmbereich
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      failUpdate("Update konnte nicht gestartet werden.");
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (updateError.length() > 0) return;
    openStart = millis();
    if (!updateHeaderChecked) {
      updateHeaderChecked = true;
      if (!checkImageHeader(upload.buf, upload.currentSize)) {
        failUpdate("Das ist keine Firmware für dieses Board (ESP32-S3).");
        return;
      }
    }
    scanMarker(upload.buf, upload.currentSize);
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      failUpdate(Update.getError() == UPDATE_ERROR_SPACE
                     ? "Die Datei ist zu groß."
                     : "Schreiben fehlgeschlagen.");
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (updateError.length() > 0) return;
    if (!updateHeaderChecked) {
      failUpdate("Die Datei ist leer.");
    } else if (!markerFound) {
      failUpdate("Das ist keine Firmware für das Abfahrtsdisplay.");
    } else if (!Update.end(true)) {
      failUpdate("Die Datei ist beschädigt oder unvollständig.");
    } else {
      Serial.print("Portal: Firmware geschrieben, ");
      Serial.print(upload.totalSize);
      Serial.println(" Bytes");
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    failUpdate("Upload abgebrochen.");
    updateRunning = false;
    Serial.println("Portal: Firmware-Upload abgebrochen");
  }
}

// Nach dem Upload: Ergebnis an den Browser, bei Erfolg Neustart
static void handleUpdateDone() {
  if (updateError.length() > 0 || !updateHeaderChecked) {
    String message = updateError.length() > 0 ? updateError : String("Keine Datei empfangen.");
    Serial.print("Portal: Firmware-Update fehlgeschlagen: ");
    Serial.println(message);
    updateRunning = false;
    server->send(400, "text/plain; charset=utf-8", message);
    return;
  }
  server->send(200, "text/plain; charset=utf-8", "ok");
  pendingRestart = true;
}

// ------------------------------------------------------------
// Task
// ------------------------------------------------------------
static void portalTask(void* param) {
  (void)param;
  while (millis() - openStart < PORTAL_DURATION_MS) {
    server->handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  server->stop();
  delete server;
  server = nullptr;
  running = false;
  Serial.println("Portal geschlossen");
  vTaskDelete(nullptr);
}

// ------------------------------------------------------------
// Oeffentliche Funktionen
// ------------------------------------------------------------
void portalBegin(const char* firmwareVersion) {
  firmwareVersionCopy = firmwareVersion;
}

void portalOpen() {
  openStart = millis();
  if (running) return;

  server = new WebServer(80);
  server->on("/", HTTP_GET, handleRoot);
  server->on("/speichern", HTTP_POST, handleSave);
  server->on("/werkseinstellungen", HTTP_POST, handleFactoryReset);
  server->on("/suche", HTTP_GET, handleSearch);
  server->on("/richtungen", HTTP_GET, handleDirections);
  server->on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server->onNotFound([]() {
    server->sendHeader("Location", "/");
    server->send(302, "text/plain", "");
  });
  server->begin();
  running = true;
  xTaskCreate(portalTask, "portal", PORTAL_TASK_STACK, nullptr, 1, nullptr);
  Serial.print("Portal geoeffnet: ");
  Serial.println(portalUrl());
}

bool portalIsOpen() {
  return running;
}

String portalUrl() {
  return "http://" + WiFi.localIP().toString() + "/";
}

String portalClosingTime() {
  const time_t TIME_VALID_MIN = 1700000000;   // vorher keine NTP-Zeit
  time_t now = time(nullptr);
  if (now < TIME_VALID_MIN || !running) return "";
  unsigned long remaining = PORTAL_DURATION_MS - (millis() - openStart);
  time_t closes = now + (time_t)(remaining / 1000UL);
  struct tm t;
  localtime_r(&closes, &t);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  return String(buf);
}

bool portalUpdateRunning() {
  return updateRunning;
}

bool portalLoop() {
  if (pendingRestart) {
    Serial.println("Portal: Firmware-Update fertig, Neustart");
    delay(1000);   // Antwort noch ausliefern
    ESP.restart();
  }
  if (pendingReset) {
    Serial.println("Portal: Werkseinstellungen, Neustart");
    settingsFactoryReset();
    delay(1000);   // Antwortseite noch ausliefern
    ESP.restart();
  }
  if (!pendingSave) return false;
  {
    SettingsLock lock;
    appSettings = pendingSettings;
  }
  settingsSave();
  settingsPrint();
  pendingSave = false;
  return true;
}
