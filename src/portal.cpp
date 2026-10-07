// portal.cpp
// Einstellungsportal (siehe portal.h).
//
// Seiten: "/" Formular, "/speichern" (POST), "/werkseinstellungen" (POST),
// "/suche?q=" Stationssuche (JSON), "/richtungen?id=&types=" Linien und Ziele
// je Richtungskennung (JSON, Beispielziele bei der Richtungswahl), "/name?id=" Stationsname (JSON),
// "/linien?id=&types=" Linien der Station
// (Verkehrsmittel als TYPE_...-Bits) mit Beispielzielen (JSON), "/update" (POST) Firmware-Upload. Die aktuellen
// Werte bekommt die Seite als JSON-Objekt S, das Formular fuellt sich per
// JavaScript.

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
#define PORTAL_HELP_URL "https://github.com/Fluddel94/MVG_Abfahrtsdisplay_E290#einstellungsportal"
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
main{max-width:560px;margin:auto;padding:12px 12px 80px}
h1{font-size:1.4em;margin:8px 0}
section{background:#fff;border-radius:8px;padding:12px;margin:12px 0}
h2{font-size:1.1em;margin:0 0 4px;display:flex;align-items:center;gap:8px}
h3{font-size:1em;margin:2px 0 6px}h3 small{font-weight:normal;color:#666}
label{display:block;margin:8px 0 4px}
input[type=text],select{width:100%;box-sizing:border-box;padding:8px;font-size:1em;border:1px solid #999;border-radius:6px;background:#fff;color:#111}
.row{display:flex;gap:8px}.row input{flex:1}
button{padding:9px 14px;font-size:1em;border-radius:6px;border:1px solid #888;background:#eee;color:#111}
button.danger{background:#fff;color:#b00;border-color:#b00}
button.link{border:0;background:none;color:#06c;text-decoration:underline;padding:4px 0}
.hint{color:#555;font-size:.88em;margin:6px 0}
.warn{color:#a60;font-weight:600}
.no{display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;background:#111;color:#fff;border-radius:3px;font-size:.85em}
.step{display:flex;gap:10px;margin-top:14px}
.sn{flex:none;width:22px;height:22px;border-radius:50%;border:2px solid #0a5;color:#0a5;font-weight:bold;font-size:.8em;display:flex;align-items:center;justify-content:center;box-sizing:border-box}
.sb{flex:1;min-width:0}
.chosen{display:flex;align-items:center;gap:8px;border:1px solid #0a5;background:#f1faf4;border-radius:6px;padding:8px}.chosen div{flex:1}
details{margin-top:6px;font-size:.88em;color:#555}summary{cursor:pointer}details input[type=text]{margin-top:6px}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chip{margin:0;position:relative}.chip input,.seg input{position:absolute;opacity:0}
.chip span{display:inline-block;border:1.5px solid #999;border-radius:16px;padding:5px 11px;font-size:.92em;background:#fff;color:#333;cursor:pointer}
.chip input:checked+span{color:#fff;font-weight:600}.chip input:checked+span::before{content:"\2713  "}
.chip input:focus-visible+span,.seg input:focus-visible+span{outline:2px solid #06c}
.cS input:checked+span{background:#408335;border-color:#408335}.cU input:checked+span{background:#0065ae;border-color:#0065ae}
.cT input:checked+span{background:#d82020;border-color:#d82020}.cB input:checked+span{background:#00586a;border-color:#00586a}
.cZ input:checked+span{background:#555;border-color:#555}
.box{border:1px solid #ddd;border-radius:6px;padding:8px 10px;margin-top:8px;background:#fafafa}
.q{font-weight:600;margin:2px 0 4px;font-size:.95em}
.rad{display:flex;gap:8px;align-items:flex-start;margin:0;padding:5px 0}
.rad input{margin-top:3px;width:18px;height:18px;flex:none}
.dst{color:#555;font-size:.88em}
.seg{display:flex;border:1px solid #888;border-radius:6px;overflow:hidden}
.seg label{flex:1;margin:0;position:relative}
.seg span{display:block;text-align:center;padding:7px 4px;font-size:.92em;background:#fff;border-left:1px solid #888;cursor:pointer}
.seg label:first-child span{border-left:0}
.seg input:checked+span{background:#111;color:#fff;font-weight:600}
.seg small{display:block;font-weight:normal;opacity:.8;font-size:.82em}
.disp{font-family:ui-monospace,monospace;border:1.5px solid #111;border-radius:3px;padding:0 5px;font-size:.9em;white-space:nowrap;color:#111}
.add{width:100%;border-style:dashed;background:#fff;padding:12px}
.bar{position:fixed;left:0;right:0;bottom:0;background:#fff;border-top:1px solid #ccc;box-shadow:0 -2px 8px rgba(0,0,0,.08)}
.bar div{max-width:560px;margin:auto;padding:10px 12px;display:flex;align-items:center;gap:10px;box-sizing:border-box}
.bar span{flex:1;font-size:.92em;color:#555}.bar.dirty span{color:#a60;font-weight:600}
.bar button{background:#ccc;border-color:#ccc;color:#555;padding:10px 22px;font-weight:600}
.bar.dirty button,.bar.fail button{background:#0a5;border-color:#0a5;color:#fff}
.bar.saved span{color:#0a5;font-weight:600}.bar.fail span{color:#b00;font-weight:600}
.err{color:#b00;font-size:.9em;font-weight:600;margin:6px 0}
.box.need{border-color:#a60;background:#fff8e8}.box.need .zq::after{content:" – bitte auswählen";color:#a60}
.chip.na span{border-style:dashed;color:#999;background:#f4f4f4}.chip.na input:checked+span{opacity:.55}
.cb{display:flex;align-items:center;gap:8px;margin:6px 0}.cb input{width:20px;height:20px;flex:none}.cb label{margin:0}
.hit{border:1px solid #bbb;border-radius:6px;padding:8px;margin:6px 0;background:#fff;color:#111;font-size:1rem}
.hit .acts{display:flex;gap:12px;align-items:center;margin-top:6px}
.grp{font-weight:bold;margin:12px 0 2px;padding-bottom:2px;border-bottom:1px solid #ccc}
.ln{display:grid;grid-template-columns:1fr auto;gap:2px 8px;align-items:center;padding:6px 0;border-bottom:1px solid #eee}
.ln .cb{margin:0}.ln select{width:auto;padding:4px;font-size:.9em}.ln select:disabled{visibility:hidden}.nod .ln select{display:none}
.ln .dst{grid-column:1/3;padding-left:28px}
.ln.old{background:#fff6e0}
.lb{display:inline-block;min-width:2.2em;padding:1px 5px;border-radius:4px;color:#fff;font-weight:bold;text-align:center}
.tS{background:#408335}.tU{background:#0065ae}.tT{background:#d82020}.tB,.tR{background:#00586a}.tZ{background:#555}
.sum{margin:0 0 8px}
.spin{display:inline-block;width:12px;height:12px;border:2px solid #999;border-top-color:transparent;border-radius:50%;animation:r 1s linear infinite;vertical-align:-2px;margin-right:6px}
@keyframes r{to{transform:rotate(360deg)}}
</style></head><body><main>
<h1>Abfahrtsdisplay</h1>
<p class="hint">Hilfe zu allen Einstellungen: <a id="help" href="#" target="_blank">Anleitung (README)</a></p>
<form id="f" method="post" action="/speichern" onsubmit="return check()">
<section><h2><span class="no" id="no1">1</span><span>Station 1<span id="hn1"></span></span></h2><div id="st1"></div></section>
<section><div id="no2"><button type="button" class="add" onclick="show2(1)">+ Zweite Station hinzuf&uuml;gen</button>
<p class="hint">Eine Taste kann dann Station 2 zeigen.</p></div>
<div id="has2"><h2><span class="no">2</span><span>Station 2<span id="hn2"></span></span></h2><div id="st2"></div>
<p style="margin-top:14px"><button type="button" class="danger" onclick="show2(0)">Station 2 entfernen</button></p></div></section>
<input type="hidden" name="lines1" id="lines1"><input type="hidden" name="station2" id="station2">
<input type="hidden" name="types2" id="types2"><input type="hidden" name="lines2" id="lines2">
<input type="hidden" name="n1" id="n1"><input type="hidden" name="n2" id="n2">
<section><h2>Anzeige</h2><h3 style="margin-top:10px">Abfahrtszeit als</h3>
<div class="seg"><label><input type="radio" name="min" value="0" id="min0"><span>Uhrzeit<small>10:34 +2</small></span></label><label><input type="radio" name="min" value="1" id="min1"><span>Minuten<small>4 min</small></span></label></div></section>
<section><h2>Tasten</h2>
<div class="box"><div class="q">Standardansicht</div><span id="std"></span><p class="hint" style="margin:4px 0 0">ergibt sich aus Station 1</p></div>
<h3 style="margin-top:14px">Linke Taste</h3><select id="keyL" name="keyL" onchange="K.L=+this.value;upd()"></select><p class="hint" id="resL"></p>
<h3 style="margin-top:14px">Mittlere Taste</h3><select id="keyM" name="keyM" onchange="K.M=+this.value;upd()"></select><p class="hint" id="resM"></p>
<div id="qrOpts" class="box"><div class="q">WLAN-QR-Code <small style="font-weight:normal">(z.B. G&auml;ste-WLAN)</small></div>
<label for="qrTitle">&Uuml;berschrift</label><input type="text" id="qrTitle" name="qrTitle" maxlength="40">
<label for="qrSsid">WLAN-Name</label><input type="text" id="qrSsid" name="qrSsid" maxlength="32">
<label for="qrPass">Passwort</label><input type="text" id="qrPass" name="qrPass" maxlength="63" autocomplete="off">
<p class="err" id="eq"></p><p class="hint">Das Passwort steht f&uuml;r alle lesbar auf dem Display. Leer lassen = unver&auml;ndert.</p></div>
<p class="hint">Dieselbe Taste nochmal: zur&uuml;ck zur Standardansicht. Die andere Taste zeigt direkt ihre Ansicht. Mittlere Taste 3&nbsp;s halten: Systeminfo und Einstellportal.</p>
<div class="cb" style="margin-top:14px"><input type="checkbox" id="back" name="back" value="1"><label for="back">Tasten-Ansicht nach 30 s beenden</label></div>
<p class="hint">Station 2, andere Richtung oder Seite 2 wechseln dann von selbst zur&uuml;ck zur Standardansicht. Ohne H&auml;kchen bleiben sie bis zum n&auml;chsten Tastendruck.</p>
<p class="hint warn" id="kw"></p></section>
<div class="bar" id="bar"><div><span id="bst">Alles gespeichert</span><button type="submit">Speichern</button></div></div>
</form>
<section><h2>Ger&auml;t</h2>
<p>WLAN: <b id="wifi"></b> (Signal <span id="rssi"></span> dBm)<br>
<span class="hint">&Auml;ndern: im Web-Installer &bdquo;Change Wi-Fi&ldquo;.</span></p>
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
const TY=['sbahn','ubahn','tram','bus','bahn'],TN=['S-Bahn','U-Bahn','Tram','Bus','Regionalzug'],TK='SUTBZ';
const TC={SBAHN:'S',UBAHN:'U',TRAM:'T',BUS:'B',REGIONAL_BUS:'R',BAHN:'Z'},TB={S:0,U:1,T:2,B:3,R:3,Z:4};
const GN={S:'S-Bahn',U:'U-Bahn',T:'Tram',B:'Bus',R:'Regionalbus',Z:'Regionalzug'},DN={B:'beide',H:'nur H',R:'nur R'};
const MAX=16,NO='keine Fahrt in den nächsten 12 h';
const sel=[0,new Map(),new Map()],lst=[0,0,0],lt=[0,0,0],sid=['','',''],nmv=['','',''],EX=[0,{},{}],K={L:0,M:0},AV=[0,null,null],PM=['','',''];
let dirty=false;
function setDirty(){if(dirty)return;dirty=true;bar('dirty','Ungespeicherte Änderungen');}
function dv(n){return $('d'+n).value;}
function sd(n){const v=dv(n);return v=='H'||v=='R'?v:'B';}
function zv(n){const r=document.querySelector('input[name=z'+n+']:checked');return r?r.value:'';}
function per(n){return dv(n)=='I';}
function needZ(n){return'UHR'.includes(dv(n));}
function sv(){const r=document.querySelector('input[name=s1]:checked');return r?r.value:'H';}
function oth(d){return d=='H'?'R':'H';}
function block(n){
 let h='<div class="step"><span class="sn">1</span><div class="sb"><h3>Station</h3>'+
  '<div class="chosen" id="card'+n+'"><div><b id="cn'+n+'"></b><br><span class="hint" id="cp'+n+'"></span></div><button type="button" class="link" onclick="srch('+n+',1)">Ändern</button></div>'+
  '<div id="sr'+n+'"><div class="row"><input type="text" id="q'+n+'" placeholder="Name der Haltestelle, z.B. Marienplatz"><button type="button" onclick="search('+n+')">Suchen</button></div><div id="res'+n+'" class="hint"></div></div>'+
  '<details id="man'+n+'"><summary>Station-ID manuell eingeben</summary><input type="text" id="id'+n+'"'+(n==1?' name="station"':'')+' placeholder="z.B. de:09162:2" onchange="setSt('+n+')"><p class="hint" id="nm'+n+'"></p>'+
  '<p class="hint">Die Suche nutzt die inoffizielle MVG-API. Klappt sie nicht, die ID aus der Haltestellenliste eintragen (siehe Anleitung).</p></details><p class="err" id="es'+n+'"></p></div></div>'+
  '<div id="more'+n+'"><div class="step"><span class="sn">2</span><div class="sb"><h3>Verkehrsmittel</h3><div class="chips">';
 TY.forEach((t,i)=>{h+='<label class="chip c'+TK[i]+'" id="ch'+t+n+'"><input type="checkbox" id="'+t+n+'"'+(n==1?' name="'+t+'"':'')+' onchange="tyc('+n+')"><span>'+TN[i]+'</span></label>';});
 h+='</div><p class="hint" id="tna'+n+'">Gestrichelt: fährt an dieser Station nicht.</p><p class="err" id="et'+n+'"></p><p class="hint" id="tyh'+n+'">Mit Linienauswahl zeigt das Display nur die gewählten Linien.</p></div></div>'+
  '<div class="step"><span class="sn">3</span><div class="sb"><h3>Richtungsanzeige</h3><select id="d'+n+'" name="'+(n==1?'d1':'dir2')+'" onchange="dirc('+n+')"><option value="B">Alle Richtungen gemischt</option>'+
  (n==1?'<option value="U">Umschaltbar per Taste (H/R)</option>':'')+'<option value="H" id="oH'+n+'">Nur Richtung H</option><option value="R" id="oR'+n+'">Nur Richtung R</option><option value="I">Je Linie wählen (in der Linienauswahl)</option></select>'+
  '<div class="box" id="zb'+n+'"><div class="q zq">Welche Richtung fährt ins Zentrum?</div>';
 ['H','R'].forEach(d=>{h+='<label class="rad"><input type="radio" name="z'+n+'" value="'+d+'" onchange="upd()"><span><b>'+d+'</b> <span class="dst" id="ex'+d+n+'"></span></span></label>';});
 h+='<label class="rad"><input type="radio" name="z'+n+'" value="K" onchange="upd()"><span><b>Keine oder gemischt</b> <span class="dst">– ohne Richtungsanzeige im Display (z.B. Querverbindungen oder Linien mit verschiedenen Zentrum-Richtungen)</span></span></label>';
 if(n==1)h+='<div id="sb1"><div class="q" style="margin-top:8px">Startansicht</div><div class="seg">'+
  ['H','R'].map(d=>'<label><input type="radio" name="s1" value="'+d+'" onchange="upd()"><span id="sl'+d+'"></span></label>').join('')+'</div></div>';
 h+='<p class="err" id="ez'+n+'"></p></div><p class="hint" id="pv'+n+'"></p></div></div>'+
  '<div class="step"><span class="sn">4</span><div class="sb"><h3>Linien <small>(optional)</small></h3><p class="sum" id="sum'+n+'"></p><p><button type="button" id="lb'+n+'" onclick="lines('+n+')">Linien auswählen</button></p><div id="ls'+n+'"></div></div></div></div>';
 $('st'+n).innerHTML=h;
 $('q'+n).addEventListener('keydown',e=>{if(e.key=='Enter'){e.preventDefault();search(n);}});}
function srch(n,on){$('sr'+n).style.display=on?'':'none';$('card'+n).style.display=on||!$('id'+n).value.trim()?'none':'';if(on)$('q'+n).focus();}
function card(n,name,place){$('cn'+n).textContent=name||$('id'+n).value.trim();$('cp'+n).textContent=place||('ID '+$('id'+n).value.trim());srch(n,0);}
function parseSel(n,t){sel[n].clear();(t||'').split(',').forEach(e=>{const p=e.split(':');if(p.length==3)sel[n].set(p[0],{d:p[1],c:p[2],n:p[0]});});}
function selStr(n){return[...sel[n]].map(([k,v])=>k+':'+(per(n)?v.d:'B')+':'+v.c).join(',');}
function on2(){return $('has2').style.display!='none';}
// Richtungsname im Header: Zentrum/Auswärts, ohne Zuordnung bei umschaltbarer Richtung H/R
function lab(n,d){if(d!='H'&&d!='R')return'';const z=zv(n);if(z=='H'||z=='R')return d==z?'Zentrum':'Auswärts';return n==1&&dv(1)=='U'?d:'';}
function view(n,d,p2){const l=lab(n,d);return'<span class="disp">'+(on2()?n+' ':'')+esc(nmv[n]||'Station '+n)+(l?' → '+l:'')+(p2?' (2/2)':'')+'</span>';}
function startDir(){return dv(1)=='U'?sv():sd(1);}
function keyOpts(id,mid,v){const o=[];if(dv(1)=='U')o.push([1,'Richtung wechseln']);if(on2())o.push([2,'Station 2']);o.push([3,'Weitere Abfahrten (Seite 2)']);
 if(mid)o.push([4,'WLAN-QR-Code']);o.push([0,'Keine Funktion']);if(!o.some(x=>x[0]==v))v=3;
 $(id).innerHTML=o.map(x=>'<option value="'+x[0]+'">'+x[1]+'</option>').join('');$(id).value=v;return v;}
function keyRes(v){if(v==1)return'zeigt '+view(1,oth(sv()));if(v==2)return'zeigt '+view(2,sd(2));if(v==3)return'zeigt '+view(1,startDir(),1)+', Abfahrten 5–8';
 if(v==4)return'zeigt den WLAN-QR-Code (60 s)';return'ohne Funktion';}
// Freie Taste (Seite 2 oder ohne Funktion) mit einer neuen Funktion belegen
function assign(v){if(K.L==v||K.M==v)return;if(K.L==3||K.L==0)K.L=v;else if(K.M==3||K.M==0)K.M=v;}
function upd(){const t=on2();
 $('no1').style.display=t?'':'none';
 [1,2].forEach(n=>{const d=dv(n),has=!!$('id'+n).value.trim();$('more'+n).style.display=has?'':'none';
  $('zb'+n).style.display=needZ(n)?'':'none';$('zb'+n).classList.toggle('need',needZ(n)&&!zv(n));
  const l=d=='U'?lab(1,sv()):lab(n,d);
  $('pv'+n).innerHTML=!has?'':d=='B'?'Display: '+view(n,'B')+', alle Richtungen gemischt.':
   d=='I'?'Display: '+view(n,'B')+', Richtung je Linie wie in der Linienauswahl.':
   d=='U'?'Display: '+view(1,sv())+', eine Taste wechselt auf '+view(1,oth(sv()))+'.':
   'Display: '+view(n,d)+' (nur Fahrten Richtung '+d+(l?'':', ohne Richtungsanzeige')+').';
  if(has)ex(n);av(n);
  if($('ls'+n).querySelector('.ln'))draw(n);else sum(n);});
 $('sb1').style.display=dv(1)=='U'?'':'none';
 ['H','R'].forEach(d=>{const l=lab(1,d);$('sl'+d).textContent=l||d;});
 K.L=keyOpts('keyL',0,K.L);K.M=keyOpts('keyM',1,K.M);
 $('std').innerHTML=view(1,startDir());$('resL').innerHTML=keyRes(K.L);$('resM').innerHTML=keyRes(K.M);
 $('qrOpts').style.display=K.M==4?'':'none';
 const w=[];if(dv(1)=='U'&&K.L!=1&&K.M!=1)w.push('Station 1 ist umschaltbar, aber keine Taste wechselt die Richtung.');
 if(t&&K.L!=2&&K.M!=2)w.push('Station 2 ist keiner Taste zugeordnet.');
 $('kw').innerHTML=w.map(x=>'⚠ '+x).join('<br>');}
// Neue Richtungsanzeige: Zentrum-Frage offen lassen, wenn sie vorher nicht gestellt war
function dirc(n){const d=dv(n);if(needZ(n)&&!'UHR'.includes(PM[n])&&zv(n)=='K')document.querySelectorAll('input[name=z'+n+']').forEach(r=>{r.checked=false;});
 PM[n]=d;if(n==1&&d=='U')assign(1);setDirty();upd();}
// Verkehrsmittel, die an der Station nicht fahren (aus der Suche), gestrichelt
function av(n){const b=AV[n];let any=0;TY.forEach((t,i)=>{const na=b!=null&&!(b&(1<<i));any|=na;$('ch'+t+n).classList.toggle('na',na);});$('tna'+n).style.display=any?'':'none';}
function loadAv(n){if(!nmv[n])return;const id=$('id'+n).value.trim();
 fetch('/suche?q='+encodeURIComponent(nmv[n])).then(r=>r.json()).then(l=>{const s=l.find(x=>x.id==id);if(s&&s.b!=null&&$('id'+n).value.trim()==id){AV[n]=s.b;av(n);}}).catch(()=>{});}
function sum(n,note){const s=sel[n],d=sd(n),p=per(n);
 $('sum'+n).innerHTML=(s.size?'<b>'+s.size+' von max. '+MAX+' Linien'+(d=='B'?'':', nur Richtung '+d)+':</b> '+[...s.values()].map(v=>'<span class="lb t'+v.c+'">'+esc(v.n)+'</span>'+(p?' '+DN[v.d]:'')).join(p?', ':' '):'Keine Auswahl: alle Linien der gewählten Verkehrsmittel.')+(note?' <i>'+note+'</i>':'');
 $('tyh'+n).style.display=s.size?'':'none';
 $('ls'+n).querySelectorAll('.ln input').forEach(c=>{c.disabled=!c.checked&&s.size>=MAX;});
 const f=$('full'+n);if(f)f.style.display=s.size>=MAX?'':'none';}
function nm(n,t){nmv[n]=t||'';$('hn'+n).textContent=t?': '+t:'';}
function getName(n){const id=$('id'+n).value.trim();nm(n,'');if(!id)return Promise.resolve();
 return fetch('/name?id='+encodeURIComponent(id)).then(r=>r.json()).then(j=>{if(!j.n)throw 0;nm(n,j.n);card(n,j.n);upd();if(AV[n]==null)loadAv(n);})
 .catch(()=>{$('nm'+n).textContent='Name nicht gefunden – ID prüfen.';card(n,'',('ID '+id+' (Name nicht gefunden)'));});}
function setSt(n,name,place,bits){const v=$('id'+n).value.trim();if(v==sid[n])return;sid[n]=v;$('nm'+n).textContent='';setDirty();
 AV[n]=bits==null?null:bits;if(bits!=null)TY.forEach((t,i)=>{if(!(bits&(1<<i)))$(t+n).checked=false;});
 if(name!==undefined){nm(n,name);card(n,name,place);}else if(v)getName(n);else{nm(n,'');srch(n,1);}
 const had=sel[n].size>0;sel[n].clear();lst[n]=0;$('ls'+n).innerHTML='';EX[n]={};
 sum(n,had?'(Auswahl gelöscht: andere Station)':'');upd();}
function show2(on){$('no2').style.display=on?'none':'';$('has2').style.display=on?'':'none';
 if(on){if(!$('id2').value.trim())srch(2,1);assign(2);}
 else{$('id2').value='';$('d2').value='B';$('res2').innerHTML='';$('nm2').textContent='';setSt(2);}setDirty();upd();}
// Fehler direkt am Feld zeigen (erster Fehler wird angesprungen)
function err(id,msg){const e=$(id);e.textContent=msg;e.style.display='';return e;}
function check(){const t=on2(),E=[];document.querySelectorAll('.err').forEach(e=>{e.style.display='none';});
 for(const n of t?[1,2]:[1]){
  if(!$('id'+n).value.trim()){E.push(err('es'+n,'Bitte eine Station wählen.'));continue;}
  if(!TY.some(i=>$(i+n).checked)){if(sel[n].size)sel[n].forEach(v=>{$(TY[TB[v.c]]+n).checked=true;});else E.push(err('et'+n,'Mindestens ein Verkehrsmittel wählen.'));}
  if(needZ(n)&&!zv(n))E.push(err('ez'+n,'Bitte wählen, welche Richtung ins Zentrum fährt (oder „Keine oder gemischt“).'));}
 if(K.M==4&&!$('qrSsid').value.trim())E.push(err('eq','WLAN-Name für den QR-Code fehlt.'));
 if(E.length){E[0].scrollIntoView({block:'center'});bar('fail','Bitte die markierten Angaben ergänzen.');return false;}
 let b=0;TY.forEach((i,x)=>{if($(i+'2').checked)b|=1<<x;});
 $('lines1').value=selStr(1);$('station2').value=t?$('id2').value.trim():'';$('types2').value=b||S.types2;$('lines2').value=t?selStr(2):'';
 $('n1').value=nmv[1];$('n2').value=t?nmv[2]:'';
 save();return false;}
function bar(c,m){$('bar').className='bar '+c;$('bst').textContent=m;}
// Im Hintergrund speichern, die Seite bleibt offen
function save(){const B=$('bar').querySelector('button');B.disabled=true;bar('dirty','Speichert …');
 fetch('/speichern',{method:'POST',body:new URLSearchParams(new FormData($('f')))}).then(r=>r.text().then(x=>{
  const p=new DOMParser().parseFromString(x,'text/html').querySelector('p');
  if(r.ok){dirty=false;bar('saved','✓ Gespeichert – das Display übernimmt die Einstellungen.');}else bar('fail',(p&&p.textContent)||'Speichern fehlgeschlagen.');}))
 .catch(()=>{bar('fail','Keine Verbindung zum Display – bitte erneut versuchen.');}).finally(()=>{B.disabled=false;});}
function search(n){
 const q=$('q'+n).value.trim(),R=$('res'+n); if(q.length<2)return;
 R.textContent='Suche ...';
 fetch('/suche?q='+encodeURIComponent(q)).then(r=>r.json()).then(l=>{
  if(!l.length){R.textContent='Keine Station gefunden.';return;}
  R.innerHTML='';
  l.forEach(s=>{const d=document.createElement('div');d.className='hit';
   let h='<b>'+esc(s.n)+'</b><br>'+esc(s.p)+(s.z?' &middot; Zone '+esc(s.z.toUpperCase()):'')+'<br><span class="hint">'+esc(s.t)+'</span><div class="acts"><button type="button">Ausw&auml;hlen</button>';
   if(s.lat&&s.lon)h+='<a target="_blank" href="https://www.openstreetmap.org/?mlat='+s.lat+'&mlon='+s.lon+'#map=17/'+s.lat+'/'+s.lon+'">Auf Karte zeigen</a>';
   d.innerHTML=h+'</div>';
   d.querySelector('button').onclick=()=>{$('id'+n).value=s.id;R.innerHTML='';$('q'+n).value='';setSt(n,s.n,s.p+' · '+s.t,s.b);};
   R.appendChild(d);});
 }).catch(()=>{R.textContent='Suche fehlgeschlagen (MVG-API nicht erreichbar).';});}
function tb(n){let b=0;TY.forEach((t,i)=>{if($(t+n).checked)b|=1<<i;});sel[n].forEach(v=>{b|=1<<TB[v.c];});return b;}
// Beispielziele je Richtungskennung (aktuelle Abfahrten, bei Linienauswahl nur diese Linien)
function ex(n){const id=$('id'+n).value.trim(),b=tb(n);if(!id||!b)return;const k=id+'|'+b;
 if(EX[n].k==k){exShow(n);return;}EX[n]={k:k};['H','R'].forEach(d=>{$('ex'+d+n).textContent='→ Ziele werden geladen …';});
 fetch('/richtungen?id='+encodeURIComponent(id)+'&types='+b).then(r=>r.json()).then(l=>{if(EX[n].k!=k)return;if(!Array.isArray(l))throw 0;EX[n].l=l;exShow(n);})
 .catch(()=>{if(EX[n].k!=k)return;EX[n]={};['H','R'].forEach(d=>{$('ex'+d+n).textContent='→ (Ziele nicht abrufbar)';});});}
function exShow(n){const l=EX[n].l;if(!l)return;const s=sel[n];
 ['H','R'].forEach(d=>{let m=l.filter(e=>e.d==d);if(s.size){const f=m.filter(e=>s.has(e.l.replace(/ /g,'').toUpperCase()));if(f.length)m=f;}
  const z=[...new Set(m.map(e=>e.z))].slice(0,3);$('ex'+d+n).textContent='→ '+(z.length?z.join(', '):'gerade keine Fahrt');
  $('o'+d+n).textContent='Nur Richtung '+d+(z.length?' → '+z.slice(0,2).join(', '):'');});}
function lines(n){
 const id=$('id'+n).value.trim(),B=$('lb'+n),D=$('ls'+n),b=tb(n); if(!id){alert('Erst eine Station wählen.');return;}
 if(!b){alert('Erst Verkehrsmittel wählen.');return;}
 if(lst[n]&&lt[n]==b){draw(n);return;}
 B.disabled=true;D.innerHTML='<p class="hint"><span class="spin"></span>Lade Linien und Ziele der nächsten 12 Stunden (ca. 10 s) ...</p>';
 fetch('/linien?id='+encodeURIComponent(id)+'&types='+b).then(r=>r.json()).then(l=>{if(!Array.isArray(l))throw 0;lst[n]=l;lt[n]=b;draw(n);})
 .catch(()=>{D.innerHTML='<p class="hint">Abruf fehlgeschlagen (MVG-API nicht erreichbar). Bitte erneut versuchen.</p>';})
 .finally(()=>{B.disabled=false;});}
function sk(k){const m=k.match(/\d+/),c=/^N\d/.test(k)?2:/^X\d/.test(k)?1:0;return[c,m?+m[0]:1e9,k];}
function cmp(a,b){const x=sk(a.k),y=sk(b.k);for(let i=0;i<3;i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;return 0;}
function tyc(n){if($('ls'+n).querySelector('.ln'))$('ls'+n).innerHTML='<p class="hint">Verkehrsmittel geändert – „Linien auswählen“ lädt die Liste neu.</p>';upd();}
function done(n){$('ls'+n).innerHTML='';$('sum'+n).scrollIntoView({block:'center'});exShow(n);}
function draw(n){
 const d=sd(n),s=sel[n],L=lst[n].map(x=>({k:x.k,n:x.n,c:TC[x.t]||'B',H:x.H,R:x.R}));
 s.forEach((v,k)=>{const x=L.find(y=>y.k==k);if(x){v.n=x.n;v.c=x.c;}else L.push({k:k,n:v.n,c:v.c,old:1});});
 let h='<p class="hint">Linien anhaken, die das Display zeigen soll (max. '+MAX+'). Keine angehakt = alle Linien'+(d=='B'?'. Darunter Beispielziele je Richtungskennung H/R'+(per(n)?', rechts die Richtung je Linie.':'.'):', darunter Beispielziele in Richtung '+d+'.')+'</p><p class="hint" id="full'+n+'"><b>Maximum von '+MAX+' Linien erreicht.</b></p>';
 'SUTBRZ'.split('').forEach(c=>{const g=L.filter(x=>x.c==c).sort(cmp);if(!g.length)return;
  h+='<div class="grp">'+GN[c]+'</div>';
  g.forEach(x=>{const i=esc('l'+n+x.k);
   h+='<div class="ln'+(x.old?' old':'')+'" data-k="'+esc(x.k)+'"><div class="cb"><input type="checkbox" id="'+i+'"><label for="'+i+'"><span class="lb t'+c+'">'+esc(x.n)+'</span></label></div>'+
    '<select><option value="B">beide Richtungen</option><option value="H">nur H</option><option value="R">nur R</option></select><div class="dst">'+
    (x.old?'Nicht mehr im Fahrplan dieser Station. Haken entfernen zum Löschen.':d!='B'?d+' &rarr; '+esc(x[d]||NO):x.H||x.R?'H &rarr; '+esc(x.H||NO)+'<br>R &rarr; '+esc(x.R||NO):NO)+'</div></div>';});});
 h+='<p><button type="button" onclick="done('+n+')">Fertig</button></p>';
 const D=$('ls'+n);D.innerHTML=h;D.className=per(n)?'':'nod';
 D.querySelectorAll('.ln').forEach(r=>{const k=r.dataset.k,x=L.find(y=>y.k==k),c=r.querySelector('input'),e=r.querySelector('select');
  c.checked=s.has(k);e.disabled=!c.checked;if(c.checked)e.value=s.get(k).d;
  c.onchange=()=>{e.disabled=!c.checked;if(c.checked)s.set(k,{d:e.value,c:x.c,n:x.n});else s.delete(k);sum(n);};
  e.onchange=()=>{if(s.has(k))s.get(k).d=e.value;sum(n);};});
 sum(n);}
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
block(1);block(2);
$('help').href=S.help;$('id1').value=sid[1]=S.station;$('id2').value=sid[2]=S.station2;
$('d1').value=S.d1;$('d2').value=S.dir2;
[['z1',S.z1],['z2',S.z2],['s1',S.s1],['min',S.min?'1':'0']].forEach(([n,v])=>{const r=document.querySelector('input[name='+n+'][value="'+v+'"]');if(r)r.checked=true;});
K.L=S.keyL;K.M=S.keyM;$('back').checked=S.back!==false;
TY.forEach((t,i)=>{$(t+'1').checked=S[t];$(t+'2').checked=!!(S.types2&(1<<i));});
$('qrTitle').value=S.qrTitle;$('qrSsid').value=S.qrSsid;if(S.qrPassSet)$('qrPass').placeholder='unverändert';
parseSel(1,S.lines1);parseSel(2,S.lines2);
// Gespeicherte Richtung je Linie (nur bei „alle Richtungen“ moeglich) = „Je Linie wählen“
[1,2].forEach(n=>{if(dv(n)=='B'&&[...sel[n].values()].some(v=>v.d!='B'))$('d'+n).value='I';PM[n]=dv(n);});
document.querySelectorAll('.err').forEach(e=>{e.style.display='none';});
$('no2').style.display=S.station2?'none':'';$('has2').style.display=S.station2?'':'none';
[1,2].forEach(n=>{if($('id'+n).value.trim())card(n,'','');else srch(n,1);});
sum(1);sum(2);upd();
// Gespeicherte Namen sofort zeigen, sonst beim Display abfragen
const nmP=[1,2].reduce((p,n)=>{const v=n==1?S.name1:S.name2;if(!$('id'+n).value.trim())return p;
 if(v){nm(n,v);card(n,v);loadAv(n);return p;}return p.then(()=>getName(n));},Promise.resolve());
upd();
$('f').addEventListener('change',e=>{if(!/^q\d/.test(e.target.id))setDirty();});$('f').addEventListener('input',e=>{if(!/^q\d/.test(e.target.id))setDirty();});
$('wifi').textContent=S.wifi;$('rssi').textContent=S.rssi;$('version').textContent=S.version;$('ip').textContent=S.ip;
$('closes').textContent=S.closes?'Diese Seite ist bis '+S.closes+' Uhr erreichbar (erneut öffnen: mittlere Taste 3 s halten).':'';
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

// Formularwert aus einem Zeichen; nicht in allowed: fallback
static char formChar(const char* name, const char* allowed, char fallback) {
  String value = server->arg(name);
  if (value.length() == 1 && strchr(allowed, value[0]) != nullptr) return value[0];
  return fallback;
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
    doc["lines1"] = s.lines1;
    doc["name1"] = s.name1;
    doc["name2"] = s.name2;
    doc["station2"] = s.station2Id;
    doc["types2"] = s.types2;
    doc["lines2"] = s.lines2;
    doc["dir2"] = String(s.dir2);
    // Richtung je Station, Tasten, Abfahrtszeit (ab 2.3.0)
    doc["d1"] = String(s.dir1);
    doc["z1"] = String(s.zentrum1);
    doc["s1"] = String(s.start1);
    doc["z2"] = String(s.zentrum2);
    doc["keyL"] = s.keyLeft;
    doc["keyM"] = s.keyMid;
    doc["min"] = s.showMinutes;
    doc["back"] = s.viewAutoBack;   // ab 2.4.0
    doc["sbahn"] = s.showSbahn;
    doc["ubahn"] = s.showUbahn;
    doc["tram"] = s.showTram;
    doc["bus"] = s.showBus;
    doc["bahn"] = s.showBahn;
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
  // Richtung Station 1 (d1): B alle, U umschaltbar, H/R nur diese
  // Kennung; Zentrum-Kennung z1/z2: H, R oder K (keine); Startansicht s1
  // H/R. Tasten keyL/keyM (BTN_...), Abfahrtszeit min 0/1, Auto-Reset
  // back (Haekchen). Unbrauchbare
  // Werte korrigiert settingsSave() (validate).
  s.dir1 = formChar("d1", "BUHR", 'B');
  s.zentrum1 = formChar("z1", "HRK", 'K');
  s.start1 = formChar("s1", "HR", 'H');
  s.zentrum2 = formChar("z2", "HRK", 'K');
  s.keyLeft = (uint8_t)server->arg("keyL").toInt();
  s.keyMid = (uint8_t)server->arg("keyM").toInt();
  s.showMinutes = server->arg("min") == "1";
  s.viewAutoBack = server->hasArg("back");
  s.showSbahn = server->hasArg("sbahn");
  s.showUbahn = server->hasArg("ubahn");
  s.showTram = server->hasArg("tram");
  s.showBus = server->hasArg("bus");
  s.showBahn = server->hasArg("bahn");
  if (!(s.showSbahn || s.showUbahn || s.showTram || s.showBus || s.showBahn)) {
    sendMessage(400, "Mindestens ein Verkehrsmittel w&auml;hlen.");
    return;
  }

  // Linienauswahl und Station 2: nur uebernehmen, wenn die Felder
  // mitgeschickt werden (sonst bleiben die gespeicherten Werte)
  if (server->hasArg("lines1")) {
    LineSelection sel;
    sel.parse(formArg("lines1", 600));
    s.lines1 = sel.toString();
  }
  // Stationsnamen (UTF-8, wie im Portal angezeigt), "" = unbekannt
  if (server->hasArg("n1")) s.name1 = formArg("n1", 60);
  if (server->hasArg("n2")) s.name2 = formArg("n2", 60);
  if (server->hasArg("station2")) {
    String station2 = formArg("station2", 40);
    if (station2.length() > 0 && !validStationId(station2)) {
      sendMessage(400, "Station 2 ist ung&uuml;ltig.");
      return;
    }
    s.station2Id = station2;
  }
  if (server->hasArg("types2")) {
    int types2 = server->arg("types2").toInt() & TYPE_ALL;
    if (types2 == 0) {
      sendMessage(400, "Station 2: mindestens ein Verkehrsmittel w&auml;hlen.");
      return;
    }
    s.types2 = (uint8_t)types2;
  }
  if (server->hasArg("dir2")) s.dir2 = formChar("dir2", "HR", 'B');
  if (server->hasArg("lines2")) {
    LineSelection sel;
    sel.parse(formArg("lines2", 600));
    s.lines2 = sel.toString();
  }

  s.qrTitle = formArg("qrTitle", 40);
  s.qrSsid = formArg("qrSsid", 32);
  String qrPass = server->arg("qrPass");   // Leerzeichen gehoeren zum Passwort
  if (qrPass.length() > 63) qrPass = qrPass.substring(0, 63);
  if (qrPass.length() > 0) s.qrPassword = qrPass;   // leer = unveraendert
  if (s.keyMid == BTN_WIFI_QR && s.qrSsid.length() == 0) {
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
  uint8_t types = (uint8_t)(server->arg("types").toInt() & TYPE_ALL);
  if (!validStationId(id) || !listDirections(id.c_str(), types, json)) {
    server->send(502, "application/json", "{\"error\":\"Abruf fehlgeschlagen\"}");
    return;
  }
  server->send(200, "application/json; charset=utf-8", json);
}

static void handleName() {
  String id = formArg("id", 40);
  String name;
  if (!validStationId(id) || !fetchStationName(id.c_str(), name, false)) {
    server->send(502, "application/json", "{\"error\":\"Abruf fehlgeschlagen\"}");
    return;
  }
  JsonDocument doc;
  doc["n"] = name;
  String json;
  serializeJson(doc, json);
  server->send(200, "application/json; charset=utf-8", json);
}

static void handleLines() {
  String id = formArg("id", 40);
  String json;
  // Verkehrsmittel als TYPE_...-Bits (fehlt/0 = alle)
  uint8_t types = (uint8_t)(server->arg("types").toInt() & TYPE_ALL);
  if (!validStationId(id) || !listStationLines(id.c_str(), types, json)) {
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
  server->on("/name", HTTP_GET, handleName);
  server->on("/linien", HTTP_GET, handleLines);
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
