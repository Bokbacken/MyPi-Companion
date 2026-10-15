// ============================================================
//  ap_profiles_html.h  —  AP profile editor page
//  Served at GET /ap-profiles
//
//  API:
//    GET  /api/approfiles              — [{slot,name,ssid,pass:"********",...}]
//    GET  /api/approfile/getpass?slot=N — {"pass":"actual"} (Reveal button only)
//    POST /api/approfile/save          — slot, name, ssid, [pass], subnet, nat, onBoot, hideOnBoot
//    POST /api/apstart                 — slot
//    GET  /api/apstatus[?test=1]       — AP/NAT/uplink diagnostics
// ============================================================
#pragma once

static const char AP_PROFILES_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MyPi Companion — AP Profiles</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:#0d1117;color:#e6e6e6;font-size:14px;max-width:640px;margin:0 auto}
.hdr{background:#1a3a6b;padding:12px 16px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:12px}
.hdr a{color:#F47B20;text-decoration:none;font-size:20px;line-height:1}
.hdr img{height:34px;image-rendering:pixelated}
.hdr h1{font-size:17px;font-weight:600}
.wavesep{line-height:0;display:block;margin:0;padding:0}
/* 2-column grid on desktop, 1 column on mobile */
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px;padding:12px}
@media(max-width:479px){.grid2{grid-template-columns:1fr}}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:14px}
.card.active{border-color:#F47B20}
.card h2{font-size:13px;color:#F47B20;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center}
.badge{font-size:10px;background:#F47B20;color:#000;border-radius:4px;padding:2px 6px;display:none}
.card.active .badge{display:inline-block}
label{display:block;font-size:11px;color:#8b949e;margin:8px 0 3px}
input[type=text],input[type=password],select{width:100%;background:#0d1117;color:#e6e6e6;border:1px solid #30363d;border-radius:5px;padding:6px 8px;font-size:13px}
input:focus,select:focus{outline:none;border-color:#F47B20}
.chk{display:flex;align-items:center;gap:8px;margin-top:8px;font-size:13px}
.chk input[type=checkbox]{width:auto;flex:none}
.btns{display:flex;gap:8px;margin-top:12px}
button{flex:1;padding:7px;border-radius:5px;border:1px solid #30363d;background:#21262d;color:#e6e6e6;cursor:pointer;font-weight:600;font-size:13px}
button:hover{border-color:#F47B20}
button.primary{background:#F47B20;border-color:#F47B20;color:#000}
.pw-wrap{position:relative;display:flex;align-items:center}
.pw-wrap input{flex:1;padding-right:30px}
button.pw-eye{position:absolute;right:4px;flex:none;background:none;border:none;cursor:pointer;color:#8b949e;font-size:14px;padding:2px 4px;line-height:1;font-weight:normal}
button.pw-eye:hover,button.pw-eye.on{color:#F47B20}
.full{padding:0 12px 12px}
.note{color:#8b949e;font-size:12px;line-height:1.5;margin-bottom:10px}
.diag{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:12px 14px;font-size:12px}
.diag h2{font-size:13px;color:#F47B20;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center}
.diag h2 button{flex:0;white-space:nowrap;padding:4px 10px;font-size:11px}
.diag table{border-collapse:collapse;width:100%}
.diag td{padding:2px 6px 2px 0;font-family:ui-monospace,Consolas,monospace}
.diag td:first-child{color:#8b949e;width:120px}
.ok{color:#3fb950}.bad{color:#f85149}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#21262d;border:1px solid #30363d;padding:9px 16px;border-radius:6px;opacity:0;transition:opacity .2s;pointer-events:none;white-space:nowrap}
#toast.show{opacity:1}
#toast.err{border-color:#f85149;color:#f85149}
</style>
</head>
<body>
<div class="hdr">
<a href="/index.html" title="Back">&#8592;</a>
<img src="/logo.png" alt="MyPi">
<h1>AP Profiles</h1>
</div>
<div class="wavesep"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>

<div class="grid2" id="cards"></div>

<div class="full">
  <p class="note">Starting an AP from here restarts the Wi-Fi access point. If connected through the AP, reconnect to the new SSID afterwards. NAT shares the ESP&#8217;s Wi-Fi (STA) internet with AP clients.</p>
  <div class="diag" id="diag">
    <h2><span>NAT / uplink status</span><button onclick="loadStatus(true)">Test uplink</button></h2>
    <table id="diagTbl"><tr><td>Loading&#8230;</td></tr></table>
  </div>
</div>

<div id="toast"></div>
<script>
var SUBNETS=["192.168.4.x","192.168.42.x","10.11.0.x","172.16.4.x"];
var MASK="********";

function toast(msg,err){
  var t=document.getElementById('toast');
  t.textContent=msg;t.className='show'+(err?' err':'');
  clearTimeout(t._h);t._h=setTimeout(function(){t.className='';},3000);
}
function esc(s){
  return String(s).replace(/[&<>"']/g,function(c){return({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]);});
}

// Reveal: first click fetches real password; subsequent clicks toggle visibility.
function togglePw(slot,btn){
  var inp=document.getElementById('pass'+slot);
  if(inp.dataset.haspass==='1'){
    // First reveal: fetch actual password from server
    fetch('/api/approfile/getpass?slot='+slot)
      .then(function(r){return r.json();})
      .then(function(d){
        inp.value=d.pass||'';
        inp.type='text';
        inp.dataset.haspass='0';
        btn.classList.add('on');
      })
      .catch(function(){toast('Could not retrieve password',true);});
    return;
  }
  if(inp.type==='password'){inp.type='text';btn.classList.add('on');}
  else{inp.type='password';btn.classList.remove('on');}
}

function cardHtml(p){
  var opts=SUBNETS.map(function(l,i){
    return '<option value="'+i+'"'+(i===p.subnet?' selected':'')+'>'+l+'</option>';
  }).join('');
  var hasPass=p.valid&&p.pass===MASK;
  return '<div class="card'+(p.active?' active':'')+'" id="card'+p.slot+'">'
    +'<h2><span>Profile '+(p.slot+1)+'</span><span class="badge">ACTIVE</span></h2>'
    +'<label>Name</label><input type="text" id="name'+p.slot+'" maxlength="23" value="'+esc(p.name)+'">'
    +'<label>SSID</label><input type="text" id="ssid'+p.slot+'" maxlength="32" value="'+esc(p.ssid)+'">'
    +'<label>Password (WPA2, 8-63 chars)</label>'
    +'<div class="pw-wrap">'
    +'<input type="password" id="pass'+p.slot+'" maxlength="63"'
    +' value="'+(hasPass?MASK:'')+'"'
    +' data-haspass="'+(hasPass?'1':'0')+'"'
    +' placeholder="'+(hasPass?'':'no password set')+'" autocomplete="new-password">'
    +'<button type="button" class="pw-eye" onclick="togglePw('+p.slot+',this)" title="Show/hide password">&#x1F441;</button>'
    +'</div>'
    +'<label>Subnet</label><select id="sub'+p.slot+'">'+opts+'</select>'
    +'<label class="chk"><input type="checkbox" id="nat'+p.slot+'"'+(p.nat?' checked':'')+'>Share internet (NAT)</label>'
    +'<label class="chk"><input type="checkbox" id="boot'+p.slot+'"'+(p.onBoot?' checked':'')+' onchange="bootChanged('+p.slot+',this)">Start on boot</label>'
    +'<label class="chk"><input type="checkbox" id="hide'+p.slot+'"'+(p.hideOnBoot?' checked':'')+'>Hide on boot</label>'
    +'<div class="btns">'
    +'<button onclick="saveProfile('+p.slot+')">Save</button>'
    +'<button class="primary" onclick="startAp('+p.slot+')">Start AP</button>'
    +'</div></div>';
}

function bootChanged(slot,cb){
  if(!cb.checked)return;
  document.querySelectorAll('input[id^="boot"]').forEach(function(el){if(el!==cb)el.checked=false;});
}

async function load(){
  try{
    var r=await fetch('/api/approfiles');
    var list=await r.json();
    document.getElementById('cards').innerHTML=list.map(cardHtml).join('');
  }catch(e){toast('Could not load AP profiles',true);}
}

async function post(url,data){
  var r=await fetch(url,{method:'POST',body:new URLSearchParams(data)});
  var txt=await r.text();
  if(!r.ok)throw new Error(txt);
  return txt;
}

async function saveProfile(slot){
  var g=function(id){return document.getElementById(id+slot);};
  var pw=g('pass').value;
  var data={slot:slot,name:g('name').value.trim(),ssid:g('ssid').value.trim(),
            subnet:g('sub').value,nat:g('nat').checked?'1':'0',
            onBoot:g('boot').checked?'1':'0',hideOnBoot:g('hide').checked?'1':'0'};
  if(!data.ssid){toast('SSID must not be empty',true);return;}
  // Send pass only if user entered a new one (not the mask, not empty)
  if(pw&&pw!==MASK){
    if(pw.length<8||pw.length>63){toast('Password must be 8-63 characters',true);return;}
    data.pass=pw;
  }
  try{toast(await post('/api/approfile/save',data));await load();}
  catch(e){toast(e.message||'Save failed',true);}
}

async function startAp(slot){
  try{toast(await post('/api/apstart',{slot:slot}));setTimeout(load,2500);}
  catch(e){toast(e.message||'Start failed',true);}
}

function cls(ok){return ok?'ok':'bad';}
async function loadStatus(test){
  try{
    var r=await fetch('/api/apstatus'+(test?'?test=1':''));
    var d=await r.json();
    var rows=[
      ['AP',d.apActive?'profile '+d.profile+' \u2014 '+d.apIp+' \u2014 '+d.clients+(d.maxClients?'/'+d.maxClients:'')+' client(s)'+(d.maxClients&&d.clients>=d.maxClients?' (full)':''):'not running'],
      ['NAT','<span class="'+cls(d.natActive||!d.natWanted)+'">'+esc(d.natStatus)+'</span>'],
      ['Client DNS',d.clientDns],
      ['Uplink (STA)',d.staConnected?'<span class="ok">'+esc(d.staSsid)+'</span> '+d.staIp+' ('+d.staRssi+' dBm)':'<span class="bad">not connected</span>'],
      ['Uplink gateway',d.staGw],
      ['Uplink DNS',d.staDns],
      ['Radio','ch '+d.channel+' \u00b7 AP '+(d.apProto?d.apProto+' ':'')+d.apBw+' MHz \u00b7 STA '+d.staBw+' MHz \u00b7 TX '+d.txPowerDbm+' dBm \u00b7 <span class="'+cls(!d.powerSave)+'">power save '+(d.powerSave?'ON':'off')+'</span>'],
      ['Uplink drops',d.staDrops?'<span class="bad">'+d.staDrops+'</span> \u2014 last '+esc(d.staLastReason)+', '+d.staLastDropAgoS+' s ago':'<span class="ok">0</span>'],
      ['ESP load','loop max '+d.loopMaxMs+' ms \u00b7 heap '+d.heapFreeKb+' kB (min '+d.heapMinKb+' kB) \u00b7 up '+Math.floor(d.uptimeS/60)+' min'],
    ];
    if(d.apActive&&d.clientList&&d.clientList.length>0){
      var cl=d.clientList.map(function(c,i){
        return['&nbsp;&nbsp;Client '+(i+1),(c.dot>=0?'.'+c.dot:'?.?')+'&nbsp;&nbsp;'+esc(c.label)
          +(c.vendor?' <span style="color:#8b949e">('+esc(c.vendor)+')</span>':'')
          +' <span style="color:#8b949e" title="'+(c.hostSrc?'name via '+esc(c.hostSrc):'no host name yet')+'">'+esc(c.mac)+'</span>'];
      });
      rows.splice(1,0,...cl);
    }
    if(d.dnsTest!==undefined)
      rows.push(['ESP DNS test','<span class="'+cls(d.dnsTest!=='FAIL')+'">google.com \u2192 '+esc(d.dnsTest)+'</span>']);
    else if(test)
      rows.push(['ESP DNS test','<span class="bad">skipped \u2014 no uplink</span>']);
    document.getElementById('diagTbl').innerHTML=
      rows.map(function(r){return'<tr><td>'+r[0]+'</td><td>'+r[1]+'</td></tr>';}).join('');
  }catch(e){
    document.getElementById('diagTbl').innerHTML='<tr><td class="bad">status unavailable</td></tr>';
  }
}

load();
loadStatus(false);
setInterval(function(){loadStatus(false);},5000);
</script>
</body>
</html>
)html";
