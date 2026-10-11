// ============================================================
//  macros_html.h  —  Macro & Menu GUI Editor
//  Served at GET /macros
//
//  Data files on FFat (compatible with v4 terminal format):
//    /files/config/macros.json — { macros:[{id,name,cmd,cr}], nextId:N }
//    /files/config/menus.json  — { menus:[{id,label,entries:[...]}], nextId:N }
//        entry types:
//          { type:"macro",   ref:<macroId> }
//          { type:"sep" }
//          { type:"submenu", label:"...", entries:[...] }
//
//  QB slots: NVS via /api/quickbtns (GET) and /api/quickbtns/save (POST)
//  File save: POST /upload  (FormData, field "file")
//  File read: GET  /files/<name>
//
//  Edit this file directly (no generator script).
//  Rebuild + flash via Arduino IDE after changes.
// ============================================================
#pragma once

static const char MACROS_HTML[] PROGMEM = R"MACROHTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HMS Rig Console — Macro Editor</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0d1117;--surf:#161b22;--elev:#21262d;--border:#30363d;
  --accent:#e94560;--blue:#3b82f6;--green:#22c55e;
  --text:#e6edf3;--muted:#8b949e;--dim:#484f58;
  --mono:'Cascadia Code','Fira Mono',Consolas,monospace;
  --radius:8px;
}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
     background:var(--bg);color:var(--text);height:100vh;
     display:flex;flex-direction:column;overflow:hidden;font-size:13px}

/* ── Toolbar ── */
#tb{background:var(--surf);border-bottom:1px solid var(--border);
    padding:8px 14px;display:flex;align-items:center;gap:10px;flex-shrink:0;z-index:10}
#tb .logo{font-weight:700;font-size:14px;color:var(--accent);white-space:nowrap}
#tb a{color:var(--muted);text-decoration:none;font-size:12px;padding:4px 8px;
       border-radius:5px;transition:background .12s}
#tb a:hover{background:var(--elev);color:var(--text)}
.sp{flex:1}
#st{font-size:11px;color:var(--muted);min-width:140px;text-align:right}
.btn{padding:6px 13px;border:none;border-radius:6px;cursor:pointer;
     font-size:12px;font-weight:600;letter-spacing:.3px;transition:filter .12s,opacity .12s}
.btn:active{filter:brightness(.9)}
.btn-r{background:var(--accent);color:#fff}
.btn-b{background:var(--blue);color:#fff}
.btn-g{background:var(--elev);color:var(--text);border:1px solid var(--border)}
.btn-g:hover{background:#2d333b}
.btn:disabled{opacity:.4;cursor:default}

/* ── Layout ── */
#main{display:flex;flex:1;overflow:hidden}

/* ── Library panel ── */
#lib{width:260px;min-width:180px;background:var(--surf);
     border-right:1px solid var(--border);display:flex;
     flex-direction:column;flex-shrink:0}
#lh{padding:10px 10px 8px;border-bottom:1px solid var(--border)}
#lh h2{font-size:10px;text-transform:uppercase;letter-spacing:1px;
        color:var(--dim);margin-bottom:6px}
#srch{width:100%;padding:6px 9px;background:var(--elev);
      border:1px solid transparent;border-radius:6px;
      color:var(--text);font-size:12px;outline:none}
#srch:focus{border-color:var(--accent)}
#ml{flex:1;overflow-y:auto;padding:6px}
.mc{background:var(--elev);border:2px solid transparent;border-radius:var(--radius);
    padding:8px 10px;margin-bottom:5px;cursor:grab;user-select:none;
    position:relative;transition:border-color .12s,background .12s}
