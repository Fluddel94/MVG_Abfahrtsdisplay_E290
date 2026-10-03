// portal.cpp
// Einstellungsportal (siehe portal.h).
//
// Seiten: "/" Formular, "/speichern" (POST), "/werkseinstellungen" (POST),
// "/suche?q=" Stationssuche (JSON), "/richtungen?id=&types=" Linien je
// Richtungskennung (JSON, max. 15), "/name?id=" Stationsname (JSON),
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
main{max-width:560px;margin:auto;padding:12px}
h1{font-size:1.4em;margin:8px 0}
section{background:#fff;border-radius:8px;padding:12px;margin:12px 0}
h2{font-size:1.1em;margin:0 0 8px}h3{font-size:1em;margin:16px 0 4px}
label{display:block;margin:8px 0 4px}
input[type=text],select{width:100%;box-sizing:border-box;padding:8px;font-size:1em}
.row{display:flex;gap:8px}.row input{flex:1}
button{padding:9px 14px;font-size:1em;border-radius:6px;border:1px solid #888;background:#eee}
button.main{background:#0a5;color:#fff;border-color:#0a5;width:100%;padding:12px}
button.danger{background:#fff;color:#b00;border-color:#b00}
.hint{color:#555;font-size:.9em}
.cb{display:flex;align-items:center;gap:8px;margin:6px 0}.cb input{width:20px;height:20px;flex:none}.cb label{margin:0}

.hit{border:1px solid #bbb;border-radius:6px;padding:8px;margin:6px 0;background:#fff;color:#111;font-size:1rem}
.hit .acts{display:flex;gap:12px;align-items:center;margin-top:6px}
table{border-collapse:collapse;width:100%;font-size:.9em}td,th{border-bottom:1px solid #ddd;padding:4px;text-align:left}
.grp{font-weight:bold;margin:12px 0 2px;padding-bottom:2px;border-bottom:1px solid #ccc}
.ln{display:grid;grid-template-columns:1fr auto;gap:2px 8px;align-items:center;padding:6px 0;border-bottom:1px solid #eee}
.ln .cb{margin:0}.ln select{width:auto;padding:4px;font-size:.9em}.ln select:disabled{visibility:hidden}.nod .ln select{display:none}
.ln .dst{grid-column:1/3;padding-left:28px;color:#555;font-size:.85em}
.ln.old{background:#fff6e0}
.lb{display:inline-block;min-width:2.2em;padding:1px 5px;border-radius:4px;color:#fff;font-weight:bold;text-align:center}
.tS{background:#408335}.tU{background:#0065ae}.tT{background:#d82020}.tB,.tR{background:#00586a}.tZ{background:#555}
.sum{margin:4px 0}
.spin{display:inline-block;width:12px;height:12px;border:2px solid #999;border-top-color:transparent;border-radius:50%;animation:r 1s linear infinite;vertical-align:-2px;margin-right:6px}
@keyframes r{to{transform:rotate(360deg)}}
</style></head><body><main>
<h1>Abfahrtsdisplay</h1>
<p class="hint">Hilfe zu allen Einstellungen: <a id="help" href="#" target="_blank">Anleitung (README)</a></p>
<form method="post" action="/speichern" onsubmit="return check()">
<section><h2>Station 1<span id="hn1"></span></h2><div id="st1"></div></section>
<section><h2>Station 2<span id="hn2"></span> <span class="hint" id="opt2">(optional)</span></h2>
<div id="no2"><p class="hint">Mit einer zweiten Station wechselt die BOOT-Taste zwischen beiden Stationen (nach 30&nbsp;s zur&uuml;ck zu Station 1).</p>
<button type="button" onclick="show2(1)">Zweite Station hinzuf&uuml;gen</button></div>
<div id="has2"><div id="st2"></div><p><button type="button" class="danger" onclick="show2(0)">Station 2 entfernen</button></p></div>
</section>
<input type="hidden" name="lines1" id="lines1"><input type="hidden" name="station2" id="station2">
<input type="hidden" name="types2" id="types2"><input type="hidden" name="lines2" id="lines2">
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
const TY=['sbahn','ubahn','tram','bus','bahn'],TN=['S-Bahn','U-Bahn','Tram','Bus (Stadt- und Regionalbus)','Regionalzug'];
const TC={SBAHN:'S',UBAHN:'U',TRAM:'T',BUS:'B',REGIONAL_BUS:'R',BAHN:'Z'},TB={S:0,U:1,T:2,B:3,R:3,Z:4};
const GN={S:'S-Bahn',U:'U-Bahn',T:'Tram',B:'Bus',R:'Regionalbus',Z:'Regionalzug'},DN={B:'beide',H:'nur H',R:'nur R'};
const MAX=16,NO='keine Fahrt in den nächsten 12 h';
const sel=[0,new Map(),new Map()],lst=[0,0,0],lt=[0,0,0],sid=['','',''];
function sd(n){const v=$(n==1?'dirView':'dir2').value;return'HR'.includes(v)?v:'B';}
function tb(n){let b=0;TY.forEach((t,i)=>{if($(t+n).checked)b|=1<<i;});sel[n].forEach(v=>{b|=1<<TB[v.c];});return b;}
function block(n){
 let h='<label for="q'+n+'">Station suchen</label><div class="row"><input type="text" id="q'+n+'" placeholder="z.B. Marienplatz"><button type="button" onclick="search('+n+')">Suchen</button></div><div id="res'+n+'" class="hint"></div>'+
  '<label for="id'+n+'">Station-ID (globalId)</label><input type="text" id="id'+n+'"'+(n==1?' name="station"':'')+' placeholder="z.B. de:09162:2" onchange="setSt('+n+')"><p class="hint" id="nm'+n+'"></p>';
 if(n==1)h+='<p class="hint">Die Suche nutzt die inoffizielle MVG-API. Klappt sie nicht, die ID aus der Haltestellenliste eintragen (siehe Anleitung).</p>';
 h+='<h3>Verkehrsmittel</h3><div id="ty'+n+'">';
 TY.forEach((t,i)=>{h+='<div class="cb"><input type="checkbox" id="'+t+n+'"'+(n==1?' name="'+t+'"':'')+' onchange="tyc('+n+')"><label for="'+t+n+'">'+TN[i]+'</label></div>';});
 h+='</div><p class="hint" id="tyh'+n+'">Es sind Linien gewählt: Das Display zeigt nur diese Linien. Die Verkehrsmittel legen dann nur fest, welche Linien die Liste anbietet.</p><h3>Richtung</h3><select id="'+(n==1?'dirView" name="dirView':'dir2" name="dir2')+'" onchange="upd()">'+
  (n==1?'<option value="0">alle Richtungen</option><option value="1">getrennt: Zentrum / Auswärts (H/R)</option>':'<option value="B">alle Richtungen</option>')+
  '<option value="H">nur Richtung H</option><option value="R">nur Richtung R</option></select><p class="hint">H und R sind die Richtungskennungen der MVG.'+(n==1?'<span id="gh"> „Getrennt“ zeigt Zentrum oder Auswärts, die BOOT-Taste schaltet um. Welche Kennung Richtung Zentrum fährt, legst du dann darunter fest.</span>':'')+'</p><p class="hint" id="dh'+n+'">Gilt für alle Linien der Station.</p>';
 if(n==1)h+='<p class="hint" id="viewHint">Mit zwei Stationen wechselt die BOOT-Taste die Station, getrennte Ansicht und Seite 2 gibt es dann nicht.</p>'+
  '<div id="dirOpts"><label for="zentrum">Richtung Zentrum hat die Kennung</label><select id="zentrum" name="zentrum"><option value="H">H</option><option value="R">R</option></select>'+
  '<label for="defView">Standardansicht</label><select id="defView" name="defView"><option value="Z">Zentrum</option><option value="A">Auswärts</option></select></div>';
 h+='<p><button type="button" onclick="dirs('+n+')">Richtungen anzeigen</button></p><div id="dl'+n+'" class="hint"></div>'+
  '<h3>Linien</h3><p class="sum" id="sum'+n+'"></p><p><button type="button" id="lb'+n+'" onclick="lines('+n+')">Linien auswählen</button></p><div id="ls'+n+'"></div>';
 $('st'+n).innerHTML=h;
 $('q'+n).addEventListener('keydown',e=>{if(e.key=='Enter'){e.preventDefault();search(n);}});}
function parseSel(n,t){sel[n].clear();(t||'').split(',').forEach(e=>{const p=e.split(':');if(p.length==3)sel[n].set(p[0],{d:p[1],c:p[2],n:p[0]});});}
function selStr(n){const d=sd(n);return[...sel[n]].map(([k,v])=>k+':'+(d=='B'?v.d:'B')+':'+v.c).join(',');}
function on2(){return $('has2').style.display!='none';}
function upd(){const t=on2(),v=$('dirView'),o=v.options[1];o.hidden=o.disabled=t;if(t&&v.value=='1')v.value='0';
 $('viewHint').style.display=t?'':'none';$('gh').style.display=t?'none':'';$('opt2').style.display=t?'none':'';$('dirOpts').style.display=v.value=='1'?'':'none';
 $('dh1').style.display='HR'.includes(v.value)?'':'none';$('dh2').style.display=$('dir2').value!='B'?'':'none';
 $('qrOpts').style.display=$('qrOn').checked?'':'none';
 [1,2].forEach(n=>{if($('ls'+n).querySelector('.ln'))draw(n);else sum(n);});}
function sum(n,note){const s=sel[n],d=sd(n);
 $('sum'+n).innerHTML=(s.size?'<b>'+s.size+' von max. '+MAX+' Linien'+(d=='B'?'':', nur Richtung '+d)+':</b> '+[...s.values()].map(v=>'<span class="lb t'+v.c+'">'+esc(v.n)+'</span>'+(d=='B'?' '+DN[v.d]:'')).join(d=='B'?', ':' '):'Keine Auswahl: alle Linien der gewählten Verkehrsmittel.')+(note?' <i>'+note+'</i>':'');
 $('tyh'+n).style.display=s.size?'':'none';
 $('ls'+n).querySelectorAll('.ln input').forEach(c=>{c.disabled=!c.checked&&s.size>=MAX;});
 const f=$('full'+n);if(f)f.style.display=s.size>=MAX?'':'none';}
function nm(n,t){$('hn'+n).textContent=t?': '+t:'';}
function getName(n){const id=$('id'+n).value.trim();nm(n,'');if(!id)return Promise.resolve();
 return fetch('/name?id='+encodeURIComponent(id)).then(r=>r.json()).then(j=>{if(!j.n)throw 0;nm(n,j.n);})
 .catch(()=>{$('nm'+n).textContent='Name nicht gefunden – ID prüfen.';});}
function setSt(n,name){const v=$('id'+n).value.trim();if(v==sid[n])return;sid[n]=v;$('nm'+n).textContent='';if(name!==undefined)nm(n,name);else getName(n);
 const had=sel[n].size>0;sel[n].clear();lst[n]=0;$('ls'+n).innerHTML='';
 sum(n,had?'(Auswahl gelöscht: andere Station)':'');$('dl'+n).textContent='';}
function show2(on){$('no2').style.display=on?'none':'';$('has2').style.display=on?'':'none';
 if(!on){$('id2').value='';$('dir2').value='B';$('res2').innerHTML='';$('nm2').textContent='';setSt(2);}upd();}
function check(){const t=on2();
 if(!$('id1').value.trim()){alert('Bitte Station 1 wählen.');return false;}
 if(t&&!$('id2').value.trim()){alert('Bitte Station 2 wählen oder entfernen.');return false;}
 for(const n of t?[1,2]:[1]){if(!TY.some(i=>$(i+n).checked)){
  if(!sel[n].size){alert('Station '+n+': mindestens ein Verkehrsmittel wählen.');return false;}
  sel[n].forEach(v=>{$(TY[TB[v.c]]+n).checked=true;});}}
 if($('qrOn').checked&&!$('qrSsid').value.trim()){alert('WLAN-Name für den QR-Code fehlt.');return false;}
 let b=0;TY.forEach((i,x)=>{if($(i+'2').checked)b|=1<<x;});
 $('lines1').value=selStr(1);$('station2').value=t?$('id2').value.trim():'';$('types2').value=b||S.types2;$('lines2').value=t?selStr(2):'';
 return true;}
function search(n){
 const q=$('q'+n).value.trim(),R=$('res'+n); if(q.length<2)return;
 R.textContent='Suche ...';
 fetch('/suche?q='+encodeURIComponent(q)).then(r=>r.json()).then(l=>{
  if(!l.length){R.textContent='Keine Station gefunden.';return;}
  R.innerHTML='';
  l.forEach(s=>{const d=document.createElement('div');d.className='hit';
   let h='<b>'+esc(s.n)+'</b><br>Ort: '+esc(s.p)+(s.z?' &middot; Zone '+esc(s.z.toUpperCase()):'')+
    '<br>'+esc(s.t)+'<br><span class="hint">ID '+esc(s.id)+'</span><div class="acts"><button type="button">Ausw&auml;hlen</button>';
   if(s.lat&&s.lon)h+='<a target="_blank" href="https://www.openstreetmap.org/?mlat='+s.lat+'&mlon='+s.lon+'#map=17/'+s.lat+'/'+s.lon+'">Auf Karte zeigen</a>';
   d.innerHTML=h+'</div>';
   d.querySelector('button').onclick=()=>{$('id'+n).value=s.id;R.innerHTML='';setSt(n,s.n);$('nm'+n).textContent='Gewählt: '+s.n+', '+s.p+' ('+s.id+')';};
   R.appendChild(d);});
 }).catch(()=>{R.textContent='Suche fehlgeschlagen (MVG-API nicht erreichbar).';});}
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
function tyc(n){if($('ls'+n).querySelector('.ln'))$('ls'+n).innerHTML='<p class="hint">Verkehrsmittel geändert – „Linien auswählen“ lädt die Liste neu.</p>';}
function done(n){$('ls'+n).innerHTML='';$('sum'+n).scrollIntoView({block:'center'});}
function draw(n){
 const d=sd(n),s=sel[n],L=lst[n].map(x=>({k:x.k,n:x.n,c:TC[x.t]||'B',H:x.H,R:x.R}));
 s.forEach((v,k)=>{const x=L.find(y=>y.k==k);if(x){v.n=x.n;v.c=x.c;}else L.push({k:k,n:v.n,c:v.c,old:1});});
 let h='<p class="hint">Linien anhaken, die das Display zeigen soll (max. '+MAX+'). Keine angehakt = alle Linien. Gezeigt werden die Linien der angehakten Verkehrsmittel'+(d=='B'?'. H und R sind die Richtungskennungen der MVG, darunter Beispielziele.':', darunter Beispielziele in Richtung '+d+'.')+'</p><p class="hint" id="full'+n+'"><b>Maximum von '+MAX+' Linien erreicht.</b></p>';
 'SUTBRZ'.split('').forEach(c=>{const g=L.filter(x=>x.c==c).sort(cmp);if(!g.length)return;
  h+='<div class="grp">'+GN[c]+'</div>';
  g.forEach(x=>{const i=esc('l'+n+x.k);
   h+='<div class="ln'+(x.old?' old':'')+'" data-k="'+esc(x.k)+'"><div class="cb"><input type="checkbox" id="'+i+'"><label for="'+i+'"><span class="lb t'+c+'">'+esc(x.n)+'</span></label></div>'+
    '<select><option value="B">beide Richtungen</option><option value="H">nur H</option><option value="R">nur R</option></select><div class="dst">'+
    (x.old?'Nicht mehr im Fahrplan dieser Station. Haken entfernen zum Löschen.':d!='B'?d+' &rarr; '+esc(x[d]||NO):x.H||x.R?'H &rarr; '+esc(x.H||NO)+'<br>R &rarr; '+esc(x.R||NO):NO)+'</div></div>';});});
 h+='<p><button type="button" onclick="done('+n+')">Fertig</button></p>';
 const D=$('ls'+n);D.innerHTML=h;D.className=d=='B'?'':'nod';
 D.querySelectorAll('.ln').forEach(r=>{const k=r.dataset.k,x=L.find(y=>y.k==k),c=r.querySelector('input'),d=r.querySelector('select');
  c.checked=s.has(k);d.disabled=!c.checked;if(c.checked)d.value=s.get(k).d;
  c.onchange=()=>{d.disabled=!c.checked;if(c.checked)s.set(k,{d:d.value,c:x.c,n:x.n});else s.delete(k);sum(n);};
  d.onchange=()=>{if(s.has(k))s.get(k).d=d.value;sum(n);};});
 sum(n);}
function dirs(n){
 const id=$('id'+n).value.trim(),D=$('dl'+n); if(!id){alert('Erst eine Station wählen.');return;}
 const b=tb(n);if(!b){alert('Erst Verkehrsmittel wählen.');return;}
 D.textContent='Lade Abfahrten ...';
 fetch('/richtungen?id='+encodeURIComponent(id)+'&types='+b).then(r=>r.json()).then(l=>{
  if(!l.length){D.textContent='Keine Abfahrten gefunden.';return;}
  let h='<table><tr><th>Kennung</th><th>Linie</th><th>Ziel</th></tr>';
  l.forEach(d=>{h+='<tr><td>'+esc(d.d)+'</td><td>'+esc(d.l)+'</td><td>'+esc(d.z)+'</td></tr>';});
  D.innerHTML=h+'</table>';
 }).catch(()=>{D.textContent='Abruf fehlgeschlagen.';});}
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
$('dirView').value=S.dir1!='B'?S.dir1:S.dirView?'1':'0';$('dir2').value=S.dir2;$('zentrum').value=S.zentrumIsH?'H':'R';$('defView').value=S.defZentrum?'Z':'A';
TY.forEach((t,i)=>{$(t+'1').checked=S[t];$(t+'2').checked=!!(S.types2&(1<<i));});
$('qrOn').checked=S.qrOn;$('qrTitle').value=S.qrTitle;$('qrSsid').value=S.qrSsid;if(S.qrPassSet)$('qrPass').placeholder='unverändert';
parseSel(1,S.lines1);parseSel(2,S.lines2);sum(1);sum(2);show2(S.station2?1:0);
getName(1).then(()=>{if(S.station2)getName(2);});
$('wifi').textContent=S.wifi;$('rssi').textContent=S.rssi;$('version').textContent=S.version;$('ip').textContent=S.ip;
$('closes').textContent=S.closes?'Diese Seite ist bis '+S.closes+' Uhr erreichbar (erneut öffnen: Taste 3 s halten).':'';
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
    doc["lines1"] = s.lines1;
    doc["dir1"] = String(s.dir1);
    doc["station2"] = s.station2Id;
    doc["types2"] = s.types2;
    doc["lines2"] = s.lines2;
    doc["dir2"] = String(s.dir2);
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
  // "Richtung" Station 1: 0 = alle, 1 = getrennt (Zentrum/Auswaerts),
  // H/R = nur diese Richtungskennung
  String view = server->arg("dirView");
  s.directionView = view == "1";
  s.dir1 = (view == "H" || view == "R") ? view[0] : 'B';
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

  // Linienauswahl und Station 2: nur uebernehmen, wenn die Felder
  // mitgeschickt werden (sonst bleiben die gespeicherten Werte)
  if (server->hasArg("lines1")) {
    LineSelection sel;
    sel.parse(formArg("lines1", 600));
    s.lines1 = sel.toString();
  }
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
  if (server->hasArg("dir2")) {
    String dir2 = server->arg("dir2");
    s.dir2 = (dir2 == "H" || dir2 == "R") ? dir2[0] : 'B';
  }
  if (server->hasArg("lines2")) {
    LineSelection sel;
    sel.parse(formArg("lines2", 600));
    s.lines2 = sel.toString();
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
