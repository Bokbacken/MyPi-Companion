// ============================================================
//  ap_profiles_html.h  —  AP profile editor page
//  Served at GET /ap-profiles
//
//  API:
//    GET  /api/approfiles     — [{slot,name,ssid,pass:"********",subnet,subnetLabel,nat,valid,active}]
//    POST /api/approfile/save — slot, name, ssid, [pass], subnet, nat
//    POST /api/apstart        — slot
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
<title>HMS Rig Console — AP Profiles</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#e6e6e6;font-family:-apple-system,Segoe UI,Roboto,sans-serif;font-size:14px}
header{background:#1a3a6b;padding:12px 16px;display:flex;align-items:center;gap:12px;border-bottom:3px solid #F47B20}
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
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:#21262d;border:1px solid #30363d;padding:10px 16px;border-radius:6px;opacity:0;transition:opacity .2s;pointer-events:none}
#toast.show{opacity:1}
#toast.err{border-color:#f85149;color:#f85149}
</style>
</head>
<body>
<header><a href="/" title="Back">&#8592;</a><h1>HMS Rig Console — AP Profiles</h1></header>
<main id="cards">
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
    <input type="password" id="pass${p.slot}" maxlength="63" value=""
      placeholder="${p.pass === PLACEHOLDER ? 'unchanged' : ''}" autocomplete="new-password">
    <label>Subnet</label><select id="sub${p.slot}">${opts}</select>
    <label class="chk"><input type="checkbox" id="nat${p.slot}"${p.nat ? ' checked' : ''}> Share internet (NAT)</label>
    <div class="btns">
      <button onclick="saveProfile(${p.slot})">Save</button>
      <button class="primary" onclick="startAp(${p.slot})">Start AP</button>
    </div>
  </div>`;
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
                 subnet: g('sub').value, nat: g('nat').checked ? '1' : '0' };
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

load();
</script>
</body>
</html>
)html";
