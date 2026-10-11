// Auto-generated header — do NOT place raw-string HTML in .ino files.
// This file is included by web_server.ino.
#pragma once

// ============================================================
//  DASHBOARD (main page)
//  - HMS logo banner
//  - Live status card (Pi + ESP)
//  - 3×2 navigation button grid
// ============================================================
static const char PAGE_TMPL[] PROGMEM = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HMS Rig Console</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:12px 16px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:12px}
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
</style></head><body>
<div class="hdr">
<img src="/hms_logo.png" alt="HMS">
<div><small>HMS Networks</small><h1>Rig Console</h1></div>
</div>
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
<div class="grid">
<a class="gbtn" href="/networks"><span class="ic">📡</span><span class="tx">Pi Networks</span></a>
<a class="gbtn" href="/profiles"><span class="ic">🔐</span><span class="tx">Profile Manager</span></a>
<a class="gbtn" href="/terminal" target="_blank"><span class="ic">🖥️</span><span class="tx">Terminal</span></a>
<a class="gbtn" href="/installonpi"><span class="ic">📦</span><span class="tx">Install on Pi</span></a>
<a class="gbtn" href="/update"><span class="ic">📤</span><span class="tx">OTA Update</span></a>
<a class="gbtn" href="/info"><span class="ic">ℹ️</span><span class="tx">System Info</span></a>
</div>
<footer>HMS Rig Console &bull; rigconsole.local</footer>
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
<title>Pi Networks — HMS Rig Console</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:10px 14px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:10px}
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
</style></head><body>
<div class="hdr">
<img src="/hms_logo.png" alt="HMS">
<div><a class="back" href="/">← Dashboard</a><h1>Pi Networks</h1></div>
</div>
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
<footer><a href="/" style="color:#484f58">HMS Rig Console</a></footer>
<script>
var polling=true,curSsid='',curConn='';
function showMsg(t,e){var m=document.getElementById('msg');m.textContent=t;m.className='msg'+(e?' err':'');m.style.display='block';}
function hideMsg(){document.getElementById('msg').style.display='none';}
function buildList(profiles){
  var h='';
  profiles.forEach(function(p){
    var active=(p.ssid===curConn);  // p.ssid is the NM connection name; compare with curConn
    h+='<div class="row"><span><span class="dot '+(active?'on':'off')+'"></span><span class="name">'+p.ssid+'</span></span>';
    if(active)h+='<button class="btn ba">Active</button>';
    else h+='<button class="btn bc" onclick="conn(\''+p.ssid+'\')">Connect</button>';
    h+='</div>';
  });
  if(!h)h='<p style="color:#8b949e">No profiles — Pi not connected</p>';
  document.getElementById('plist').innerHTML=h;
}
function poll(){
  if(!polling)return;
  fetch('/status').then(r=>r.json()).then(function(d){
    curSsid=d.ssid||'';
    curConn=d.conn||'';
    document.getElementById('cur-ssid').textContent=d.ssid||'---';
    document.getElementById('cur-ip').textContent=d.ip||'---';
    return fetch('/api/piprofiles');
  }).then(r=>r.json()).then(buildList).catch(function(){});
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
<title>Install on Pi — HMS Rig Console</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:10px 14px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:10px}
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
</style></head><body>
<div class="hdr">
<img src="/hms_logo.png" alt="HMS">
<div><a class="back" href="/">← Dashboard</a><h1>Install on Pi</h1></div>
</div>
<div class="card">
<span class="ic">📦</span>
<h2>Rig Console daemon installer</h2>
<p>Installs the HMS Rig Console daemon on the connected Raspberry Pi via USB HID keyboard emulation.</p>
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
</div>
<footer><a href="/" style="color:#484f58">HMS Rig Console</a></footer>
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
    if(x.ok){b.textContent='Installation started';setTimeout(function(){b.disabled=false;b.textContent='Install / Reinstall on Pi';},20000);}
    else{b.disabled=false;b.textContent='Install / Reinstall on Pi';}
  }).catch(function(){
    show('Connection error',true);b.disabled=false;b.textContent='Install / Reinstall on Pi';
  });
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
<title>System Info — HMS Rig Console</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto}
.hdr{background:#1a3a6b;padding:10px 14px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:10px}
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
</style></head><body>
<div class="hdr">
<img src="/hms_logo.png" alt="HMS">
<div><a class="back" href="/">← Dashboard</a><h1>System Info</h1></div>
</div>
<div class="ts" id="ts">Updating...</div>
<div class="card">
<h2>ESP32</h2>
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
<footer><a href="/" style="color:#484f58">HMS Rig Console</a></footer>
<script>
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
<title>OTA Update — HMS Rig Console</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:480px;margin:0 auto;padding:0}
.hdr{background:#1a3a6b;padding:10px 14px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:10px}
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
</style></head><body>
<div class="hdr">
<img src="/hms_logo.png" alt="HMS">
<div><a class="back" href="/">← Dashboard</a><h1>OTA Update</h1></div>
</div>
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
<footer><a href="/" style="color:#484f58">HMS Rig Console</a></footer>
<script>
document.getElementById('upf').onsubmit=function(e){
  e.preventDefault();
  var f=document.getElementById('fw').files[0];
  if(!f)return;
  document.getElementById('upbtn').disabled=true;
  document.getElementById('progress').style.display='block';
  document.getElementById('result').style.display='none';
  var xhr=new XMLHttpRequest();
  xhr.upload.onprogress=function(ev){
    if(ev.lengthComputable){
      var p=Math.round(ev.loaded/ev.total*100);
      document.getElementById('bar').value=p;
      document.getElementById('pct').textContent='Uploading: '+p+'%';
    }
  };
  xhr.onload=function(){
    document.getElementById('upbtn').disabled=false;
    var r=document.getElementById('result');
    r.style.display='block';
    if(xhr.status===200){
      document.getElementById('bar').value=100;
      document.getElementById('pct').textContent='Done!';
      r.className='result ok';
      r.textContent='Flash successful! Device is rebooting...';
    }else{
      r.className='result err';
      r.textContent='Error '+xhr.status+': '+xhr.responseText;
    }
  };
  xhr.onerror=function(){
    document.getElementById('upbtn').disabled=false;
    var r=document.getElementById('result');
    r.style.display='block';r.className='result err';
    r.textContent='Connection lost during upload.';
  };
  // Must be multipart: the server's upload handler only receives
  // multipart/form-data (a raw body is routed to the raw handler instead).
  var fd=new FormData();
  fd.append('firmware',f,f.name);
  xhr.open('POST','/update');
  xhr.send(fd);
};
</script></body></html>)html";

