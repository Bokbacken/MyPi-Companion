// Auto-generated header — do NOT place raw-string HTML in .ino files.
// This file is included by web_server.ino.
#pragma once

// ============================================================
//  DASHBOARD (main page)
//  - MyPi Companion logo banner
//  - Live status card (Pi + ESP)
//  - 3×2 navigation button grid
// ============================================================
static const char PAGE_TMPL[] PROGMEM = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MyPi Companion</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:12px 16px;display:flex;align-items:center;gap:12px}
.hdr img{height:40px;image-rendering:pixelated}
.hdr h1{font-size:20px}.hdr small{color:#F47B20;font-size:11px;display:block}
.card{background:#161b22;margin:12px;border-radius:8px;padding:14px;border:1px solid #30363d}
.lbl{font-size:11px;color:#8b949e;margin-bottom:3px}
.val{font-size:20px;color:#3fb950;font-weight:700}
.ip{font-size:12px;color:#8b949e;margin-top:3px}
.esps{font-size:16px;color:#58a6ff;font-weight:700}
.div{border-top:1px solid #21262d;margin-top:10px;padding-top:8px}
.grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin:0 12px 12px}
.gbtn{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:14px 6px;text-align:center;text-decoration:none;color:#e6e6e6;display:block;transition:border-color .15s,background .15s}
.gbtn:hover{border-color:#F47B20;background:#1f2937}
.gbtn .ic{font-size:24px;display:block;margin-bottom:6px}
.gbtn .tx{font-size:11px;color:#8b949e;font-weight:600;line-height:1.3}
footer{text-align:center;padding:12px;font-size:11px;color:#484f58}
.wavesep{line-height:0;display:block;margin:0;padding:0}
.bitsep{display:flex;align-items:center;gap:8px;margin:12px 0 8px}
.bs-b{letter-spacing:3px;color:#4CC870;font-size:13px;white-space:nowrap}
.bs-l{flex:1;height:1px;background:linear-gradient(90deg,rgba(76,200,112,0.4),rgba(76,200,112,0.07),rgba(76,200,112,0.4))}
.bs-t{font-size:9px;letter-spacing:3px;color:#4a8a5a;padding:0 6px;white-space:nowrap}
</style></head><body>
<div class="hdr">
<img src="/logo.png" alt="MyPi"><h1>MyPi Companion</h1>
</div>
<div class="wavesep" style="line-height:0;display:block;margin:0;padding:0"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>
<div class="card">
<div class="lbl">Pi network</div>
<div class="val" id="cur-ssid">---</div>
<div class="ip" id="cur-conn"></div>
<div class="ip"  id="cur-ip">---</div>
<div class="div">
<div class="lbl">ESP network</div>
<div class="esps" id="esp-ssid">---</div>
<div class="ip"   id="esp-ip">---</div>
</div>
</div>
<div class="bitsep">
  <span class="bs-b">◎◉◉◎◎◎◎◉◎◉</span>
  <span class="bs-l"></span>
  <span class="bs-t">COMPANION</span>
  <span class="bs-l"></span>
  <span class="bs-b">◎◎◉◉◉◎◉◉◎◉</span>
</div>
<div class="grid">
<a class="gbtn" href="/networks"><span class="ic">📡</span><span class="tx">Pi Networks</span></a>
<a class="gbtn" href="/profiles"><span class="ic">🔐</span><span class="tx">Profile Manager</span></a>
<a class="gbtn" href="/terminal" target="_blank"><span class="ic">🖥️</span><span class="tx">Terminal</span></a>
<a class="gbtn" href="/installonpi"><span class="ic">📦</span><span class="tx">Install on Pi</span></a>
<a class="gbtn" href="/update"><span class="ic">📤</span><span class="tx">OTA Update</span></a>
<a class="gbtn" href="/info"><span class="ic">ℹ️</span><span class="tx">System Info</span></a>
<a class="gbtn" href="/ap-profiles"><span class="ic">🔌</span><span class="tx">AP Profiles</span></a>
</div>
<footer>MyPi Companion &bull; rigconsole.local</footer>
<script>
function poll(){
  fetch('/status').then(r=>r.json()).then(function(d){
    document.getElementById('cur-ssid').textContent=d.ssid||'---';
    document.getElementById('cur-conn').textContent=d.conn&&d.conn!=='---'?'('+d.conn+')':'';
    document.getElementById('cur-ip').textContent=d.ip||'---';
    document.getElementById('esp-ssid').textContent=d.espssid||'---';
    document.getElementById('esp-ip').textContent=d.espip||'---';
  }).catch(function(){});
  setTimeout(poll,3000);
}
poll();
</script></body></html>)html";


// ============================================================
//  PI NETWORKS PAGE  (/networks)
//  Lists Pi's known profiles + connect buttons.
//  Form at bottom to add a new network.
//  All data fetched client-side from /api/piprofiles + /status.
// ============================================================
static const char NETWORKS_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pi Networks — MyPi Companion</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:10px 14px;display:flex;align-items:center;gap:10px}
.hdr img{height:34px;image-rendering:pixelated}
.back{font-size:12px;color:#F47B20;text-decoration:none;display:block}
.hdr h1{font-size:17px}
.card{background:#161b22;margin:10px 12px;border-radius:8px;padding:14px;border:1px solid #30363d}
.lbl{font-size:11px;color:#8b949e;margin-bottom:3px}
.val{font-size:17px;color:#3fb950;font-weight:700}
.ip{font-size:12px;color:#8b949e;margin-top:2px}
h2{font-size:14px;color:#c9d1d9;margin-bottom:10px}
.row{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid #21262d}
.row:last-child{border:none}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block;margin-right:7px}
.on{background:#3fb950}.off{background:#484f58}
.name{font-size:14px}
.btn{padding:7px 12px;border:none;border-radius:6px;cursor:pointer;font-size:12px;font-weight:600}
.bc{background:#F47B20;color:#fff}.ba{background:#238636;color:#fff}
input{width:100%;padding:9px;border-radius:6px;border:1px solid #30363d;background:#0d1117;color:#e6e6e6;font-size:14px;margin-bottom:8px;display:block}
.msg{margin:0 12px 8px;padding:10px;border-radius:6px;font-size:13px;background:#0f3520;color:#3fb950;display:none}
.err{background:#3a0f0f!important;color:#f85149!important}
footer{text-align:center;padding:10px;font-size:11px;color:#484f58}
.wavesep{line-height:0;display:block;margin:0;padding:0}
</style></head><body>
<div class="hdr">
<img src="/logo.png" alt="MyPi"><div><a class="back" href="/">← Dashboard</a><h1>Pi Networks</h1></div>
</div>
<div class="wavesep" style="line-height:0;display:block;margin:0;padding:0"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>
<div class="card">
<div class="lbl">Active Pi network</div>
<div class="val" id="cur-ssid">---</div>
<div class="ip"  id="cur-ip">---</div>
</div>
<div class="card">
<h2>Select network</h2>
<div id="plist"><p style="color:#8b949e;padding:6px 0">Loading profiles...</p></div>
</div>
<div class="card">
<h2>&#43; Add new network</h2>
<form onsubmit="addWifi(event)">
<input type="text"     id="nssid" placeholder="SSID" required>
<input type="password" id="npass" placeholder="Password (blank = open network)">
<button type="submit" class="btn bc" style="width:100%;padding:11px">Add to Pi</button>
</form>
</div>
<div class="msg" id="msg"></div>
<footer><a href="/" style="color:#484f58">MyPi Companion</a></footer>
<script>
var polling=true,curSsid='',curConn='';
function showMsg(t,e){var m=document.getElementById('msg');m.textContent=t;m.className='msg'+(e?' err':'');m.style.display='block';}
function hideMsg(){document.getElementById('msg').style.display='none';}
function esc(s){return String(s).replace(/[&<>"']/g,function(c){return '&#'+c.charCodeAt(0)+';';});}
// names = NM connection names. Netplan-generated ones ("netplan-wlan0-TOFTA")
// are shown without the prefix but connected by their full name.
function buildList(names,piOk){
  var h='';
  names.forEach(function(n,i){
    var active=(n===curConn);
    var label=n.replace(/^netplan-[^-]+-/,'');
    h+='<div class="row"><span><span class="dot '+(active?'on':'off')+'"></span><span class="name">'+esc(label)+'</span></span>';
    if(active)h+='<button class="btn ba">Active</button>';
    else h+='<button class="btn bc" onclick="conn(PN['+i+'])">Connect</button>';
    h+='</div>';
  });
  window.PN=names;
  if(!h)h='<p style="color:#8b949e">'+(piOk?'No Wi-Fi profiles on the Pi (loading…)':'No profiles — Pi not connected')+'</p>';
  document.getElementById('plist').innerHTML=h;
}
function poll(){
  if(!polling)return;
  var piOk=false;
  fetch('/status').then(r=>r.json()).then(function(d){
    curSsid=d.ssid||'';
    curConn=d.conn||'';
    piOk=!!d.pi;
    document.getElementById('cur-ssid').textContent=d.ssid||'---';
    document.getElementById('cur-ip').textContent=d.ip||'---';
    return fetch('/api/piconns');   // NM connection names, as used by CMD:CONNECT
  }).then(r=>r.json()).then(function(names){
    buildList(names, piOk);
  }).catch(function(){});
  setTimeout(poll,3000);
}
function conn(p){
  polling=false;
  fetch('/connect?profile='+encodeURIComponent(p)).then(function(){
    showMsg('Connecting to: '+p);
    setTimeout(function(){hideMsg();polling=true;poll();},4000);
  });
}
function addWifi(e){
  e.preventDefault();
  var s=document.getElementById('nssid').value.trim();
  var p=document.getElementById('npass').value;
  if(!s)return;
  fetch('/addwifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)})
  .then(r=>r.text()).then(function(t){
    showMsg(t,false);
    document.getElementById('nssid').value='';
    document.getElementById('npass').value='';
    setTimeout(hideMsg,4000);
  });
}
poll();
</script></body></html>)html";


// ============================================================
//  INSTALL ON PI PAGE  (/installonpi)
//  Confirmation page before running HID installer.
// ============================================================
static const char INSTALLPI_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Install on Pi — MyPi Companion</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:10px 14px;display:flex;align-items:center;gap:10px}
.hdr img{height:34px;image-rendering:pixelated}
.back{font-size:12px;color:#F47B20;text-decoration:none;display:block}
.hdr h1{font-size:17px}
.card{background:#161b22;margin:10px 12px;border-radius:8px;padding:16px;border:1px solid #30363d}
.ic{font-size:40px;display:block;text-align:center;margin-bottom:10px}
h2{font-size:16px;color:#c9d1d9;margin-bottom:8px;text-align:center}
p{font-size:13px;color:#8b949e;margin-bottom:12px;line-height:1.5}
ul{font-size:13px;color:#8b949e;padding-left:18px;margin-bottom:14px;line-height:1.7}
.btn{width:100%;padding:13px;background:#F47B20;color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:15px;font-weight:700}
.btn:hover{background:#e06010}
.btn:disabled{background:#484f58;cursor:not-allowed}
.msg{margin-top:12px;padding:10px;border-radius:6px;font-size:13px;background:#0f3520;color:#3fb950;display:none;text-align:center}
.err{background:#3a0f0f!important;color:#f85149!important}
.seg{display:flex;gap:8px;margin-bottom:12px}.seg label{flex:1;background:#0d1117;border:1px solid #30363d;border-radius:6px;padding:9px;font-size:13px;cursor:pointer;text-align:center}
.fl{display:block;font-size:12px;color:#8b949e;margin:8px 0 4px}
.in{width:100%;padding:9px;background:#0d1117;color:#e6e6e6;border:1px solid #30363d;border-radius:6px;font-size:14px;margin-bottom:6px}
footer{text-align:center;padding:10px;font-size:11px;color:#484f58}
.wavesep{line-height:0;display:block;margin:0;padding:0}
.progress{margin-top:10px;padding:10px 12px;background:#0a0e13;border:1px solid #30363d;border-radius:6px;font-family:monospace;font-size:13px;color:#58a6ff;display:none;line-height:1.6}
.progress.err{color:#f85149;border-color:#6e1f1f}
</style></head><body>
<div class="hdr">
<img src="/logo.png" alt="MyPi"><div><a class="back" href="/">← Dashboard</a><h1>Install on Pi</h1></div>
</div>
<div class="wavesep" style="line-height:0;display:block;margin:0;padding:0"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>
<div class="card">
<span class="ic">📦</span>
<h2>MyPi Companion daemon installer</h2>
<p>Installs the MyPi Companion daemon on the connected Raspberry Pi via USB HID keyboard emulation.</p>
<div class="seg">
<label><input type="radio" name="mode" value="desktop" checked onchange="upd()"> 🖥️ Desktop</label>
<label><input type="radio" name="mode" value="cli" onchange="upd()"> ⌨️ CLI / headless</label>
</div>
<ul id="hdesk">
<li>Pi logged in with a desktop session</li>
<li>A terminal is opened with Ctrl+Alt+T</li>
</ul>
<div id="hcli" style="display:none">
<ul>
<li>Raspberry Pi OS Lite (or any Pi on the text console)</li>
<li>The ESP32 logs in on the console, installs, then logs out</li>
<li>The password is kept in RAM only for this install and never stored</li>
</ul>
<label class="fl">Pi username</label>
<input class="in" id="user" autocomplete="username" maxlength="32">
<label class="fl">Pi password</label>
<input class="in" id="pass" type="password" autocomplete="current-password" maxlength="64">
</div>
<label class="fl">Pi keyboard layout</label>
<select class="in" id="layout">
<option value="-1">Auto-detect</option>
<option value="0">English (US)</option>
<option value="1">Swedish / Danish</option>
<option value="2">German</option>
<option value="3">French</option>
<option value="4">Spanish / Italian / Portuguese</option>
</select>
<ul><li>Takes about 1–2 minutes — do not disconnect USB</li></ul>
<button class="btn" id="btn" onclick="install()">Install / Reinstall on Pi</button>
<div class="msg" id="msg"></div>
<div class="progress" id="prog"></div>
</div>
<footer><a href="/" style="color:#484f58">MyPi Companion</a></footer>
<script>
function $(i){return document.getElementById(i);}
function mode(){return document.querySelector('input[name=mode]:checked').value;}
function upd(){var c=mode()==='cli';$('hcli').style.display=c?'block':'none';$('hdesk').style.display=c?'none':'block';}
fetch('/api/installcfg').then(r=>r.json()).then(function(c){
  document.querySelector('input[name=mode][value='+(c.mode==1?'cli':'desktop')+']').checked=true;
  $('user').value=c.user||'';$('layout').value=String(c.layout);upd();
}).catch(function(){});
function show(t,err){var m=$('msg');m.textContent=t;m.className=err?'msg err':'msg';m.style.display='block';}
function install(){
  var b=$('btn'),f=new URLSearchParams();
  f.append('mode',mode());f.append('layout',$('layout').value);
  if(mode()==='cli'){
    var u=$('user').value.trim(),p=$('pass').value;
    if(!u||!p){show('Enter username and password',true);return;}
    if(/[^\x20-\x7e]/.test(u+p)){show('Only plain ASCII characters can be typed by the HID keyboard',true);return;}
    f.append('user',u);f.append('pass',p);
  }
  b.disabled=true;b.textContent='Starting...';
  fetch('/api/installonpi',{method:'POST',body:f})
  .then(function(r){return r.text().then(function(t){return {ok:r.ok,t:t};});})
  .then(function(x){
    $('pass').value='';show(x.t,!x.ok);
    if(x.ok){b.textContent='Installation started';startPoll();setTimeout(function(){b.disabled=false;b.textContent='Install / Reinstall on Pi';},20000);}
    else{b.disabled=false;b.textContent='Install / Reinstall on Pi';}
  }).catch(function(){
    show('Connection error',true);b.disabled=false;b.textContent='Install / Reinstall on Pi';
  });
}
var _pt=null;
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function startPoll(){
  var pg=$('prog');pg.style.display='block';
  if(_pt)clearInterval(_pt);
  _pt=setInterval(function(){
    fetch('/api/installstatus').then(function(r){return r.json();}).then(function(d){
      pg.className=d.err?'progress err':'progress';
      pg.innerHTML=(d.line0?'<span>'+esc(d.line0)+'</span><br>':'')+(d.line1?'<span>'+esc(d.line1)+'</span>':'');
      if(!d.running){clearInterval(_pt);_pt=null;}
    }).catch(function(){});
  },1000);
}
</script></body></html>)html";


// ============================================================
//  SYSTEM INFO PAGE  (/info)
//  Polls /api/info every 5 s and displays diagnostics.
// ============================================================
static const char SYSINFO_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>System Info — MyPi Companion</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:10px 14px;display:flex;align-items:center;gap:10px}
.hdr img{height:34px;image-rendering:pixelated}
.back{font-size:12px;color:#F47B20;text-decoration:none;display:block}
.hdr h1{font-size:17px}
.card{background:#161b22;margin:10px 12px;border-radius:8px;padding:14px;border:1px solid #30363d}
h2{font-size:13px;color:#8b949e;margin-bottom:10px;text-transform:uppercase;letter-spacing:.05em}
.row{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px solid #21262d;font-size:13px}
.row:last-child{border:none}
.k{color:#8b949e}
.v{color:#e6e6e6;font-weight:600;text-align:right}
.on{color:#3fb950}.off{color:#f85149}
.acc{color:#F47B20}
.badge{display:inline-block;padding:1px 7px;border-radius:10px;font-size:11px;background:#0f3520;color:#3fb950}
.badge.err{background:#3a0f0f;color:#f85149}
footer{text-align:center;padding:10px;font-size:11px;color:#484f58}
.ts{font-size:10px;color:#484f58;text-align:right;margin:0 12px 4px}
.wavesep{line-height:0;display:block;margin:0;padding:0}
</style></head><body>
<div class="hdr">
<img src="/logo.png" alt="MyPi"><div><a class="back" href="/">← Dashboard</a><h1>System Info</h1></div>
</div>
<div class="wavesep" style="line-height:0;display:block;margin:0;padding:0"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>
<div class="ts" id="ts">Updating...</div>
<div class="card">
<h2>ESP32</h2>
<div class="row"><span class="k">Firmware</span><span class="v acc" id="fw">---</span></div>
<div class="row"><span class="k">Uptime</span><span class="v" id="uptime">---</span></div>
<div class="row"><span class="k">Free heap</span><span class="v" id="heap">---</span></div>
<div class="row"><span class="k">Sketch size</span><span class="v" id="sketch">---</span></div>
<div class="row"><span class="k">Free OTA space</span><span class="v" id="free">---</span></div>
</div>
<div class="card">
<h2>Connectivity</h2>
<div class="row"><span class="k">Pi link</span><span class="v" id="pi-link">---</span></div>
<div class="row"><span class="k">USB port</span><span class="v acc" id="usb-port">---</span></div>
<div class="row"><span class="k">Pi network</span><span class="v on" id="pi-ssid">---</span></div>
<div class="row"><span class="k">Pi profile</span><span class="v" id="pi-conn">---</span></div>
<div class="row"><span class="k">Pi IP</span><span class="v" id="pi-ip">---</span></div>
<div class="row"><span class="k">ESP network</span><span class="v" id="esp-ssid">---</span></div>
<div class="row"><span class="k">ESP IP</span><span class="v" id="esp-ip">---</span></div>
</div>
<div class="card">
<h2>Raspberry Pi</h2>
<div id="pi-sys"><div class="row"><span class="k">No data yet</span><span class="v">---</span></div></div>
</div>
<footer><a href="/" style="color:#484f58">MyPi Companion</a></footer>
<script>
// STATUS:SYS keys → label + formatter; keys not listed here are shown raw
var SYS={
 host:['Hostname',function(v){return v},'acc'],
 model:['Model'],os:['OS'],code:null,
 arch:['Architecture'],kernel:['Kernel'],daemon:['Daemon',function(v){return 'v'+v}],
 up:['Uptime',function(v){v=+v;var d=Math.floor(v/86400),h=Math.floor(v/3600)%24,m=Math.floor(v/60)%60;return (d?d+'d ':'')+h+'h '+('0'+m).slice(-2)+'m'}],
 temp:['CPU temp',function(v){return v+' °C'},function(v){return v>=80?'off':v>=70?'acc':''}],
 load:['Load (1 min)'],
 mem:['RAM used',function(v){return v+' MB'}],
 disk:['Disk / used',function(v){return v+' GB'}],
 thr:['Power',function(v){var f=parseInt(v,16);return f&1?'UNDER-VOLTAGE now':f&14?'THROTTLED now':f&0x10000?'Under-voltage since boot':f&0xE0000?'Throttled since boot':'OK'},
      function(v){var f=parseInt(v,16);return f&15?'off':f&0xF0000?'acc':'on'}],
 wifi:['Wi-Fi signal',function(v){return v+' dBm'},function(v){return v<=-80?'off':v<=-70?'acc':''}],
 ntp:['Clock',function(v){return v=='yes'?'NTP synced':'Not synced'},function(v){return v=='yes'?'':'acc'}]
};
function esc(t){return String(t).replace(/[&<>"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function renderSys(d){
  var el=document.getElementById('pi-sys'),h='';
  if(!d.pi_sys||!d.pi_sys.length){
    h='<div class="row"><span class="k">'+(d.pi_connected?'Daemon too old — reinstall on the Pi':'Pi offline')+'</span><span class="v">---</span></div>';
  }else{
    var stale=!d.pi_connected||d.pi_sys_age>20;
    d.pi_sys.forEach(function(kv){
      var k=kv[0],v=kv[1],m=SYS[k];
      if(m===null)return;
      var lab=m?m[0]:k,txt=m&&m[1]?m[1](v):v,cls=m&&m[2]?(typeof m[2]=='function'?m[2](v):m[2]):'';
      if(stale)cls='';
      h+='<div class="row"><span class="k">'+esc(lab)+'</span><span class="v '+cls+'">'+esc(txt)+'</span></div>';
    });
    if(stale)h+='<div class="row"><span class="k off">Stale data</span><span class="v">'+(d.pi_sys_age>=0?d.pi_sys_age+' s old':'')+'</span></div>';
  }
  el.innerHTML=h;
}
function fmtUptime(ms){
  var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);
  s%=60;m%=60;
  return h+'h '+('0'+m).slice(-2)+'m '+('0'+s).slice(-2)+'s';
}
function fmtBytes(b){
  if(b>=1024*1024)return (b/1024/1024).toFixed(1)+' MB';
  if(b>=1024)return (b/1024).toFixed(0)+' KB';
  return b+' B';
}
function set(id,v,cls){var e=document.getElementById(id);if(e){e.textContent=v;if(cls){e.className='v '+cls;}}}
function poll(){
  fetch('/api/info').then(r=>r.json()).then(function(d){
    var now=new Date();
    document.getElementById('ts').textContent='Updated: '+now.toLocaleTimeString();
    set('fw', d.fw_version ? 'v'+d.fw_version+(d.fw_date?' ('+d.fw_date+')':'') : '---');
    set('uptime', fmtUptime(d.millis));
    set('heap',   fmtBytes(d.heap));
    set('sketch', fmtBytes(d.sketch_size));
    set('free',   fmtBytes(d.free_sketch));
    set('pi-link', d.pi_connected?'Online':'Offline', d.pi_connected?'on':'off');
    set('usb-port', d.usb_port||'---');
    set('pi-ssid', d.pi_ssid||'---', d.pi_connected?'on':'');
    set('pi-conn', d.pi_conn||'---');
    set('pi-ip',   d.pi_ip||'---');
    set('esp-ssid', d.esp_ssid||'---', d.esp_connected?'acc':'');
    set('esp-ip',  d.esp_ip||'---');
    renderSys(d);
  }).catch(function(){
    document.getElementById('ts').textContent='Error fetching data';
  });
  setTimeout(poll,5000);
}
poll();
</script></body></html>)html";


// ============================================================
//  OTA UPDATE PAGE  (PROGMEM)
// ============================================================
static const char OTA_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA Update — MyPi Companion</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto;padding:0}
.hdr{background:#1a3a6b;padding:10px 14px;display:flex;align-items:center;gap:10px}
.hdr img{height:34px;image-rendering:pixelated}
.back{font-size:12px;color:#F47B20;text-decoration:none;display:block}
.hdr h1{font-size:17px}
.card{background:#161b22;border-radius:8px;padding:16px;border:1px solid #30363d;margin:10px 12px}
label{display:block;margin-bottom:8px;color:#8b949e;font-size:13px}
input[type=file]{width:100%;padding:8px;border-radius:6px;border:1px solid #30363d;background:#0d1117;color:#e6e6e6;margin-bottom:12px}
button{width:100%;padding:11px;background:#F47B20;color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:14px;font-weight:600}
button:hover{background:#e06010}
button:disabled{background:#484f58;cursor:not-allowed}
#progress{display:none;margin-top:16px}
progress{width:100%;height:20px;border-radius:4px}
#pct{text-align:center;margin-top:6px;font-size:13px;color:#8b949e}
#result{margin-top:12px;padding:10px;border-radius:6px;font-size:14px;font-weight:600;display:none;text-align:center}
.ok{background:#0f3520;color:#3fb950}
.err{background:#3a0f0f;color:#f85149}
footer{text-align:center;padding:10px;font-size:11px;color:#484f58}
.wavesep{line-height:0;display:block;margin:0;padding:0}
</style></head><body>
<div class="hdr">
<img src="/logo.png" alt="MyPi"><div><a class="back" href="/">← Dashboard</a><h1>OTA Update</h1></div>
</div>
<div class="wavesep" style="line-height:0;display:block;margin:0;padding:0"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>
<div class="card">
<label>Select compiled firmware (.bin):</label>
<form id="upf">
<input type="file" id="fw" accept=".bin" required>
<button type="submit" id="upbtn">&#x1F4E4; Upload &amp; Flash</button>
</form>
<div id="progress">
<progress id="bar" value="0" max="100"></progress>
<div id="pct">Uploading...</div>
</div>
<div id="result"></div>
</div>
<footer><a href="/" style="color:#484f58">MyPi Companion</a></footer>
<script>
// OTA upload progress driven by XHR upload events.
// Rationale: the ESP32 sync WebServer is blocked during the multipart POST and
// cannot answer /api/otaprogress GET requests while writing chunks to flash.
// XHR upload.onprogress fires independently of server responses; since the ESP32
// writes to flash on each received chunk, upload bytes ~= flash bytes => accurate
// progress without polling.  /api/otaprogress is retained on the server for debug.
document.getElementById('upf').onsubmit=function(e){
  e.preventDefault();
  var f=document.getElementById('fw').files[0];
  if(!f)return;
  var bar=document.getElementById('bar');
  var pct=document.getElementById('pct');
  var res=document.getElementById('result');
  document.getElementById('upbtn').disabled=true;
  document.getElementById('progress').style.display='block';
  res.style.display='none';
  bar.value=0;pct.textContent='Uploading...';

  function showOk(){
    document.getElementById('upbtn').disabled=false;
    res.style.display='block';res.className='result ok';
    res.textContent='Flash successful! Device is rebooting...';
    bar.value=100;pct.textContent='Done!';
  }
  function showErr(msg){
    document.getElementById('upbtn').disabled=false;
    res.style.display='block';res.className='result err';
    res.textContent=msg;
  }

  var xhr=new XMLHttpRequest();
  xhr.upload.onprogress=function(ev){
    if(ev.lengthComputable){
      var p=Math.round(ev.loaded/ev.total*100);
      bar.value=p;
      pct.textContent=p<100?'Flashing: '+p+'%':'Verifying...';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){showOk();}
    else{showErr('Error '+xhr.status+': '+xhr.responseText);}
  };
  xhr.onerror=function(){showErr('Connection lost during upload.');};
  var fd=new FormData();
  fd.append('firmware',f,f.name);
  xhr.open('POST','/update');
  xhr.send(fd);
};
</script></body></html>)html";

