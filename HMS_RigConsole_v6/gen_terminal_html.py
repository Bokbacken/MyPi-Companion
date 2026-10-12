#!/usr/bin/env python3
"""
gen_terminal_html.py
Generate terminal_html.h for MyPi Companion.
NOTE: No FitAddon — sizing handled via termFit() DOM measurement.
"""
import os

OUTPUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "terminal_html.h")

def c_escape(line):
    line = line.replace('\\', '\\\\')
    line = line.replace('"',  '\\"')
    return line

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MyPi Companion</title>
<link rel="stylesheet" href="/xterm.css">
<script src="/xterm.js"></script>
<style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{width:100%;height:100%;background:#111;color:#ccc;font-family:sans-serif;overflow:hidden}
#layout{display:flex;flex-direction:column;height:100vh}
#menubar{display:flex;align-items:center;background:#1a1a1a;border-bottom:1px solid #333;
         padding:2px 4px;gap:2px;flex-shrink:0;flex-wrap:wrap;position:relative;z-index:100}
#main{display:flex;flex:1;min-height:0}
#qbpanel{width:160px;flex-shrink:0;background:#161616;border-right:1px solid #2a2a2a;
          display:flex;flex-direction:column;padding:6px 4px;gap:4px;overflow-y:auto}
#termwrap{flex:1;min-width:0;min-height:0;position:relative;background:#0d0d0d}
#term{position:absolute;inset:0}
.mnu-btn{background:none;border:1px solid transparent;border-radius:3px;color:#bbb;
          padding:3px 8px;cursor:pointer;font-size:13px;white-space:nowrap}
