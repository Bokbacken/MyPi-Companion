// Profile Manager page (/profiles). Edited by hand (generator not in repo).
#pragma once
static const char PROFILES_HTML[] PROGMEM = R"PHTML(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MyPi Companion — Profiles</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#e6e6e6;max-width:520px;margin:0 auto}
.hdr{background:#1a3a6b;padding:12px 16px;border-bottom:3px solid #F47B20;display:flex;align-items:center;gap:12px}
.hdr a{color:#F47B20;text-decoration:none;font-size:20px;line-height:1}
.hdr h1{font-size:18px;flex:1}
.card{background:#161b22;margin:10px;border-radius:8px;padding:14px;border:1px solid #30363d}
h2{font-size:14px;color:#c9d1d9;margin-bottom:10px;display:flex;align-items:center;gap:8px}
.badge{background:#1f6feb;color:#fff;font-size:10px;padding:2px 6px;border-radius:4px;font-weight:600}
.slot{display:grid;grid-template-columns:18px 1fr auto;gap:8px;align-items:center;padding:8px 0;border-bottom:1px solid #21262d}
.slot:last-child{border:none}
.slot-num{font-size:11px;color:#8b949e;font-weight:700}
.slot-info{min-width:0}
.slot-ssid{font-size:14px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.slot-sub{font-size:10px;color:#8b949e;margin-top:2px}
.slot-empty{color:#484f58;font-style:italic;font-size:13px}
.slot-btns{display:flex;gap:4px;flex-shrink:0}
.btn{padding:5px 10px;border:none;border-radius:5px;cursor:pointer;font-size:12px;font-weight:600;white-space:nowrap}
.bc{background:#F47B20;color:#fff}.ba{background:#238636;color:#fff}
.bd{background:#6e2020;color:#fff}.bn{background:#2d333b;color:#e6e6e6}
.bc:hover{background:#e06010}.bd:hover{background:#8a2424}.bn:hover{background:#373e47}
.pi-row{display:flex;justify-content:space-between;align-items:center;padding:7px 0;border-bottom:1px solid #21262d;gap:8px}
.pi-row:last-child{border:none}
.pi-name{font-size:13px;flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pi-load{display:flex;gap:4px;align-items:center;flex-shrink:0}
select{background:#0d1117;color:#e6e6e6;border:1px solid #30363d;border-radius:5px;padding:4px 6px;font-size:12px}
label{font-size:12px;color:#8b949e;display:block;margin-bottom:3px;margin-top:8px}
label:first-child{margin-top:0}
input[type=text],input[type=password]{width:100%;padding:8px;border-radius:6px;border:1px solid #30363d;background:#0d1117;color:#e6e6e6;font-size:13px;display:block}
.iprow{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.tog{display:flex;gap:8px;margin:4px 0 8px}
.tog-btn{padding:6px 16px;border:1px solid #30363d;border-radius:6px;cursor:pointer;font-size:12px;background:#0d1117;color:#8b949e}
.tog-btn.active{background:#1f6feb;color:#fff;border-color:#1f6feb}
.static-fields{display:none;margin-top:6px}
.msg{margin:0 10px 6px;padding:10px;border-radius:6px;font-size:12px;display:none}
.ok{background:#0f3520;color:#3fb950}.er{background:#3a0f0f;color:#f85149}
.rfbtn{margin-left:auto;padding:3px 8px;font-size:11px}
footer{text-align:center;padding:14px;font-size:11px;color:#484f58}
.est{font-size:13px;line-height:1.6}.est b{color:#e6e6e6}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:#484f58}
.dot.on{background:#3fb950}.dot.wait{background:#F47B20}.dot.bad{background:#f85149}
.note{font-size:11px;color:#8b949e;margin-top:6px}
.scan-row{display:flex;justify-content:space-between;padding:6px 4px;border-bottom:1px solid #21262d;cursor:pointer;font-size:13px}
.scan-row:hover{background:#1c2330}.scan-row span:last-child{color:#8b949e;font-size:11px}
#scan-list{max-height:220px;overflow-y:auto;margin-top:6px}
</style></head><body>
<div class="hdr"><a href="/">&#8592;</a><h1>Profile Manager</h1></div>

<div class="card">
<h2>ESP32 Wi-Fi Connection <span class="badge">ESP</span></h2>
<div class="est" id="esp-st"><span class="dot"></span>Loading...</div>
<div class="note" id="esp-note"></div>
<div style="margin-top:8px;display:flex;gap:6px">
<button class="btn bn" onclick="espScan()" id="scan-btn">&#128269; Scan</button>
<button class="btn bd" onclick="espDisconnect()" id="disc-btn" style="display:none">Disconnect ESP</button>
</div>
<div id="scan-list"></div>
<label>SSID</label>
<input type="text" id="e-ssid" placeholder="Network name (tap a scan result)" autocomplete="off">
<label>Password</label>
<input type="password" id="e-pass" placeholder="Leave blank for open network" autocomplete="off">
<button class="btn ba" style="width:100%;padding:10px;margin-top:10px" onclick="espConnectNew()">Connect ESP to this SSID</button>
<div class="note">Saved to an ESP NVS slot (same SSID reused, else first free slot), then the ESP joins it.</div>
</div>

<div class="card">
<h2>ESP32 Saved Profiles <span class="badge">NVS &bull; 10 slots</span></h2>
<div id="nvs-slots"><em style="color:#484f58;font-size:13px">Loading...</em></div>
</div>

<div class="card">
<h2>Pi Network Profiles <button class="btn bn rfbtn" onclick="loadPiProfiles()">&#8635; Fetch from Pi</button></h2>
<div id="pi-list"><em style="color:#484f58;font-size:13px">Press Fetch to load from Pi</em></div>
</div>

<div class="card" id="form-card">
<h2>Add / Edit Profile</h2>
<label>Target slot</label>
<select id="f-slot">
  <option value="0">Slot 1</option><option value="1">Slot 2</option>
  <option value="2">Slot 3</option><option value="3">Slot 4</option>
  <option value="4">Slot 5</option><option value="5">Slot 6</option>
  <option value="6">Slot 7</option><option value="7">Slot 8</option>
  <option value="8">Slot 9</option><option value="9">Slot 10</option>
</select>
<label>SSID</label>
<input type="text" id="f-ssid" placeholder="Network name" autocomplete="off">
<label>Password</label>
<div style="display:flex;gap:6px;align-items:center">
<input type="password" id="f-pass" placeholder="Leave blank for open network" autocomplete="off" style="flex:1">
<button class="btn bn" onclick="togglePass()" id="eye-btn" style="padding:6px 10px;flex-shrink:0" title="Show/hide password">&#128065;</button>
</div>
<label>IP Assignment</label>
<div class="tog">
  <button class="tog-btn active" id="tog-dhcp" onclick="setDhcp(true)">DHCP</button>
  <button class="tog-btn" id="tog-static" onclick="setDhcp(false)">Static IP</button>
</div>
<div class="static-fields" id="static-fields">
<div class="iprow">
  <div><label>IP Address</label><input type="text" id="f-ip" placeholder="192.168.1.100"></div>
  <div><label>Subnet Mask</label><input type="text" id="f-sn" placeholder="255.255.255.0"></div>
</div>
<div class="iprow">
  <div><label>Gateway</label><input type="text" id="f-gw" placeholder="192.168.1.1"></div>
  <div><label>DNS</label><input type="text" id="f-dns" placeholder="8.8.8.8"></div>
</div>
</div>
<div style="margin-top:10px;display:flex;gap:8px">
<button class="btn bc" style="flex:1;padding:10px" onclick="saveSlot()">Save to Slot</button>
<button class="btn bn" style="padding:10px" onclick="clearForm()">Clear</button>
</div>
</div>

<div class="msg" id="msg"></div>
<footer>MyPi Companion &bull; Profile Manager &bull; 192.168.4.1</footer>

<script>
var isDhcp=true;
function setDhcp(v){
  isDhcp=v;
  document.getElementById('tog-dhcp').className='tog-btn'+(v?' active':'');
  document.getElementById('tog-static').className='tog-btn'+(!v?' active':'');
  document.getElementById('static-fields').style.display=v?'none':'block';
}
function showMsg(t,ok){
  var m=document.getElementById('msg');
  m.textContent=t;m.className='msg '+(ok?'ok':'er');m.style.display='block';
  setTimeout(function(){m.style.display='none';},4000);
}
function esc(s){
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
}
function togglePass(){
  var f=document.getElementById('f-pass');
  var b=document.getElementById('eye-btn');
  if(f.type==='password'){f.type='text';b.style.background='#1f6feb';b.style.color='#fff';}
  else{f.type='password';b.style.background='';b.style.color='';}
}
function renderNvs(slots){
  var h='';
  for(var i=0;i<10;i++){
    var s=(slots&&slots[i])?slots[i]:{valid:false};
    h+='<div class="slot">';
    h+='<span class="slot-num">'+(i+1)+'</span>';
    if(s.valid){
      h+='<div class="slot-info"><div class="slot-ssid">'+esc(s.ssid)+'</div>';
      h+='<div class="slot-sub">'+(s.dhcp?'DHCP':'Static: '+(s.ip||'?'))+(i===_espSlot&&_espConn?' \u2022 <span style="color:#3fb950">ESP connected</span>':'')+'</div></div>';
      h+='<div class="slot-btns">';
      h+='<button class="btn bc" onclick="editSlot('+i+')">Edit</button>';
      h+='<button class="btn ba" onclick="espConnectSlot('+i+')" title="Connect the ESP32 to this network">ESP</button>';
      h+='<button class="btn ba" onclick="connectSlot('+i+')" title="Ask the Pi to join this network">Pi</button>';
      h+='<button class="btn bd" onclick="delSlot('+i+')">Del</button>';
      h+='</div>';
    } else {
      h+='<span class="slot-empty">\u2014 empty \u2014</span>';
      h+='<div class="slot-btns"></div>';
    }
    h+='</div>';
  }
  document.getElementById('nvs-slots').innerHTML=h;
}
var _nvsCache=[];
function loadNvs(){
  fetch('/api/nvs').then(function(r){return r.json();}).then(function(d){
    _nvsCache=d;renderNvs(d);
  }).catch(function(){
    document.getElementById('nvs-slots').innerHTML='<em style="color:#f85149">Load failed</em>';
  });
}
function editSlot(i){
  var s=_nvsCache&&_nvsCache[i];if(!s||!s.valid)return;
  document.getElementById('f-slot').value=i;
  document.getElementById('f-ssid').value=s.ssid||'';
  document.getElementById('f-pass').value=s.pass||'';
  setDhcp(s.dhcp!==false);
  if(!s.dhcp){
    document.getElementById('f-ip').value=s.ip||'';
    document.getElementById('f-sn').value=s.sn||'255.255.255.0';
    document.getElementById('f-gw').value=s.gw||'';
    document.getElementById('f-dns').value=s.dns||'';
  }
  document.getElementById('form-card').scrollIntoView({behavior:'smooth'});
}
function clearForm(){
  document.getElementById('f-ssid').value='';
  document.getElementById('f-pass').value='';
  document.getElementById('f-ip').value='';
  document.getElementById('f-sn').value='';
  document.getElementById('f-gw').value='';
  document.getElementById('f-dns').value='';
  setDhcp(true);
}
function loadPiProfiles(){
  document.getElementById('pi-list').innerHTML='<em style="color:#484f58;font-size:13px">Requesting from Pi...</em>';
  fetch('/api/reqpiprofiles',{method:'POST'}).then(function(){
    setTimeout(fetchPiProfiles,900);
  }).catch(function(){showMsg('Could not reach ESP32',false);});
}
function fetchPiProfiles(){
  fetch('/api/piprofiles').then(function(r){return r.json();}).then(function(profs){
    if(!profs||profs.length===0){
      document.getElementById('pi-list').innerHTML='<em style="color:#484f58;font-size:13px">No profiles on Pi</em>';
      return;
    }
    var h='';
    profs.forEach(function(p){
      var sid=esc(p.ssid);
      h+='<div class="pi-row">';
      h+='<span class="pi-name">'+sid+'</span>';
      h+='<div class="pi-load">';
      h+='<select id="pisl-'+sid+'">';
      for(var s=0;s<10;s++){h+='<option value="'+s+'">Slot '+(s+1)+'</option>';}
      h+='</select>';
      h+='<button class="btn bc" onclick="loadPiToForm(\''+sid+'\',\''+esc(p.pass)+'\')">Load</button>';
      h+='</div></div>';
    });
    document.getElementById('pi-list').innerHTML=h;
  });
}
function loadPiToForm(ssid,pass){
  var slotEl=document.getElementById('pisl-'+ssid);
  if(slotEl)document.getElementById('f-slot').value=slotEl.value;
  document.getElementById('f-ssid').value=ssid;
  document.getElementById('f-pass').value=pass;
  setDhcp(true);
  showMsg('Profile loaded into form \u2014 review and press Save to Slot',true);
  document.getElementById('form-card').scrollIntoView({behavior:'smooth'});
}
function saveSlot(){
  var slot=parseInt(document.getElementById('f-slot').value);
  var ssid=document.getElementById('f-ssid').value.trim();
  var pass=document.getElementById('f-pass').value;
  var ip=document.getElementById('f-ip').value.trim();
  var sn=document.getElementById('f-sn').value.trim();
  var gw=document.getElementById('f-gw').value.trim();
  var dns=document.getElementById('f-dns').value.trim();
  if(!ssid){showMsg('SSID is required',false);return;}
  if(!isDhcp&&!ip){showMsg('IP Address required for Static mode',false);return;}
  var body='slot='+slot+'&ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass);
  body+='&dhcp='+(isDhcp?'1':'0');
  if(!isDhcp){
    body+='&ip='+encodeURIComponent(ip)+'&sn='+encodeURIComponent(sn||'255.255.255.0');
    body+='&gw='+encodeURIComponent(gw)+'&dns='+encodeURIComponent(dns);
  }
  fetch('/api/nvssave',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
  .then(function(r){return r.text();})
  .then(function(t){showMsg(t,!t.startsWith('ERR'));loadNvs();});
}
function delSlot(i){
  if(!confirm('Delete slot '+(i+1)+'?'))return;
  fetch('/api/nvsdel',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+i})
  .then(function(r){return r.text();})
  .then(function(t){showMsg(t,!t.startsWith('ERR'));loadNvs();});
}
function connectSlot(i){
  fetch('/api/nvsconnect',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+i})
  .then(function(r){return r.text();})
  .then(function(t){showMsg(t,!t.startsWith('ERR'));});
}
// ── ESP STA connection (the ESP itself, not the Pi) ──────────
var _espSlot=-1,_espConn=false,_espStTimer=null,_scanTimer=null;
function post(url,body){
  return fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''})
    .then(function(r){return r.text().then(function(t){if(!r.ok)throw new Error(t);return t;});});
}
function espStatus(){
  fetch('/api/espstatus').then(function(r){return r.json();}).then(function(d){
    var h,n='';
    if(d.connecting){h='<span class="dot wait"></span>Joining <b>'+esc(d.target)+'</b>...';}
    else if(d.connected){h='<span class="dot on"></span>Connected to <b>'+esc(d.ssid)+'</b> &bull; '+esc(d.ip)+' &bull; '+d.rssi+' dBm &bull; ch '+d.channel;}
    else if(d.result===2){h='<span class="dot bad"></span>Not connected &mdash; joining <b>'+esc(d.target)+'</b> failed';}
    else{h='<span class="dot"></span>Not connected';}
    if(d.ap)n='AP is running'+(d.via_ap?' (you are on the AP)':'')+'. When the ESP joins a network the AP moves to that channel; AP clients may drop for a few seconds.';
    else n='AP is off: if you reach this page via the ESP\'s Wi-Fi, connecting elsewhere will cut this page off. Use the new IP or rigconsole.local.';
    document.getElementById('esp-st').innerHTML=h;
    document.getElementById('esp-note').textContent=n;
    document.getElementById('disc-btn').style.display=(d.connected&&d.ap)?'':'none';
    var changed=(_espSlot!==d.slot||_espConn!==d.connected);
    _espSlot=d.slot;_espConn=d.connected;
    if(changed)renderNvs(_nvsCache);
    schedEsp(d.connecting?1500:4000);
  }).catch(function(){
    document.getElementById('esp-st').innerHTML='<span class="dot bad"></span>ESP not reachable (reconnecting...)';
    schedEsp(4000);
  });
}
function schedEsp(ms){clearTimeout(_espStTimer);_espStTimer=setTimeout(espStatus,ms);}
function espConnectSlot(i){
  var s=_nvsCache&&_nvsCache[i];if(!s||!s.valid)return;
  if(!confirm('Connect the ESP32 to "'+s.ssid+'"?'))return;
  post('/api/espconnect','slot='+i).then(function(){showMsg('ESP joining '+s.ssid+'...',true);schedEsp(800);})
    .catch(function(e){showMsg(e.message,false);});
}
function espConnectNew(){
  var ssid=document.getElementById('e-ssid').value.trim();
  var pass=document.getElementById('e-pass').value;
  if(!ssid){showMsg('SSID is required',false);return;}
  if(pass&&(pass.length<8||pass.length>63)){showMsg('Password must be 8-63 characters',false);return;}
  post('/api/espconnect','ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass))
    .then(function(t){var d=JSON.parse(t);showMsg('Saved to slot '+(d.slot+1)+', ESP joining '+ssid+'...',true);loadNvs();schedEsp(800);})
    .catch(function(e){showMsg(e.message,false);});
}
function espDisconnect(){
  if(!confirm('Disconnect the ESP32 from its Wi-Fi network? The AP stays up.'))return;
  post('/api/espdisconnect').then(function(){schedEsp(800);}).catch(function(e){showMsg(e.message,false);});
}
function espScan(){
  document.getElementById('scan-btn').disabled=true;
  document.getElementById('scan-list').innerHTML='<em style="color:#484f58;font-size:13px">Scanning...</em>';
  post('/api/espscan').then(function(){clearTimeout(_scanTimer);_scanTimer=setTimeout(pollScan,1500);})
    .catch(function(e){document.getElementById('scan-btn').disabled=false;showMsg(e.message,false);});
}
function pollScan(){
  fetch('/api/espscan').then(function(r){return r.json();}).then(function(d){
    if(d.running){_scanTimer=setTimeout(pollScan,1000);return;}
    document.getElementById('scan-btn').disabled=false;
    var l=d.list.sort(function(a,b){return b.rssi-a.rssi;});
    if(!l.length){document.getElementById('scan-list').innerHTML='<em style="color:#484f58;font-size:13px">No networks found</em>';return;}
    var h='';
    l.forEach(function(n,i){
      h+='<div class="scan-row" data-i="'+i+'"><span>'+esc(n.ssid)+(n.open?' (open)':' &#128274;')+'</span><span>'+n.rssi+' dBm &bull; ch '+n.ch+'</span></div>';
    });
    var el=document.getElementById('scan-list');el.innerHTML=h;
    Array.prototype.forEach.call(el.querySelectorAll('.scan-row'),function(r){
      r.onclick=function(){var n=l[+r.getAttribute('data-i')];
        document.getElementById('e-ssid').value=n.ssid;
        document.getElementById('e-pass').value='';
        document.getElementById('e-pass').focus();};
    });
  }).catch(function(){document.getElementById('scan-btn').disabled=false;});
}
loadNvs();
espStatus();
// NVS slots can also change from the touch screen: refresh periodically
setInterval(function(){if(!document.hidden)loadNvs();},10000);
</script></body></html>
)PHTML";