.mc:hover{background:#2d333b}
.mc.sel{border-color:var(--accent)}
.mc.dragging{opacity:.35}
.mc .mn{font-weight:600;font-size:12px;display:flex;align-items:center;gap:4px;
         padding-right:44px}
.mc .mk{font-family:var(--mono);font-size:10px;color:var(--muted);
        margin-top:3px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.mc .cr-badge{font-size:9px;background:#1f2d3d;color:#60a5fa;
              padding:1px 4px;border-radius:3px;flex-shrink:0}
.mcb{position:absolute;right:6px;top:8px;display:none;gap:2px}
.mc:hover .mcb{display:flex}
.ic{background:none;border:none;cursor:pointer;font-size:12px;color:var(--muted);
    padding:3px 5px;border-radius:4px;line-height:1;transition:background .1s,color .1s}
.ic:hover{background:rgba(255,255,255,.1);color:var(--text)}
.ic.del:hover{color:var(--accent)}
#nmb{margin:6px;padding:9px;background:none;border:2px dashed var(--border);
     border-radius:var(--radius);color:var(--dim);cursor:pointer;
     width:calc(100% - 12px);font-size:12px;transition:border-color .12s,color .12s}
#nmb:hover{border-color:var(--accent);color:var(--accent)}
.lib-empty{text-align:center;color:var(--dim);font-size:12px;padding:20px;font-style:italic}

/* ── Editor panel ── */
#ed{flex:1;display:flex;flex-direction:column;overflow:hidden}
.tabs{display:flex;background:var(--surf);border-bottom:1px solid var(--border);flex-shrink:0}
.tab{padding:10px 18px;cursor:pointer;border-bottom:2px solid transparent;
     color:var(--muted);font-size:13px;font-weight:500;transition:color .12s;white-space:nowrap}
.tab:hover{color:var(--text)}
.tab.on{color:var(--accent);border-bottom-color:var(--accent)}
.pane{flex:1;overflow-y:auto;padding:16px;display:none}
.pane.on{display:block}

/* ── Shared hint strip ── */
.hint-strip{background:var(--elev);border:1px solid var(--border);border-radius:var(--radius);
            padding:8px 12px;font-size:11px;color:var(--muted);margin-bottom:14px;line-height:1.6}
.hint-strip b{color:var(--text)}

/* ── Menu tree ── */
.tree-tb{display:flex;align-items:center;gap:8px;margin-bottom:12px}
.tree-tb .sp{flex:1}
.grp{background:var(--surf);border:1px solid var(--border);
     border-radius:10px;margin-bottom:10px;overflow:hidden}
.gh{display:flex;align-items:center;gap:6px;padding:9px 12px;
    background:var(--elev);border-radius:10px;user-select:none}
.grp:not(.col) .gh{border-radius:10px 10px 0 0}
.chev{color:var(--muted);font-size:10px;transition:transform .2s;flex-shrink:0;cursor:pointer}
.grp.col .chev{transform:rotate(-90deg)}
.grp.col .gb{display:none}
.gl{flex:1;background:none;border:none;color:var(--text);font-size:13px;
    font-weight:600;outline:none;min-width:60px;cursor:text}
.ga{display:flex;gap:3px;flex-shrink:0}
.gb{padding:8px}
/* drop zone */
.dz{border:2px dashed var(--border);border-radius:6px;padding:9px;
    text-align:center;color:var(--dim);font-size:11px;margin-top:4px;
    transition:border-color .15s,color .15s,background .15s}
.dz.ov{border-color:var(--accent);color:var(--accent);background:rgba(233,69,96,.06)}
/* rows inside group */
.row{display:flex;align-items:center;gap:6px;padding:6px 8px;
     background:var(--elev);border-radius:6px;margin-bottom:4px;font-size:12px}
.row.sep-r{background:none;padding:2px 8px}
.rh{cursor:grab;color:var(--dim);font-size:11px;flex-shrink:0;padding:0 2px}
.rn{flex:0 0 auto;font-weight:500;max-width:130px;
    overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.rc{flex:1;font-family:var(--mono);font-size:10px;color:var(--muted);
    overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
/* submenu block */
.sub{background:#0a0f16;border:1px solid #1f2937;border-radius:var(--radius);
     margin-bottom:4px;overflow:hidden}
.suh{display:flex;align-items:center;gap:6px;padding:7px 10px;background:#111827}
.sul{flex:1;background:none;border:none;color:#60a5fa;font-size:12px;
     font-weight:600;outline:none}
.sub .gb{padding:5px;min-height:24px}
.sdz{border:2px dashed #1f2937;border-radius:5px;padding:6px;
     text-align:center;color:#2d3748;font-size:11px;
     transition:border-color .15s,color .15s,background .15s}
.sdz.ov{border-color:var(--accent);color:var(--accent);background:rgba(233,69,96,.08)}
.tree-empty{text-align:center;color:var(--dim);font-size:12px;
            padding:32px;font-style:italic}

/* ── QB panel ── */
.qb-hint{background:var(--elev);border:1px solid var(--border);border-radius:var(--radius);
         padding:8px 12px;font-size:11px;color:var(--muted);margin-bottom:12px;line-height:1.6}
.ql{display:flex;flex-direction:column;gap:4px;max-width:420px}
.qs{display:flex;align-items:center;gap:10px;padding:8px 12px;
    background:var(--surf);border:1px solid var(--border);
    border-radius:var(--radius);cursor:pointer;transition:border-color .12s,background .12s}
.qs:hover{background:var(--elev);border-color:var(--blue)}
.qs.fil{border-color:var(--blue)}
.qs.ov{border-color:var(--accent);background:var(--elev)}
.qn{width:24px;height:24px;background:var(--elev);border-radius:50%;
    display:flex;align-items:center;justify-content:center;
    font-size:10px;font-weight:700;color:var(--muted);flex-shrink:0}
.qs.fil .qn{background:var(--blue);color:#fff}
.qlb{flex:1;color:var(--muted);font-style:italic;font-size:12px}
.qs.fil .qlb{color:var(--text);font-style:normal}
.qsub{font-family:var(--mono);font-size:10px;color:var(--dim);
      overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:120px}
.qx{background:none;border:none;color:var(--dim);cursor:pointer;
    font-size:14px;padding:0 4px;border-radius:3px;transition:color .1s}
.qx:hover{color:var(--accent)}

/* ── Modal ── */
#ov{position:fixed;inset:0;background:rgba(0,0,0,.75);
    display:none;align-items:center;justify-content:center;z-index:200}
#ov.op{display:flex}
#mo{background:var(--surf);border:1px solid var(--border);
    border-radius:14px;padding:22px;width:440px;max-width:96vw;
    box-shadow:0 8px 32px rgba(0,0,0,.5)}
#mo h3{font-size:14px;font-weight:700;color:var(--accent);margin-bottom:16px}
.fr{margin-bottom:12px}
.fr label{display:block;font-size:11px;color:var(--muted);margin-bottom:4px;font-weight:500}
.fr input,.fr textarea{width:100%;padding:8px 10px;background:var(--elev);
  border:1px solid var(--border);border-radius:6px;color:var(--text);
  font-size:13px;outline:none;transition:border-color .12s}
.fr input:focus,.fr textarea:focus{border-color:var(--accent)}
.fr textarea{font-family:var(--mono);height:76px;resize:vertical}
.fr .hint{font-size:10px;color:var(--dim);margin-top:4px}
.fr .chk{display:flex;align-items:center;gap:8px;font-size:12px;color:var(--muted);
         margin-top:6px}
.fr .chk input{width:14px;height:14px;accent-color:var(--accent)}
.ma{display:flex;gap:8px;justify-content:flex-end;margin-top:16px}
.btn-d{background:#dc2626;color:#fff}
.btn-d:hover{filter:brightness(1.1)}

/* ── Toast ── */
#toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);
       background:#0d2137;border:1px solid var(--blue);border-radius:8px;
       padding:8px 20px;color:#60a5fa;font-size:12px;z-index:500;
       display:none;white-space:nowrap}

::-webkit-scrollbar{width:5px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:#30363d;border-radius:3px}
</style>
</head>
<body>

<!-- Toolbar -->
<div id="tb">
  <span class="logo">⚡ HMS Rig — Macro Editor</span>
  <a href="/terminal">← Terminal</a>
  <span class="sp"></span>
  <span id="st">Laddar...</span>
  <button class="btn btn-g" onclick="doExport()" title="Download macros + menus as JSON">⬇ Export</button>
  <label class="btn btn-g" title="Import macros + menus from JSON" style="cursor:pointer">
    ⬆ Import<input type="file" accept=".json" style="display:none" onchange="doImport(event)">
  </label>
  <button class="btn btn-r" id="savebtn" onclick="saveAll()">💾 Spara alla</button>
</div>

<div id="main">

  <!-- Library -->
  <div id="lib">
    <div id="lh">
      <h2>Makropool (<span id="pool-cnt">0</span>)</h2>
      <input id="srch" type="text" placeholder="Sök makro…" oninput="rl()">
    </div>
    <div id="ml"></div>
    <button id="nmb" onclick="openModal()">＋ Nytt makro</button>
  </div>

  <!-- Editor -->
  <div id="ed">
    <div class="tabs">
      <div class="tab on"  onclick="sw('m')" id="tab-m">🗂 Menyer</div>
      <div class="tab"     onclick="sw('q')" id="tab-q">⚡ QB-panel (25)</div>
    </div>

    <!-- Menus pane -->
    <div id="pane-m" class="pane on">
      <div class="hint-strip">
        Markera ett makro i poolen → klicka <b>＋ Makro</b> på en grupp,
        eller <b>dra det direkt</b> till droppzonen.<br>
        Klicka på ett gruppnamn för att byta namn direkt.
        <b>Ctrl+S</b> sparar allt.
      </div>
      <div class="tree-tb">
        <span class="sp"></span>
        <button class="btn btn-g" style="font-size:12px" onclick="addGrp()">＋ Ny menygrupp</button>
      </div>
      <div id="tree"></div>
      <div class="tree-empty" id="te">Inga menygrupper ännu — klicka "＋ Ny menygrupp".</div>
    </div>

    <!-- QB pane -->
    <div id="pane-q" class="pane">
      <div class="qb-hint">
        Markera ett makro i poolen → klicka på ett QB-slot för att tilldela det.<br>
        Du kan också <b>dra ett makro direkt</b> till ett slot.<br>
        QB-slots sparas till NVS via <code>/api/quickbtns/save</code> direkt vid tilldelning.
      </div>
      <div class="ql" id="qbl"></div>
    </div>
  </div>
</div>

<!-- Macro edit modal -->
<div id="ov" onclick="ovClick(event)">
  <div id="mo">
    <h3 id="mt">Nytt makro</h3>
    <div class="fr">
      <label>Namn</label>
      <input id="mn" type="text" maxlength="40" placeholder="t.ex. IP info">
    </div>
    <div class="fr">
      <label>Kommando</label>
      <textarea id="mc" maxlength="200"
        placeholder="ip addr show&#10;En rad = ett kommando.&#10;Använd {variabel} för platshållare som frågas vid körning."></textarea>
      <div class="hint">Tips: <code>{host}</code>, <code>{port}</code>, <code>{ip}</code> — popup visas i terminalen</div>
    </div>
    <div class="fr">
      <label class="chk">
        <input type="checkbox" id="mcr"> Lägg till CR (Enter) automatiskt
      </label>
    </div>
    <div class="ma">
      <button class="btn btn-d" id="mdb" style="display:none;margin-right:auto" onclick="delMacro()">🗑 Radera</button>
      <button class="btn btn-g" onclick="closeModal()">Avbryt</button>
      <button class="btn btn-r" onclick="saveMo()">Spara</button>
    </div>
  </div>
</div>

<div id="toast"></div>

<script>
// ================================================================
//  State  (v4-compatible schema)
//  macros.json: { macros:[{id,name,cmd,cr}], nextId:N }
//  menus.json:  { menus:[{id,label,entries:[...]}], nextId:N }
//    entry: {type:'macro',ref:id} | {type:'sep'} | {type:'submenu',label,entries:[...]}
// ================================================================
var MP  = { macros:[], nextId:1 };   // macro pool
var MC  = { menus:[], nextId:1 };    // menu config
var QB  = new Array(25).fill(-1);    // QB slots

var selId   = null;   // selected macro id in library
var dragId  = null;   // macro id being dragged
var editId  = null;   // modal: null=new, else id
var dirty   = false;  // unsaved changes flag

// ================================================================
//  Init
// ================================================================
window.addEventListener('load', async function() {
  st('Laddar…');
  // Load all data in parallel, then render once — avoids the race where
  // rt() runs before MP is populated and macro refs resolve to '?'.
  await Promise.all([loadMacros(), loadMenus(), loadQB()]);
  rl(); rt(); rqb();
  st('Redo');
});

async function loadMacros() {
  try {
    var r = await fetch('/files/config/macros.json');
    if (r.ok) MP = await r.json();
  } catch(e) {}
}

async function loadMenus() {
  try {
    var r = await fetch('/files/config/menus.json');
    if (r.ok) MC = await r.json();
  } catch(e) {}
}

async function loadQB() {
  try {
    var r = await fetch('/api/quickbtns');
    if (r.ok) {
      var arr = await r.json();
      arr.forEach(function(x){ if(x.slot>=0&&x.slot<25) QB[x.slot]=x.macroid; });
    }
  } catch(e) {}
}

// ================================================================
//  Save
// ================================================================
async function saveAll() {
  st('Sparar…');
  document.getElementById('savebtn').disabled = true;
  var ok = await uploadJson('config/macros.json', MP)
        && await uploadJson('config/menus.json',  MC);
  document.getElementById('savebtn').disabled = false;
  dirty = false;
  st(ok ? '✓ Sparat ' + new Date().toLocaleTimeString('sv') : '✗ Fel vid sparning!', !ok);
  if (ok) toast('Sparat ✓', 2000);
}

async function uploadJson(name, data) {
  try {
    var blob = new Blob([JSON.stringify(data)], {type:'application/json'});
    var fd = new FormData(); fd.append('file', blob, name);
    var r = await fetch('/upload', {method:'POST', body:fd});
    return r.ok;
  } catch(e) { return false; }
}

async function saveQBSlot(slot, mid) {
  try {
    var fd = new FormData(); fd.append('slot', slot); fd.append('macroid', mid);
    await fetch('/api/quickbtns/save', {method:'POST', body:fd});
  } catch(e) {}
}

function markDirty() { dirty = true; }

// ================================================================
//  Export / Import
// ================================================================
function doExport() {
  var d = { macros_json:MP, menus_json:MC };
  var b = new Blob([JSON.stringify(d,null,2)], {type:'application/json'});
  var a = document.createElement('a');
  a.href = URL.createObjectURL(b); a.download = 'rig_macros_export.json'; a.click();
}

function fixNextId(obj, arr) {
  var max = 0;
  arr.forEach(function(x){ if (x.id > max) max = x.id; });
  if (!(obj.nextId > max)) obj.nextId = max + 1;
  return obj;
}

function doImport(e) {
  var file = e.target.files[0]; if (!file) return;
  var fr = new FileReader();
  fr.onload = function(ev) {
    try {
      var d = JSON.parse(ev.target.result);
      // Accepts the combined export ({macros_json, menus_json}) as well as
      // a plain macros.json ({macros:[...]}) or menus.json ({menus:[...]}).
      var mp = d.macros_json || (Array.isArray(d.macros) ? d : null);
      var mc = d.menus_json  || (Array.isArray(d.menus)  ? d : null);
      if (!mp && !mc) { toast('No macros or menus found in file!'); return; }
      if (mp) MP = fixNextId(mp, mp.macros);
      if (mc) MC = fixNextId(mc, mc.menus);
      rl(); rt(); rqb(); markDirty();
      toast((mp && mc ? 'Macros + menus' : mp ? mp.macros.length + ' macros' : mc.menus.length + ' menus') +
            ' imported — click Save to apply', 4000);
    } catch(err) { toast('Ogiltig JSON-fil!'); }
  };
  fr.readAsText(file); e.target.value = '';
}

// ================================================================
//  Helpers
// ================================================================
function st(msg, err) {
  var el = document.getElementById('st');
  el.textContent = msg; el.style.color = err ? '#f87171' : '#8b949e';
}
function esc(s) {
  return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;')
                      .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function gm(id)  { return MP.macros.find(function(m){return m.id===id;}); }
function toast(msg, ms) {
  var t = document.getElementById('toast');
  t.textContent = msg; t.style.display = 'block';
  clearTimeout(t._t);
  t._t = setTimeout(function(){t.style.display='none';}, ms||2500);
}
function uid() { return MC.nextId++; }

// ================================================================
//  Tab switch
// ================================================================
function sw(t) {
  ['m','q'].forEach(function(k){
    document.getElementById('tab-'+k).classList.toggle('on', k===t);
    document.getElementById('pane-'+k).classList.toggle('on', k===t);
  });
}

// ================================================================
//  Library render
// ================================================================
function selMacro(id) { selId = (selId===id) ? null : id; rl(); }

function rl() {
  var q = document.getElementById('srch').value.toLowerCase();
  var c = document.getElementById('ml'); c.innerHTML = '';
  document.getElementById('pool-cnt').textContent = MP.macros.length;
  var fl = MP.macros.filter(function(m){
    return !q || m.name.toLowerCase().includes(q) || (m.cmd||'').toLowerCase().includes(q);
  });
  if (!fl.length) {
    c.innerHTML = '<div class="lib-empty">' + (q?'Inga träffar':'Inga makron ännu') + '</div>';
    return;
  }
  fl.forEach(function(m) {
    var d = document.createElement('div');
    d.className = 'mc' + (m.id===selId?' sel':'');
    d.draggable = true; d.dataset.id = m.id;
    d.innerHTML =
      '<div class="mn">' + esc(m.name) +
        (m.cr ? '<span class="cr-badge">CR</span>' : '') +
        '<div class="mcb">' +
          '<button class="ic" title="Redigera" onclick="openModal('+m.id+');event.stopPropagation()">✏️</button>' +
          '<button class="ic del" title="Radera" onclick="delById('+m.id+');event.stopPropagation()">🗑</button>' +
        '</div>' +
      '</div>' +
      '<div class="mk">' + esc(m.cmd||'') + '</div>';
    d.addEventListener('click', function(){ selMacro(m.id); });
    d.addEventListener('dragstart', function(e){
      dragId = m.id; d.classList.add('dragging');
      e.dataTransfer.effectAllowed = 'copy';
    });
    d.addEventListener('dragend', function(){
      dragId = null; d.classList.remove('dragging');
    });
    c.appendChild(d);
  });
}

// ================================================================
//  Menu tree
// ================================================================
function addGrp() {
  var lbl = prompt('Namn på menygrupp:','Ny grupp'); if (!lbl) return;
  MC.menus.push({id:uid(), label:lbl.trim(), entries:[]}); rt(); markDirty();
}

function remGrp(idx) {
  if (!confirm('Radera hela gruppen?')) return;
  MC.menus.splice(idx,1); rt(); markDirty();
}

function addSub(mi) {
  var lbl = prompt('Namn på undermeny:','Ny undermeny'); if (!lbl) return;
  MC.menus[mi].entries.push({type:'submenu', label:lbl.trim(), entries:[]}); rt(); markDirty();
}

function addSep(mi, si) {
  getEntries(mi,si).push({type:'sep'}); rt(); markDirty();
}

function addSelTo(mi, si) {
  if (selId===null) { toast('Markera ett makro i poolen först'); return; }
  addMacroTo(mi, si, selId);
}

function addMacroTo(mi, si, mid) {
  getEntries(mi,si).push({type:'macro', ref:mid}); rt(); markDirty();
}

function remEntry(mi, si, ei) {
  getEntries(mi,si).splice(ei,1); rt(); markDirty();
}

function mvEntry(mi, si, from, to) {
  var arr = getEntries(mi,si);
  if (from<0||to<0||from>=arr.length||to>=arr.length) return;
  arr.splice(to,0,arr.splice(from,1)[0]); rt(); markDirty();
}

function renGrp(mi, v) {
  if (v.trim()) MC.menus[mi].label = v.trim(); markDirty();
}

function renSub(mi, ei, v) {
  var e = MC.menus[mi].entries[ei];
  if (e && v.trim()) e.label = v.trim(); markDirty();
}

function getEntries(mi, si) {
  var grp = MC.menus[mi];
  if (si===null||si===undefined) return grp.entries;
  return grp.entries[si].entries;
}

// ---- render tree ----
function rt() {
  var tree = document.getElementById('tree');
  var emp  = document.getElementById('te');
  tree.innerHTML = '';
  if (!MC.menus.length) { emp.style.display=''; return; }
  emp.style.display = 'none';
  MC.menus.forEach(function(grp, mi){ tree.appendChild(mkGrp(grp,mi)); });
}

function mkGrp(grp, mi) {
  var d = document.createElement('div'); d.className = 'grp';
  // header
  var h = document.createElement('div'); h.className = 'gh';
  var chev = document.createElement('span'); chev.className = 'chev'; chev.textContent = '▼';
  chev.onclick = function(){ d.classList.toggle('col'); };
  var inp = document.createElement('input'); inp.className = 'gl';
  inp.value = grp.label; inp.title = 'Klicka för att byta namn';
  inp.addEventListener('click', function(e){e.stopPropagation();});
  inp.addEventListener('blur', function(){renGrp(mi,this.value);});
  var ga = document.createElement('div'); ga.className = 'ga';
  ga.innerHTML =
    '<button class="ic" title="Lägg till markerat makro" onclick="addSelTo('+mi+',null)">＋ Makro</button>' +
    '<button class="ic" title="Ny undermeny" onclick="addSub('+mi+')">↳ Sub</button>' +
    '<button class="ic" title="Lägg till separator" onclick="addSep('+mi+',null)">— Sep</button>' +
    '<button class="ic del" title="Radera grupp" onclick="remGrp('+mi+')">🗑</button>';
  h.appendChild(chev); h.appendChild(inp); h.appendChild(ga); d.appendChild(h);
  // body
  var b = document.createElement('div'); b.className = 'gb';
  grp.entries.forEach(function(entry, ei){
    b.appendChild(mkEntry(entry, mi, null, ei, grp.entries.length));
  });
  b.appendChild(mkDZ('Dra ett makro hit', mi, null, 'dz'));
  d.appendChild(b); return d;
}

function mkEntry(entry, mi, si, ei, tot) {
  if (entry.type==='sep') {
    var r = document.createElement('div'); r.className = 'row sep-r';
    r.innerHTML = '<span style="flex:1;border-top:1px dashed var(--border);margin:0 6px"></span>' +
      '<button class="ic del" onclick="remEntry('+mi+','+si+','+ei+')">✕</button>';
    return r;
  }
  if (entry.type==='macro') {
    var m = gm(entry.ref);
    var r = document.createElement('div'); r.className = 'row';
    r.innerHTML =
      '<span class="rh">⠿</span>' +
      '<span class="rn" title="'+(m?esc(m.cmd):'')+'">'+esc(m?m.name:'?')+'</span>' +
      '<span class="rc">'+esc(m?m.cmd:'')+'</span>' +
      (ei>0?'<button class="ic" onclick="mvEntry('+mi+','+si+','+(ei)+','+(ei-1)+')">↑</button>':'') +
      (ei<tot-1?'<button class="ic" onclick="mvEntry('+mi+','+si+','+(ei)+','+(ei+1)+')">↓</button>':'') +
      '<button class="ic del" onclick="remEntry('+mi+','+si+','+ei+')">✕</button>';
    return r;
  }
  if (entry.type==='submenu') {
    var sb = document.createElement('div'); sb.className = 'sub';
    var suh = document.createElement('div'); suh.className = 'suh';
    var sulInp = document.createElement('input'); sulInp.className = 'sul';
    sulInp.value = entry.label;
    sulInp.addEventListener('click', function(e){e.stopPropagation();});
    sulInp.addEventListener('blur', function(){renSub(mi,ei,this.value);});
    suh.innerHTML =
      '<span style="color:#60a5fa;font-size:10px">▶</span>';
    suh.appendChild(sulInp);
    suh.innerHTML +=
      '<button class="ic" title="Lägg till markerat makro" onclick="addSelTo('+mi+','+ei+');event.stopPropagation()">＋</button>' +
      '<button class="ic" title="Separator" onclick="addSep('+mi+','+ei+');event.stopPropagation()">—</button>' +
      '<button class="ic del" onclick="remEntry('+mi+',null,'+ei+');event.stopPropagation()">🗑</button>';
    sb.appendChild(suh);
    // fix: re-append sulInp after innerHTML wipe
    suh.insertBefore(sulInp, suh.children[1]);
    var sbb = document.createElement('div'); sbb.className = 'gb';
    entry.entries.forEach(function(se,sei){
      sbb.appendChild(mkEntry(se, mi, ei, sei, entry.entries.length));
    });
    sbb.appendChild(mkDZ('Dra hit',mi,ei,'sdz'));
    sb.appendChild(sbb); return sb;
  }
  return document.createElement('div');
}

function mkDZ(lbl, mi, si, cls) {
  var dz = document.createElement('div'); dz.className = cls;
  dz.textContent = lbl;
  dz.addEventListener('dragover', function(e){ e.preventDefault(); dz.classList.add('ov'); });
  dz.addEventListener('dragleave', function(){ dz.classList.remove('ov'); });
  dz.addEventListener('drop', function(e){
    e.preventDefault(); dz.classList.remove('ov');
    if (dragId!==null) addMacroTo(mi, si, dragId);
  });
  return dz;
}

// ================================================================
//  QB
// ================================================================
function rqb() {
  var list = document.getElementById('qbl'); list.innerHTML = '';
  for (var i=0; i<25; i++) {
    var mid = QB[i]; var m = mid!==-1 ? gm(mid) : null;
    var s = document.createElement('div');
    s.className = 'qs' + (m?' fil':'');
    s.dataset.slot = i;
    s.innerHTML =
      '<div class="qn">'+(i+1)+'</div>' +
      '<div class="qlb">'+(m?esc(m.name):'— Tomt slot —')+'</div>' +
      (m?'<div class="qsub">'+esc(m.cmd||'')+'</div>':'') +
      (m?'<button class="qx" title="Rensa slot" onclick="clrQB('+i+');event.stopPropagation()">✕</button>':'');
    (function(slot){
      s.addEventListener('click', function(){ asQB(slot); });
      s.addEventListener('dragover', function(e){ e.preventDefault(); s.classList.add('ov'); });
      s.addEventListener('dragleave', function(){ s.classList.remove('ov'); });
      s.addEventListener('drop', function(e){
        e.preventDefault(); s.classList.remove('ov');
        if (dragId!==null) doQB(slot,dragId);
      });
    })(i);
    list.appendChild(s);
  }
}

function asQB(slot) {
  if (selId!==null) doQB(slot,selId);
  else toast('Markera ett makro i poolen först');
}

function doQB(slot, mid) {
  QB[slot] = mid;
  saveQBSlot(slot, mid);
  var m = gm(mid);
  toast('QB '+(slot+1)+' → '+(m?m.name:'?'), 1800);
  rqb();
}

function clrQB(slot) { QB[slot]=-1; saveQBSlot(slot,-1); rqb(); }

// ================================================================
//  Modal
// ================================================================
function openModal(id) {
  editId = id || null;
  var m = id ? gm(id) : null;
  document.getElementById('mt').textContent = m ? 'Redigera makro' : 'Nytt makro';
  document.getElementById('mn').value  = m ? m.name : '';
  document.getElementById('mc').value  = m ? (m.cmd||'')  : '';
  document.getElementById('mcr').checked = !!(m && m.cr);
  document.getElementById('mdb').style.display = m ? '' : 'none';
  document.getElementById('ov').classList.add('op');
  setTimeout(function(){ document.getElementById('mn').focus(); }, 50);
}

function closeModal() { document.getElementById('ov').classList.remove('op'); }

function ovClick(e) { if (e.target===document.getElementById('ov')) closeModal(); }

function saveMo() {
  var name = document.getElementById('mn').value.trim();
  var cmd  = document.getElementById('mc').value.trim();
  var cr   = document.getElementById('mcr').checked;
  if (!name) { toast('Namn krävs'); return; }
  if (editId!==null) {
    var m = gm(editId); if (m){ m.name=name; m.cmd=cmd; m.cr=cr; }
  } else {
    MP.macros.push({id:MP.nextId++, name:name, cmd:cmd, cr:cr});
  }
  markDirty(); rl(); rqb(); closeModal();
}

function delMacro() { if (editId!==null){ delById(editId); closeModal(); } }

function delById(id) {
  if (!confirm('Radera makrot? Det tas bort från alla menyer och QB-slots.')) return;
  MP.macros = MP.macros.filter(function(m){return m.id!==id;});
  // remove from menus
  MC.menus.forEach(function(g){
    g.entries = g.entries.filter(function(e){return !(e.type==='macro'&&e.ref===id);});
    g.entries.forEach(function(e){
      if(e.type==='submenu')
        e.entries=e.entries.filter(function(se){return !(se.type==='macro'&&se.ref===id);});
    });
  });
  // remove from QB
  for(var i=0;i<25;i++){ if(QB[i]===id){ QB[i]=-1; saveQBSlot(i,-1); } }
  if(selId===id) selId=null;
  markDirty(); rl(); rt(); rqb();
}

// ================================================================
//  Keyboard
// ================================================================
document.addEventListener('keydown', function(e){
  if (e.key==='Escape') closeModal();
  if ((e.ctrlKey||e.metaKey) && e.key==='s'){ e.preventDefault(); saveAll(); }
});

window.addEventListener('beforeunload', function(e){
  if (dirty){ e.preventDefault(); e.returnValue=''; }
});
</script>
</body>
</html>
)MACROHTML";