.mnu-btn:hover,.mnu-btn.open{background:#2a2a2a;border-color:#444;color:#fff}
.dropdown{position:absolute;top:100%;left:0;background:#222;border:1px solid #444;
           border-radius:4px;min-width:180px;z-index:200;display:none}
.dropdown.show{display:block}
.dd-item{display:flex;align-items:center;padding:6px 12px;cursor:pointer;font-size:13px;
          color:#ccc;position:relative;white-space:nowrap}
.dd-item:hover{background:#333;color:#fff}
.dd-item.has-sub::after{content:"\25b6";margin-left:auto;padding-left:12px;font-size:10px;opacity:.6}
.dd-sep{height:1px;background:#333;margin:3px 6px}
.sub-menu{position:absolute;left:100%;top:-1px;background:#222;border:1px solid #444;
           border-radius:4px;min-width:180px;z-index:201;display:none}
.dd-item:hover>.sub-menu{display:block}
#qb-label{font-size:10px;color:#555;text-align:center;text-transform:uppercase;letter-spacing:.5px}
#qbgrid{display:flex;flex-direction:column;gap:3px;flex:1;overflow-y:auto}
.qb{background:#1e1e1e;border:1px solid #2e2e2e;border-radius:4px;color:#aaa;
     font-size:11px;padding:4px 6px;cursor:pointer;text-align:left;
     overflow:hidden;text-overflow:ellipsis;white-space:nowrap;
     width:100%;flex-shrink:0}
.qb:hover{background:#2a2a2a;color:#fff}
.qb.empty{color:#333;border-color:#222;text-align:center}
#modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.7);z-index:500;
           align-items:center;justify-content:center}
#modal-bg.show{display:flex}
#modal{background:#1c1c1c;border:1px solid #444;border-radius:8px;width:760px;max-width:96vw;
        max-height:90vh;display:flex;flex-direction:column}
#modal-hdr{display:flex;align-items:center;padding:10px 14px;border-bottom:1px solid #333;gap:8px}
#modal-hdr h2{flex:1;font-size:15px;font-weight:600;color:#eee}
#modal-close{background:none;border:none;color:#aaa;font-size:20px;cursor:pointer;line-height:1}
#modal-close:hover{color:#fff}
#modal-tabs{display:flex;border-bottom:1px solid #333;padding:0 14px}
.tab-btn{background:none;border:none;border-bottom:2px solid transparent;color:#888;
          padding:8px 14px;cursor:pointer;font-size:13px;margin-bottom:-1px}
.tab-btn.active{color:#4af;border-bottom-color:#4af}
#modal-body{flex:1;overflow:auto;padding:12px 14px}
.tab-pane{display:none}.tab-pane.active{display:block}
#macro-list{max-height:320px;overflow-y:auto;border:1px solid #2a2a2a;border-radius:4px}
.macro-row{display:flex;align-items:center;padding:5px 8px;border-bottom:1px solid #222;gap:8px;font-size:12px}
.macro-row:last-child{border-bottom:none}
.macro-row:hover{background:#232323}
.macro-name{flex:0 0 150px;color:#ddd;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.macro-cmd{flex:1;color:#888;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.macro-cr{flex:0 0 28px;text-align:center;color:#555}
.macro-cr.on{color:#4af}
.macro-actions{display:flex;gap:4px}
#menu-tree{max-height:300px;overflow-y:auto;border:1px solid #2a2a2a;border-radius:4px;padding:4px}
.mt-menu{margin-bottom:6px}
.mt-menu-hdr{display:flex;align-items:center;gap:6px;padding:4px 6px;background:#222;
              border-radius:4px;font-size:12px;font-weight:600;color:#ccc}
.mt-entry{display:flex;align-items:center;gap:6px;padding:3px 6px 3px 20px;font-size:12px;color:#bbb}
.mt-sub-hdr{display:flex;align-items:center;gap:6px;padding:3px 6px 3px 14px;font-size:12px;
             font-style:italic;color:#7af}
.mt-sub-entry{display:flex;align-items:center;gap:6px;padding:3px 6px 3px 32px;font-size:12px;color:#aaa}
.mt-del{background:#333;border:1px solid #444;border-radius:3px;color:#f66;
         font-size:11px;padding:1px 5px;cursor:pointer}
.mt-del:hover{background:#500}
#qb-assign-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:6px}
.qba{background:#1e1e1e;border:1px solid #2e2e2e;border-radius:5px;padding:6px 4px;
      text-align:center;font-size:11px;cursor:pointer;color:#aaa;min-height:48px;
      display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px}
.qba:hover{background:#2a2a2a}
.qba.assigned{border-color:#4af;color:#4af}
.qba .slot-no{font-size:9px;color:#555}
.qba .macro-nm{font-size:11px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:100%}
.form-row{display:flex;gap:8px;align-items:center;margin-bottom:8px;flex-wrap:wrap}
.form-row label{font-size:12px;color:#888;flex:0 0 80px}
.form-row input,.form-row select{flex:1;background:#111;border:1px solid #333;border-radius:4px;
  color:#ddd;padding:5px 8px;font-size:12px;min-width:0}
.form-row input:focus,.form-row select:focus{outline:none;border-color:#4af}
.btn{border:none;border-radius:4px;padding:5px 12px;cursor:pointer;font-size:12px}
.btn-pri{background:#246;color:#4af}.btn-pri:hover{background:#2a5070}
.btn-sm{padding:2px 7px;font-size:11px}
.btn-del{background:#400;color:#f66}.btn-del:hover{background:#600}
.btn-sec{background:#2a2a2a;color:#ccc;border:1px solid #333}.btn-sec:hover{background:#333}
.section-hdr{font-size:12px;color:#666;text-transform:uppercase;letter-spacing:.5px;margin:12px 0 6px}
.cr-check{display:flex;align-items:center;gap:6px;font-size:12px;color:#aaa}
.cr-check input{width:14px;height:14px}
#toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);
        background:#234;border:1px solid #4af;border-radius:6px;padding:7px 18px;
        color:#4af;font-size:13px;z-index:999;display:none}
</style>
</head>
<body>
<div id="layout">
<div id="menubar">
  <div class="mnu-wrap" style="position:relative">
    <button class="mnu-btn" onclick="toggleMenu(this)">&#9776; General</button>
    <div class="dropdown">
      <div class="dd-item" onclick="fatBrowse()">&#128194; Browse files&hellip;</div>
      <div class="dd-item" onclick="fatUpload()">&#8679; Upload file&hellip;</div>
      <div class="dd-item" onclick="saveSession()">&#128190; Save session&hellip;</div>
      <div class="dd-sep"></div>
      <div class="dd-item" onclick="fontInc()">A&#8314; Font +</div>
      <div class="dd-item" onclick="fontDec()">A&#8315; Font &minus;</div>
      <div class="dd-sep"></div>
      <div class="dd-item" onclick="window.open('/macros','_blank')">&#9998; Edit macros &amp; menus&hellip;</div>
      <div class="dd-sep"></div>
      <div class="dd-item" id="sd-share-item" onclick="sdShareToggle()">&#128190; SD Share: <span id="sd-share-st">&hellip;</span></div>
      <div class="dd-item" onclick="window.open('/storage','_blank')">&#128451; Storage &rarr;</div>
      <div class="dd-sep"></div>
      <div class="dd-item" onclick="doClear()">&#10006; Clear</div>
      <div class="dd-item" onclick="doReconnect()">&#8635; Reconnect</div>
    </div>
  </div>
  <div id="dyn-menus" style="display:contents"></div>
</div>
<div id="main">
  <div id="qbpanel">
    <div id="qb-label">Quick</div>
    <div id="qbgrid"></div>
  </div>
  <div id="termwrap"><div id="term"></div></div>
</div>
</div>

<div id="modal-bg">
  <div id="modal">
    <div id="modal-hdr">
      <h2>&#9998; Macros &amp; menus</h2>
      <button id="modal-close" onclick="closeEditor()">&times;</button>
    </div>
    <div id="modal-tabs">
      <button class="tab-btn active" onclick="showTab('macros',this)">Macros</button>
      <button class="tab-btn"        onclick="showTab('menus',this)">Menus</button>
      <button class="tab-btn"        onclick="showTab('qbtns',this)">Quick Buttons</button>
    </div>
    <div id="modal-body">
      <div id="tab-macros" class="tab-pane active">
        <div class="section-hdr">Macro pool (<span id="mp-count">0</span>/200)</div>
        <div id="macro-list"></div>
        <div style="margin-top:10px">
          <div class="section-hdr">Add / edit macro</div>
          <input type="hidden" id="ed-macro-id" value="">
          <div class="form-row"><label>Name</label><input id="ed-name" maxlength="40" placeholder="Macro name"></div>
          <div class="form-row"><label>Command</label><input id="ed-cmd" maxlength="200" placeholder="Command string"></div>
          <div class="form-row"><label></label><label class="cr-check"><input type="checkbox" id="ed-cr"> Append CR (\n)</label></div>
          <div class="form-row"><label></label>
            <button class="btn btn-pri" onclick="saveMacroEdit()">Save macro</button>
            <button class="btn btn-sec" onclick="clearMacroForm()" style="margin-left:6px">Clear</button>
          </div>
        </div>
        <div style="margin-top:10px;display:flex;gap:8px">
          <button class="btn btn-pri" onclick="saveMacroPool()">&#8679; Save macros.json</button>
          <button class="btn btn-sec" onclick="loadMacroPool()">&#8635; Reload</button>
        </div>
      </div>
      <div id="tab-menus" class="tab-pane">
        <div class="section-hdr">Menu structure (max 12 menus)</div>
        <div id="menu-tree"></div>
        <div style="margin-top:10px;display:flex;gap:8px;flex-wrap:wrap">
          <button class="btn btn-pri btn-sm" onclick="addMenu()">+ Menu</button>
          <button class="btn btn-sec btn-sm" onclick="addEntryPrompt()">+ Entry</button>
          <button class="btn btn-sec btn-sm" onclick="addSubmenuPrompt()">+ Submenu</button>
        </div>
        <div style="margin-top:10px;display:flex;gap:8px">
          <button class="btn btn-pri" onclick="saveMenuConf()">&#8679; Save menus.json</button>
          <button class="btn btn-sec" onclick="loadMenuConf()">&#8635; Reload</button>
        </div>
      </div>
      <div id="tab-qbtns" class="tab-pane">
        <div class="section-hdr">Assign macros to quick-button slots</div>
        <div id="qb-assign-grid"></div>
        <p style="margin-top:8px;font-size:11px;color:#555">Click a slot to assign a macro. Right-click to clear.</p>
        <div style="margin-top:10px">
          <button class="btn btn-pri" onclick="saveAllQbtns()">&#8679; Save to device</button>
        </div>
      </div>
    </div>
  </div>
</div>

<div id="toast"></div>

<script>
// ================================================================
//  State
// ================================================================
let term, ws;
let wsReconnectTimer = null;
let sessSlot = -1;          // session slot (0-3) assigned by ESP32 via SESS:<n>
let sessFull = false;       // true after ERR:SESSION_FULL -> no auto-reconnect
let macroPool  = { macros: [], nextId: 1 };
let menuConf   = { menus: [], nextId: 1 };
let qbSlots    = new Array(25).fill(-1);
let fontSize   = parseInt(localStorage.getItem('fontSize') || '14', 10);
let sessionLog = '';

// ================================================================
//  Terminal fit — no external addon needed
// ================================================================
function termFit() {
  if (!term) return;
  const wrap = document.getElementById('termwrap');
  const w = wrap.clientWidth;
  const h = wrap.clientHeight;
  if (!w || !h) return;
  const probe = document.createElement('span');
  probe.style.cssText = 'position:absolute;visibility:hidden;font-family:Menlo,Monaco,"Courier New",monospace;font-size:' + fontSize + 'px';
  probe.textContent = 'W';
  document.body.appendChild(probe);
  const rect = probe.getBoundingClientRect();
  const cw = rect.width  || fontSize * 0.6;
  const ch = rect.height || fontSize * 1.2;
  document.body.removeChild(probe);
  const cols = Math.max(10, Math.floor(w / cw));
  const rows = Math.max(4,  Math.floor(h / ch));
  term.resize(cols, rows);
}

// ================================================================
//  Clipboard helpers (copy works on both HTTP and HTTPS)
// ================================================================
function _clipCopy(text) {
  if (!text) return;
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).catch(() => _clipCopyExec(text));
  } else {
    _clipCopyExec(text);
  }
}
function _clipCopyExec(text) {
  const ta = document.createElement('textarea');
  ta.value = text;
  ta.style.cssText = 'position:fixed;opacity:0;pointer-events:none;top:0;left:0;width:1px;height:1px';
  document.body.appendChild(ta); ta.focus(); ta.select();
  try { document.execCommand('copy'); } catch (_) {}
  document.body.removeChild(ta);
  term.focus();
}

// ================================================================
//  Terminal init
// ================================================================
function initTerm() {
  term = new Terminal({
    cols: 80, rows: 24,
    fontSize,
    fontFamily: 'Menlo,Monaco,"Courier New",monospace',
    theme: { background: '#0d0d0d', foreground: '#cccccc', cursor: '#aaaaaa' },
    scrollback: 10000,
    cursorBlink: true
  });
  term.open(document.getElementById('term'));
  requestAnimationFrame(() => { termFit(); term.focus(); });
  term.attachCustomKeyEventHandler(ev => {
    if (ev.type !== 'keydown') return true;
    const ctrl = ev.ctrlKey && !ev.altKey;

    // Ctrl+Up / Ctrl+Down: browse local command history
    if (ctrl && !ev.shiftKey &&
        (ev.key === 'ArrowUp' || ev.key === 'ArrowDown')) {
      ev.preventDefault();
      histNav(ev.key === 'ArrowUp' ? -1 : 1);
      return false;
    }

    // Ctrl+C: copy selection if any, else send SIGINT (\x03)
    if (ctrl && !ev.shiftKey && ev.key === 'c') {
      const sel = term.getSelection();
      if (sel) { _clipCopy(sel); term.clearSelection(); return false; }
      return true;  // no selection → xterm sends \x03 (SIGINT)
    }

    // Ctrl+Shift+C: always copy selection (Linux/Windows terminal convention)
    if (ctrl && ev.shiftKey && ev.key === 'C') {
      const sel = term.getSelection(); if (sel) _clipCopy(sel);
      return false;  // never send to shell
    }

    // Ctrl+V / Ctrl+Shift+V: return false so xterm skips preventDefault,
    // which lets the browser fire the native paste event on xterm's hidden
    // textarea → xterm's own handlePasteEvent picks it up → onData → wsSendText
    if (ctrl && (ev.key === 'v' || (ev.shiftKey && ev.key === 'V'))) {
      return false;
    }

    return true;
  });
  term.onData(d => { histTrack(d); wsSendText(d); });
  term.onLineFeed(() => { sessionLog += '\n'; });
  new ResizeObserver(() => {
    termFit();
    if (ws && ws.readyState === 1)
      ws.send('CTL:RESIZE:' + term.cols + ':' + term.rows);
  }).observe(document.getElementById('termwrap'));
}

// ================================================================
//  Command history (Ctrl+Up / Ctrl+Down, last 100 commands)
// ================================================================
const HIST_MAX = 100;
let cmdHist = [];
try { cmdHist = JSON.parse(localStorage.getItem('cmdHist') || '[]'); } catch (_) { cmdHist = []; }
if (!Array.isArray(cmdHist)) cmdHist = [];
let histIdx  = cmdHist.length;   // == length -> editing a new line
let histDraft = '';
let lineBuf  = '';               // current input line as typed (null = unknown)

function wsSendText(d) {
  if (!ws || ws.readyState !== 1) return;
  try { ws.send(btoa(d)); }
  catch (_) {
    const b = new TextEncoder().encode(d);
    let s = ''; b.forEach(x => s += String.fromCharCode(x));
    ws.send(btoa(s));
  }
}

function histPush(cmd) {
  if (!cmd || !cmd.trim()) return;
  // Re-read the shared list so we never overwrite another tab's commands
  try {
    const fresh = JSON.parse(localStorage.getItem('cmdHist') || '[]');
    if (Array.isArray(fresh)) cmdHist = fresh;
  } catch (_) {}
  if (cmdHist[cmdHist.length - 1] !== cmd) cmdHist.push(cmd);
  if (cmdHist.length > HIST_MAX) cmdHist = cmdHist.slice(-HIST_MAX);
  try { localStorage.setItem('cmdHist', JSON.stringify(cmdHist)); } catch (_) {}
  histIdx = cmdHist.length;  // always point past end after a push
}

// Sync command history when another tab appends a command
window.addEventListener('storage', (e) => {
  if (e.key !== 'cmdHist') return;
  try {
    const updated = JSON.parse(e.newValue || '[]');
    if (Array.isArray(updated)) {
      cmdHist = updated;
      histIdx = cmdHist.length;  // reset navigation to end
    }
  } catch (_) {}
});

// Follow what the user types so Enter can record the command line.
function histTrack(d) {
  if (d.length > 1 && d.charCodeAt(0) === 27) { lineBuf = null; return; }  // escape seq (arrows etc.)
  for (const ch of d) {
    const c = ch.charCodeAt(0);
    if (ch === '\r' || ch === '\n') {
      if (lineBuf !== null) histPush(lineBuf);
      lineBuf = ''; histIdx = cmdHist.length; histDraft = '';
    } else if (c === 127 || c === 8) {
      if (lineBuf !== null) lineBuf = lineBuf.slice(0, -1);
    } else if (c === 3 || c === 21) {          // Ctrl+C / Ctrl+U
      lineBuf = ''; histIdx = cmdHist.length;
    } else if (c === 9 || c === 27 || c < 32) { // Tab completion / other control -> unknown
      lineBuf = null;
    } else if (lineBuf !== null) {
      lineBuf += ch;
    }
  }
}

function histNav(dir) {
  if (!cmdHist.length) return;
  if (histIdx === cmdHist.length && dir < 0) histDraft = lineBuf || '';
  const ni = Math.max(0, Math.min(cmdHist.length, histIdx + dir));
  if (ni === histIdx) return;
  histIdx = ni;
  const txt = histIdx === cmdHist.length ? histDraft : cmdHist[histIdx];
  wsSendText('\x15' + txt);  // Ctrl+U clears the shell's line, then type the entry
  lineBuf = txt;
}

// ================================================================
//  WebSocket
// ================================================================
function wsConnect() {
  const url = 'ws://' + location.hostname + ':81';
  ws = new WebSocket(url);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => {
    showToast('Connected', 2000);
    termFit();
    term.focus();
    ws.send('CTL:RESIZE:' + term.cols + ':' + term.rows);
  };
  ws.onmessage = e => {
    const raw = e.data instanceof ArrayBuffer
      ? new TextDecoder().decode(e.data) : String(e.data);
    if (raw.startsWith('SESS:')) {
      sessSlot = parseInt(raw.slice(5), 10);
      document.title = 'Terminal #' + (sessSlot + 1) + ' \u2014 MyPi Companion';
      return;
    }
    if (raw === 'ERR:SESSION_FULL') {
      sessFull = true;
      term.write('\r\n\x1b[31m[All 4 terminal sessions are in use. Close another tab and refresh.]\x1b[0m\r\n');
      ws.close();
      return;
    }
    let bytes;
    try { bytes = Uint8Array.from(atob(raw), c => c.charCodeAt(0)); }
    catch (_) { bytes = new TextEncoder().encode(raw); }
    term.write(bytes);
    sessionLog += new TextDecoder().decode(bytes);
  };
  ws.onclose = () => {
    sessSlot = -1;
    if (sessFull) { showToast('All terminal sessions in use', 4000); return; }
    showToast('Disconnected \u2014 reconnecting\u2026', 3000);
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = setTimeout(wsConnect, 2000);
  };
  ws.onerror = () => {};
}

// ================================================================
//  Macro pool
// ================================================================
async function loadMacroPool() {
  try {
    const r = await fetch('/files/config/macros.json');
    if (r.ok) macroPool = await r.json();
    else macroPool = { macros: [], nextId: 1 };
  } catch { macroPool = { macros: [], nextId: 1 }; }
  renderMacroList(); renderQbGrid(); renderQbAssignGrid();
}

async function saveMacroPool() {
  const blob = new Blob([JSON.stringify(macroPool)], {type:'application/json'});
  const fd = new FormData(); fd.append('file', blob, 'config/macros.json');
  const r = await fetch('/upload', {method:'POST', body:fd});
  showToast(r.ok ? 'macros.json saved' : 'Save failed', 2500);
}

function getMacroById(id) { return macroPool.macros.find(m => m.id === id); }

function runMacro(id) {
  const m = getMacroById(id);
  if (!m || !ws || ws.readyState !== 1) return;
  const payload = m.cmd + (m.cr ? '\n' : '');
  try { ws.send(btoa(payload)); }
  catch (_) {
    const b = new TextEncoder().encode(payload);
    let s = ''; b.forEach(x => s += String.fromCharCode(x));
    ws.send(btoa(s));
  }
}

function renderMacroList() {
  const el = document.getElementById('macro-list');
  const cnt = macroPool.macros.length;
  document.getElementById('mp-count').textContent = cnt;
  if (cnt === 0) { el.innerHTML = '<div style="padding:8px;color:#444;font-size:12px">No macros yet</div>'; return; }
  el.innerHTML = macroPool.macros.map(m => `
    <div class="macro-row">
      <span class="macro-name" title="${esc(m.name)}">${esc(m.name)}</span>
      <span class="macro-cmd"  title="${esc(m.cmd)}">${esc(m.cmd)}</span>
      <span class="macro-cr${m.cr?' on':''}">${m.cr?'\u23ce':'\u2014'}</span>
      <span class="macro-actions">
        <button class="btn btn-sm btn-sec" onclick="editMacro(${m.id})">Edit</button>
        <button class="btn btn-sm btn-del" onclick="deleteMacro(${m.id})">Del</button>
      </span>
    </div>`).join('');
}

function editMacro(id) {
  const m = getMacroById(id); if (!m) return;
  document.getElementById('ed-macro-id').value = id;
  document.getElementById('ed-name').value = m.name;
  document.getElementById('ed-cmd').value  = m.cmd;
  document.getElementById('ed-cr').checked = !!m.cr;
}

function clearMacroForm() {
  document.getElementById('ed-macro-id').value = '';
  document.getElementById('ed-name').value = '';
  document.getElementById('ed-cmd').value  = '';
  document.getElementById('ed-cr').checked = false;
}

function saveMacroEdit() {
  const name = document.getElementById('ed-name').value.trim();
  const cmd  = document.getElementById('ed-cmd').value;
  const cr   = document.getElementById('ed-cr').checked;
  if (!name) { alert('Name is required'); return; }
  const idStr = document.getElementById('ed-macro-id').value;
  if (idStr) {
    const m = getMacroById(parseInt(idStr, 10));
    if (m) { m.name = name; m.cmd = cmd; m.cr = cr; }
  } else {
    if (macroPool.macros.length >= 200) { alert('Macro pool full (200)'); return; }
    macroPool.macros.push({ id: macroPool.nextId++, name, cmd, cr });
  }
  clearMacroForm(); renderMacroList(); renderQbGrid(); renderQbAssignGrid();
}

function deleteMacro(id) {
  if (!confirm('Delete this macro?')) return;
  macroPool.macros = macroPool.macros.filter(m => m.id !== id);
  qbSlots = qbSlots.map(sid => sid === id ? -1 : sid);
  renderMacroList(); renderQbGrid(); renderQbAssignGrid();
}

// ================================================================
//  Menu configuration
// ================================================================
async function loadMenuConf() {
  try {
    const r = await fetch('/files/config/menus.json');
    if (r.ok) menuConf = await r.json();
    else menuConf = { menus: [], nextId: 1 };
  } catch { menuConf = { menus: [], nextId: 1 }; }
  buildMenuBar(); renderMenuTree();
}

async function saveMenuConf() {
  const blob = new Blob([JSON.stringify(menuConf)], {type:'application/json'});
  const fd = new FormData(); fd.append('file', blob, 'config/menus.json');
  const r = await fetch('/upload', {method:'POST', body:fd});
  showToast(r.ok ? 'menus.json saved' : 'Save failed', 2500);
  if (r.ok) buildMenuBar();
}

function buildMenuBar() {
  const cont = document.getElementById('dyn-menus');
  cont.innerHTML = '';
  if (!menuConf || !menuConf.menus) return;
  menuConf.menus.slice(0, 12).forEach(menu => {
    const wrap = document.createElement('div');
    wrap.className = 'mnu-wrap'; wrap.style.position = 'relative';
    const btn = document.createElement('button');
    btn.className = 'mnu-btn'; btn.textContent = menu.label;
    btn.onclick = () => toggleMenu(btn);
    const dd = document.createElement('div'); dd.className = 'dropdown';
    (menu.entries || []).slice(0, 20).forEach(e => dd.appendChild(buildDdEntry(e)));
    wrap.appendChild(btn); wrap.appendChild(dd); cont.appendChild(wrap);
  });
}

function buildDdEntry(e) {
  const div = document.createElement('div');
  if (e.type === 'sep') { div.className = 'dd-sep'; return div; }
  if (e.type === 'submenu') {
    div.className = 'dd-item has-sub'; div.textContent = e.label;
    const sub = document.createElement('div'); sub.className = 'sub-menu';
    (e.entries || []).slice(0, 20).forEach(se => sub.appendChild(buildDdEntry(se)));
    div.appendChild(sub); return div;
  }
  div.className = 'dd-item';
  const m = getMacroById(e.ref);
  div.textContent = m ? m.name : ('(id ' + e.ref + ')');
  div.onclick = () => { closeAllMenus(); runMacro(e.ref); };
  return div;
}

function renderMenuTree() {
  const el = document.getElementById('menu-tree');
  if (!menuConf.menus.length) {
    el.innerHTML = '<div style="padding:8px;color:#444;font-size:12px">No menus yet</div>'; return;
  }
  el.innerHTML = menuConf.menus.map((menu, mi) => {
    const entries = (menu.entries || []).map((e, ei) => renderEntryRow(e, mi, ei, null)).join('');
    return `<div class="mt-menu"><div class="mt-menu-hdr"><span>\u25bc ${esc(menu.label)}</span>
      <button class="mt-del btn btn-sm" onclick="delMenu(${mi})">Del menu</button></div>${entries}</div>`;
  }).join('');
}

function renderEntryRow(e, mi, ei, si) {
  if (e.type === 'sep') return `<div class="${si!=null?'mt-sub-entry':'mt-entry'}" style="color:#333">\u2015\u2015 separator \u2015\u2015
    <button class="mt-del btn btn-sm" onclick="delEntry(${mi},${ei},${si})">Del</button></div>`;
  if (e.type === 'submenu') {
    const subRows = (e.entries||[]).map((se,sei)=>renderEntryRow(se,mi,ei,sei)).join('');
    return `<div class="mt-sub-hdr"><span>\u25b6 ${esc(e.label)}</span>
      <button class="mt-del btn btn-sm" onclick="delEntry(${mi},${ei},null)">Del sub</button></div>${subRows}`;
  }
  const m = getMacroById(e.ref);
  const cls = si != null ? 'mt-sub-entry' : 'mt-entry';
  return `<div class="${cls}"><span>${m?esc(m.name):'(id '+e.ref+')'}</span>
    <button class="mt-del btn btn-sm" onclick="delEntry(${mi},${ei},${si})">Del</button></div>`;
}

function addMenu() {
  if (menuConf.menus.length >= 12) { alert('Max 12 menus'); return; }
  const label = prompt('Menu label:'); if (!label) return;
  menuConf.menus.push({ id: menuConf.nextId++, label, entries: [] }); renderMenuTree();
}

function delMenu(mi) {
  if (!confirm('Delete this menu?')) return;
  menuConf.menus.splice(mi, 1); renderMenuTree();
}

function addEntryPrompt() {
  if (!menuConf.menus.length) { alert('Add a menu first'); return; }
  const mLabel = prompt('Add entry to which menu? (enter menu label)');
  const menu = menuConf.menus.find(m => m.label === mLabel);
  if (!menu) { alert('Menu not found'); return; }
  if ((menu.entries||[]).length >= 20) { alert('Max 20 entries per menu'); return; }
  const mName = prompt('Macro name to add:');
  const m = macroPool.macros.find(m => m.name === mName);
  if (!m) { alert('Macro not found in pool'); return; }
  menu.entries = menu.entries || [];
  menu.entries.push({ type: 'macro', ref: m.id }); renderMenuTree();
}

function addSubmenuPrompt() {
  if (!menuConf.menus.length) { alert('Add a menu first'); return; }
  const mLabel = prompt('Add submenu to which menu? (enter menu label)');
  const menu = menuConf.menus.find(m => m.label === mLabel);
  if (!menu) { alert('Menu not found'); return; }
  if ((menu.entries||[]).length >= 20) { alert('Max 20 entries'); return; }
  const label = prompt('Submenu label:'); if (!label) return;
  menu.entries = menu.entries || [];
  menu.entries.push({ type: 'submenu', label, entries: [] }); renderMenuTree();
}

function delEntry(mi, ei, si) {
  const menu = menuConf.menus[mi]; if (!menu) return;
  if (si != null) menu.entries[ei].entries.splice(si, 1);
  else menu.entries.splice(ei, 1);
  renderMenuTree();
}

// ================================================================
//  Quick buttons
// ================================================================
async function loadQuickBtns() {
  try {
    const r = await fetch('/api/quickbtns');
    if (r.ok) { const arr = await r.json(); arr.forEach(({slot,macroid}) => { qbSlots[slot] = macroid; }); }
  } catch {}
  renderQbGrid(); renderQbAssignGrid();
}

async function saveAllQbtns() {
  for (let i = 0; i < 25; i++) {
    const fd = new FormData(); fd.append('slot', i); fd.append('macroid', qbSlots[i]);
    await fetch('/api/quickbtns/save', {method:'POST', body:fd});
  }
  showToast('Quick buttons saved', 2500);
}

function renderQbGrid() {
  const grid = document.getElementById('qbgrid'); grid.innerHTML = '';
  for (let i = 0; i < 25; i++) {
    const btn = document.createElement('button');
    const m = qbSlots[i] >= 0 ? getMacroById(qbSlots[i]) : null;
    if (m) {
      btn.className = 'qb';
      btn.textContent = m.name;
      btn.title = m.name + '\n' + m.cmd;
      btn.onclick = () => runMacro(m.id);
    } else {
      btn.className = 'qb empty';
      btn.textContent = '\u00b7';
      btn.title = 'Slot ' + (i+1) + ' (empty)';
    }
    grid.appendChild(btn);
  }
}

function renderQbAssignGrid() {
  const grid = document.getElementById('qb-assign-grid'); if (!grid) return;
  grid.innerHTML = '';
  for (let i = 0; i < 25; i++) {
    const cell = document.createElement('div');
    cell.className = 'qba'+(qbSlots[i]>=0?' assigned':'');
    const m = qbSlots[i] >= 0 ? getMacroById(qbSlots[i]) : null;
    cell.innerHTML = `<span class="slot-no">${i+1}</span><span class="macro-nm">${m?esc(m.name):'\u2014'}</span>`;
    cell.onclick = () => assignQb(i);
    cell.oncontextmenu = ev => { ev.preventDefault(); clearQb(i); };
    grid.appendChild(cell);
  }
}

function assignQb(slot) {
  if (!macroPool.macros.length) { alert('No macros in pool'); return; }
  const names = macroPool.macros.map(m => m.name).join('\n');
  const name = prompt('Enter macro name to assign to slot '+(slot+1)+':\n\n'+names);
  if (!name) return;
  const m = macroPool.macros.find(m => m.name === name);
  if (!m) { alert('Macro not found'); return; }
  qbSlots[slot] = m.id; renderQbGrid(); renderQbAssignGrid();
}

function clearQb(slot) { qbSlots[slot] = -1; renderQbGrid(); renderQbAssignGrid(); }

// ================================================================
//  Menu dropdown
// ================================================================
function toggleMenu(btn) {
  const wrap = btn.parentElement;
  const dd   = wrap.querySelector('.dropdown');
  const isOpen = dd.classList.contains('show');
  closeAllMenus();
  if (!isOpen) {
    dd.classList.add('show'); btn.classList.add('open');
    if (dd.querySelector('#sd-share-st')) sdShareRefresh();
  }
}

function closeAllMenus() {
  document.querySelectorAll('.dropdown.show').forEach(d => d.classList.remove('show'));
  document.querySelectorAll('.mnu-btn.open').forEach(b => b.classList.remove('open'));
}

document.addEventListener('click', e => { if (!e.target.closest('.mnu-wrap')) closeAllMenus(); });

// ================================================================
//  Editor modal
// ================================================================
function openEditor() {
  closeAllMenus();
  document.getElementById('modal-bg').classList.add('show');
  renderMacroList(); renderMenuTree(); renderQbAssignGrid();
}

function closeEditor() { document.getElementById('modal-bg').classList.remove('show'); }

function showTab(name, btn) {
  document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('tab-'+name).classList.add('active');
  btn.classList.add('active');
}

// ================================================================
//  General menu actions
// ================================================================
function fatBrowse() { closeAllMenus(); window.open('/api/files', '_blank'); }

function fatUpload() {
  closeAllMenus();
  const inp = document.createElement('input'); inp.type = 'file';
  inp.onchange = async () => {
    if (!inp.files.length) return;
    const fd = new FormData(); fd.append('file', inp.files[0]);
    const r = await fetch('/upload', {method:'POST', body:fd});
    showToast(r.ok ? 'Uploaded: '+inp.files[0].name : 'Upload failed', 3000);
  };
  inp.click();
}

function saveSession() {
  closeAllMenus();
  const blob = new Blob([sessionLog], {type:'text/plain'});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  const slotSuffix = sessSlot >= 0 ? '_' + (sessSlot + 1) : '';
  a.download = 'session'+slotSuffix+'_'+new Date().toISOString().replace(/[:.]/g,'').slice(0,15)+'.txt';
  a.click(); showToast('Session saved', 2000);
}

function fontInc() {
  closeAllMenus(); fontSize = Math.min(fontSize+1, 28);
  localStorage.setItem('fontSize', fontSize);
  term.options.fontSize = fontSize; termFit();
}

function fontDec() {
  closeAllMenus(); fontSize = Math.max(fontSize-1, 8);
  localStorage.setItem('fontSize', fontSize);
  term.options.fontSize = fontSize; termFit();
}

// SD Share — expose the SD card to the Pi as a USB drive
let sdSharing = false;
async function sdShareRefresh() {
  const st = document.getElementById('sd-share-st');
  try {
    const j = await (await fetch('/api/sd/status')).json();
    sdSharing = !!j.sharing;
    st.textContent = !j.sdPresent ? 'no card' : (sdSharing ? 'ON' : 'OFF');
  } catch (e) { st.textContent = '?'; }
}

async function sdShareToggle() {
  closeAllMenus();
  const url = sdSharing ? '/api/sd/share/stop' : '/api/sd/share/start';
  try {
    const r = await fetch(url, {method:'POST'});
    const t = await r.text();
    showToast(r.ok ? (sdSharing ? 'SD Share stopped' : 'SD Share started') : ('SD Share: ' + t), 3000);
  } catch (e) { showToast('SD Share: request failed', 3000); }
  sdShareRefresh();
}

function doClear() { closeAllMenus(); term.clear(); }

function doReconnect() {
  closeAllMenus();
  clearTimeout(wsReconnectTimer);
  if (ws) { ws.onclose = null; ws.close(); }
  wsReconnectTimer = setTimeout(wsConnect, 400);
}

// ================================================================
//  Utility
// ================================================================
function esc(s) {
  return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function showToast(msg, ms) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.style.display = 'block';
  clearTimeout(t._to);
  t._to = setTimeout(() => { t.style.display = 'none'; }, ms||2000);
}

// ================================================================
//  Boot
// ================================================================
document.addEventListener('DOMContentLoaded', async () => {
  initTerm();
  wsConnect();
  await loadMacroPool();
  await loadMenuConf();
  await loadQuickBtns();
});
</script>
</body>
</html>
"""

def main():
    lines = HTML.split('\n')
    if lines and lines[0] == '':
        lines = lines[1:]
    out_lines = [
        '// ============================================================',
        '//  terminal_html.h  \u2014  AUTO-GENERATED by gen_terminal_html.py',
        '//  DO NOT EDIT MANUALLY \u2014 re-run the generator instead.',
        '// ============================================================',
        '',
        '#pragma once',
        '',
        'static const char TERMINAL_HTML[] PROGMEM =',
    ]
    for line in lines:
        out_lines.append('  "' + c_escape(line) + '\\n"')
    out_lines.append('  "";')
    out_lines.append('')
    content = '\n'.join(out_lines) + '\n'
    with open(OUTPUT, 'w', encoding='utf-8') as f:
        f.write(content)
    # Sanity checks
    assert 'FitAddon' not in content, "FEL: FitAddon finns kvar!"
    assert 'termFit' in content, "FEL: termFit saknas!"
    assert 'flex-direction:column' in content, "FEL: vertikal QB saknas!"
    print(f"OK: {len(lines)} rader -> {OUTPUT}")
    print(f"Storlek: {len(content):,} bytes")
    print("OK: Ingen FitAddon, termFit finns, vertikal QB finns")

main()
