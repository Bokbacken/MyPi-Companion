// ============================================================
//  ap_profiles_html.h  —  AP profile editor page
//  Served at GET /ap-profiles
//
//  API:
//    GET  /api/approfiles     — [{slot,name,ssid,pass:"********",subnet,subnetLabel,nat,valid,active}]
//    POST /api/approfile/save — slot, name, ssid, [pass], subnet, nat
//    POST /api/apstart        — slot
//    GET  /api/apstatus[?test=1] — AP/NAT/uplink diagnostics
//
//  Edit this file directly (no generator script).
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
body{background:#0d1117;color:#e6e6e6;font-family:-apple-system,Segoe UI,Roboto,sans-serif;font-size:14px}
header{background:#1a3a6b;padding:12px 16px;display:flex;align-items:center;gap:12px}
header a{color:#fff;text-decoration:none;font-size:18px}
header h1{font-size:16px;font-weight:600}
main{max-width:900px;margin:0 auto;padding:16px;display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:14px}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:14px}
.card.active{border-color:#F47B20}
.card h2{font-size:14px;color:#F47B20;margin-bottom:10px;display:flex;justify-content:space-between}
.badge{font-size:10px;background:#F47B20;color:#000;border-radius:4px;padding:2px 6px;display:none}
.card.active .badge{display:inline-block}
label{display:block;font-size:11px;color:#8b949e;margin:8px 0 3px}
input[type=text],input[type=password],select{width:100%;background:#0d1117;color:#e6e6e6;border:1px solid #30363d;border-radius:5px;padding:7px}
input:focus,select:focus{outline:none;border-color:#F47B20}
.chk{display:flex;align-items:center;gap:8px;margin-top:10px;font-size:13px;color:#e6e6e6}
.btns{display:flex;gap:8px;margin-top:14px}
button{flex:1;padding:8px;border-radius:5px;border:1px solid #30363d;background:#21262d;color:#e6e6e6;cursor:pointer;font-weight:600}
button:hover{border-color:#F47B20}
button.primary{background:#F47B20;border-color:#F47B20;color:#000}
.note{grid-column:1/-1;color:#8b949e;font-size:12px;line-height:1.5}
.diag{grid-column:1/-1;background:#161b22;border:1px solid #30363d;border-radius:8px;padding:12px 14px;font-size:12px}
.diag h2{font-size:13px;color:#F47B20;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center}
.diag h2 button{flex:0;white-space:nowrap;padding:4px 10px;font-size:11px}
.diag table{border-collapse:collapse;width:100%}
.diag td{padding:2px 6px 2px 0;font-family:ui-monospace,Consolas,monospace}
.diag td:first-child{color:#8b949e;width:130px}
.ok{color:#3fb950}.bad{color:#f85149}
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:#21262d;border:1px solid #30363d;padding:10px 16px;border-radius:6px;opacity:0;transition:opacity .2s;pointer-events:none}
#toast.show{opacity:1}
#toast.err{border-color:#f85149;color:#f85149}
.pw-wrap{position:relative;display:flex;align-items:center}
.pw-wrap input{flex:1;padding-right:34px}
button.pw-eye{position:absolute;right:6px;flex:none;background:none;border:none;cursor:pointer;color:#8b949e;font-size:15px;padding:2px 4px;line-height:1;font-weight:normal}
button.pw-eye:hover{color:#e6e6e6}
</style>
</head>
<body>
<header><a href="/" title="Back">&#8592;</a><img src="/logo.png" alt="MyPi" style="height:30px;vertical-align:middle;margin:0 8px"><h1>AP Profiles</h1></header>
<div class="wavesep" style="line-height:0;display:block;margin:0;padding:0"><svg viewBox="0 0 900 20" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" style="display:block;width:100%;height:20px;margin:0"><path d="M0,15L10,15L10,3L20,3L30,3L30,15L40,15L50,15L60,15L70,15L70,3L80,3L80,15L90,15L90,3L100,3L100,15L110,15L110,3L120,3L130,3L140,3L150,3L150,15L160,15L160,3L170,3L180,3L180,15L190,15L190,3L200,3L200,15L210,15L210,3L220,3L220,15L230,15L230,3L240,3L250,3L250,15L260,15L260,3L270,3L280,3L280,15L290,15L290,3L300,3L300,15L310,15L320,15L330,15L340,15L350,15L350,3L360,3L370,3L380,3L380,15L390,15L390,3L400,3L400,15L410,15L410,3L420,3L420,15L430,15L440,15L450,15L460,15L460,3L470,3L480,3L480,15L490,15L490,3L500,3L500,15L510,15L520,15L520,3L530,3L540,3L550,3L550,15L560,15L560,3L570,3L580,3L580,15L590,15L590,3L600,3L600,15L610,15L610,3L620,3L620,15L630,15L640,15L640,3L650,3L650,15L660,15L660,3L670,3L680,3L680,15L690,15L690,3L700,3L700,15L710,15L710,3L720,3L730,3L740,3L750,3L750,15L760,15L760,3L770,3L780,3L780,15L790,15L790,3L800,3L800,15L810,15L820,15L820,3L830,3L840,3L850,3L850,15L860,15L860,3L870,3L880,3L880,15L890,15L890,3L900,3" stroke="#4CC870" stroke-width="1.5" fill="none"/></svg></div>
<main id="cards">
<div class="diag" id="diag"><h2><span>NAT / uplink status</span><button onclick="loadStatus(true)">Test uplink</button></h2><table id="diagTbl"><tr><td>Loading…</td></tr></table></div>
<p class="note">Starting an AP from here restarts the Wi-Fi access point. If you are connected through the AP, reconnect to the new SSID afterwards. NAT shares the ESP's Wi-Fi (STA) internet with AP clients and only works while the ESP is connected to a network.</p>
</main>
<div id="toast"></div>
<script>
const SUBNETS = ["192.168.4.x", "192.168.42.x", "10.11.0.x", "172.16.4.x"];
const PLACEHOLDER = "********";

function toast(msg, err) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.className = 'show' + (err ? ' err' : '');
  clearTimeout(t._h); t._h = setTimeout(() => t.className = '', 3000);
}

function togglePw(id, btn) {
  var inp = document.getElementById(id);
  if (inp.type === 'password') {
    inp.type = 'text';
    btn.style.color = '#F47B20';
  } else {
    inp.type = 'password';
    btn.style.color = '';
  }
}

function esc(s) {
  return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

function cardHtml(p) {
  const opts = SUBNETS.map((l, i) =>
    `<option value="${i}"${i === p.subnet ? ' selected' : ''}>${l}</option>`).join('');
  return `<div class="card${p.active ? ' active' : ''}" id="card${p.slot}">
    <h2><span>Profile ${p.slot + 1}</span><span class="badge">ACTIVE</span></h2>
    <label>Name</label><input type="text" id="name${p.slot}" maxlength="23" value="${esc(p.name)}">
    <label>SSID</label><input type="text" id="ssid${p.slot}" maxlength="32" value="${esc(p.ssid)}">
    <label>Password (WPA2, 8-63 chars)</label>
    <div class="pw-wrap">
      <input type="password" id="pass${p.slot}" maxlength="63" value=""
        placeholder="${p.pass === PLACEHOLDER ? 'unchanged' : ''}" autocomplete="new-password">
      <button type="button" class="pw-eye" onclick="togglePw('pass${p.slot}',this)" title="Show/hide">&#x1F441;</button>
    </div>
    <label>Subnet</label><select id="sub${p.slot}">${opts}</select>
    <label class="chk"><input type="checkbox" id="nat${p.slot}"${p.nat ? ' checked' : ''}> Share internet (NAT)</label>
    <label class="chk"><input type="checkbox" id="boot${p.slot}"${p.onBoot ? ' checked' : ''} onchange="bootChanged(${p.slot},this)"> Start on boot</label>
    <label class="chk"><input type="checkbox" id="hide${p.slot}"${p.hideOnBoot ? ' checked' : ''}> Hide on boot</label>
    <div class="btns">
      <button onclick="saveProfile(${p.slot})">Save</button>
      <button class="primary" onclick="startAp(${p.slot})">Start AP</button>
    </div>
  </div>`;
}

// Only one profile can start on boot: uncheck the others in the UI
// (the firmware also clears them when this profile is saved).
function bootChanged(slot, cb) {
  if (!cb.checked) return;
  document.querySelectorAll('input[id^="boot"]').forEach(el => {
    if (el !== cb) el.checked = false;
  });
}

async function load() {
  try {
    const r = await fetch('/api/approfiles');
    const list = await r.json();
    const main = document.getElementById('cards');
    main.querySelectorAll('.card').forEach(c => c.remove());
    main.insertAdjacentHTML('afterbegin', list.map(cardHtml).join(''));
  } catch (e) { toast('Could not load AP profiles', true); }
}

async function post(url, data) {
  const r = await fetch(url, { method: 'POST', body: new URLSearchParams(data) });
  const txt = await r.text();
  if (!r.ok) throw new Error(txt);
  return txt;
}

async function saveProfile(slot) {
  const g = id => document.getElementById(id + slot);
  const data = { slot, name: g('name').value.trim(), ssid: g('ssid').value.trim(),
                 subnet: g('sub').value, nat: g('nat').checked ? '1' : '0',
                 onBoot: g('boot').checked ? '1' : '0',
                 hideOnBoot: g('hide').checked ? '1' : '0' };
  const pw = g('pass').value;
  if (!data.ssid) { toast('SSID must not be empty', true); return; }
  if (pw && pw !== 'unchanged') {
    if (pw.length < 8 || pw.length > 63) { toast('Password must be 8-63 characters', true); return; }
    data.pass = pw;
  }
  try { toast(await post('/api/approfile/save', data)); g('pass').value = ''; load(); }
  catch (e) { toast(e.message || 'Save failed', true); }
}

async function startAp(slot) {
  try { toast(await post('/api/apstart', { slot })); setTimeout(load, 2500); }
  catch (e) { toast(e.message || 'Start failed', true); }
}

function cls(ok) { return ok ? 'ok' : 'bad'; }
async function loadStatus(test) {
  try {
    const r = await fetch('/api/apstatus' + (test ? '?test=1' : ''));
    const d = await r.json();
    const rows = [
      ['AP', d.apActive ? `profile ${d.profile} — ${d.apIp} — ${d.clients} client(s)` : 'not running'],
      ['NAT', `<span class="${cls(d.natActive || !d.natWanted)}">${esc(d.natStatus)}</span>`],
      ['Client DNS', d.clientDns],
      ['Uplink (STA)', d.staConnected ? `<span class="ok">${esc(d.staSsid)}</span> ${d.staIp} (${d.staRssi} dBm)` : '<span class="bad">not connected</span>'],
      ['Uplink gateway', d.staGw],
      ['Uplink DNS', d.staDns],
      ['Radio', `ch ${d.channel} · AP ${d.apBw} MHz · STA ${d.staBw} MHz · TX ${d.txPowerDbm} dBm · ` +
                `<span class="${cls(!d.powerSave)}">power save ${d.powerSave ? 'ON' : 'off'}</span>`],
      ['Uplink drops', d.staDrops
        ? `<span class="bad">${d.staDrops}</span> — last ${esc(d.staLastReason)}, ${d.staLastDropAgoS} s ago`
        : '<span class="ok">0</span>'],
      ['ESP load', `loop max ${d.loopMaxMs} ms · heap ${d.heapFreeKb} kB (min ${d.heapMinKb} kB) · up ${Math.floor(d.uptimeS / 60)} min`],
    ];
    // Connected AP clients, right after the AP row
    if (d.apActive && d.clientList && d.clientList.length > 0) {
      const cl = d.clientList.map((c, i) =>
        [`&nbsp;&nbsp;Client ${i + 1}`, `${c.dot >= 0 ? '.' + c.dot : '?.?'}&nbsp;&nbsp;${esc(c.label)} <span style="color:#8b949e">${esc(c.mac)}</span>`]);
      rows.splice(1, 0, ...cl);
    }
    if (d.dnsTest !== undefined)
      rows.push(['ESP DNS test', `<span class="${cls(d.dnsTest !== 'FAIL')}">google.com → ${esc(d.dnsTest)}</span>`]);
    else if (test)
      rows.push(['ESP DNS test', '<span class="bad">skipped — no uplink</span>']);
    document.getElementById('diagTbl').innerHTML =
      rows.map(([k, v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join('');
  } catch (e) { document.getElementById('diagTbl').innerHTML = '<tr><td class="bad">status unavailable</td></tr>'; }
}

load();
loadStatus(false);
setInterval(() => loadStatus(false), 5000);
</script>
</body>
</html>
)html";
