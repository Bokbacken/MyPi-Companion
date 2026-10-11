// ============================================================
//  storage_html.h  —  Storage page
//  Served at GET /storage
//
//  Cards:
//    SD Share   — expose the SD card to the Pi as a USB drive
//                 (/api/sd/status, /api/sd/share/start|stop, /api/sd/boot)
//    RIGSETUP   — USB install disk built from /rigsetup/ on FFat
//                 (/api/rigsetup/status|start|stop)
//  File manager (tabs "Internal (FFat)" and "SD Card"):
//    list      GET  /api/storage/ffat | /api/storage/sd
//    delete    POST /api/storage/delete   {path,fs}
//    copy      POST /api/storage/copy     {srcPath,srcFs,dstPath,dstFs}
//    upload    POST /api/storage/upload?fs=..&dir=..  (multipart "file")
//    download  GET  /api/storage/download?fs=..&path=..
//  Drag a file/folder onto the other tab to copy it there.
//
//  Edit this file directly (no generator script).
// ============================================================
#pragma once

static const char STORAGE_HTML[] PROGMEM = R"STORAGEHTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HMS Rig Console — Storage</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0d1117;--surf:#161b22;--elev:#21262d;--border:#30363d;
  --accent:#e94560;--blue:#3b82f6;--green:#22c55e;--amber:#f59e0b;
  --text:#e6edf3;--muted:#8b949e;--dim:#484f58;
  --mono:'Cascadia Code','Fira Mono',Consolas,monospace;
  --radius:8px;
}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
     background:var(--bg);color:var(--text);font-size:13px;min-height:100vh}
nav{background:var(--surf);border-bottom:1px solid var(--border);
    padding:8px 14px;display:flex;align-items:center;gap:6px;flex-wrap:wrap}
nav .logo{font-weight:700;font-size:14px;color:var(--accent);margin-right:10px}
nav a{color:var(--muted);text-decoration:none;font-size:12px;padding:5px 10px;border-radius:5px}
nav a:hover{background:var(--elev);color:var(--text)}
nav a.on{background:var(--elev);color:var(--text);font-weight:600}
main{max-width:980px;margin:0 auto;padding:16px;display:grid;gap:14px;
     grid-template-columns:1fr 1fr}
.card{background:var(--surf);border:1px solid var(--border);border-radius:var(--radius);padding:14px}
.card h2{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:var(--muted);
         margin-bottom:10px;display:flex;align-items:center;gap:8px}
.full{grid-column:1/-1}
.row{display:flex;align-items:center;gap:10px;margin:8px 0;flex-wrap:wrap}
.dot{width:10px;height:10px;border-radius:50%;background:var(--dim);display:inline-block}
.dot.ok{background:var(--green)}.dot.warn{background:var(--amber)}.dot.bad{background:var(--accent)}
.note{color:var(--muted);font-size:11px}
.btn{padding:6px 13px;border:none;border-radius:6px;cursor:pointer;font-size:12px;font-weight:600}
.btn-r{background:var(--accent);color:#fff}.btn-b{background:var(--blue);color:#fff}
.btn-g{background:var(--elev);color:var(--text);border:1px solid var(--border)}
.btn:disabled{opacity:.4;cursor:default}
label.chk{display:flex;align-items:center;gap:6px;cursor:pointer;color:var(--text)}
ul.files{list-style:none;font-family:var(--mono);font-size:12px;color:var(--muted)}
/* File manager */
.tabs{display:flex;gap:4px;border-bottom:1px solid var(--border);margin-bottom:8px}
.tab{padding:7px 14px;cursor:pointer;border:1px solid transparent;border-bottom:none;
     border-radius:6px 6px 0 0;color:var(--muted);user-select:none}
.tab.on{background:var(--elev);border-color:var(--border);color:var(--text);font-weight:600}
.tab.drop{outline:2px dashed var(--blue);color:var(--text)}
.bar{display:flex;align-items:center;gap:8px;margin-bottom:8px;flex-wrap:wrap}
.cwd{font-family:var(--mono);font-size:12px;color:var(--muted)}
#tree{border:1px solid var(--border);border-radius:6px;min-height:160px;max-height:55vh;overflow:auto}
#tree.drop{outline:2px dashed var(--green)}
.it{display:flex;align-items:center;gap:6px;padding:5px 8px;border-bottom:1px solid #1b2028;cursor:default}
.it:hover{background:var(--elev)}
.it.sel{background:#1f2a3a}
.it .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.it .dir{cursor:pointer;color:var(--text);font-weight:600}
.it .sz{color:var(--muted);font-family:var(--mono);font-size:11px;min-width:70px;text-align:right}
.it button{background:none;border:none;cursor:pointer;font-size:14px;padding:0 4px;color:var(--muted)}
.it button:hover{color:var(--text)}
.empty{padding:20px;text-align:center;color:var(--dim)}
#toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);background:var(--elev);
       border:1px solid var(--border);padding:8px 16px;border-radius:6px;display:none;z-index:9}
@media(max-width:720px){main{grid-template-columns:1fr}}
</style>
</head>
<body>
<nav>
  <span class="logo">⚡ HMS Rig</span>
  <a href="/">Home</a>
  <a href="/macros">Macros</a>
  <a href="/terminal">Terminal</a>
  <a href="/storage" class="on">Storage</a>
  <a href="/info">Advanced</a>
</nav>

<main>
  <!-- SD Share -->
  <section class="card">
    <h2>💾 SD Share</h2>
    <div class="row"><span class="dot" id="sd-dot"></span><span id="sd-card">Checking…</span></div>
    <div class="row"><span>Sharing:</span><b id="sd-share-st">—</b></div>
    <div class="row">
      <button class="btn btn-b" id="sd-btn" onclick="sdToggle()" disabled>Start sharing</button>
      <label class="chk"><input type="checkbox" id="sd-boot" onchange="sdBoot(this.checked)"> Enable on boot</label>
    </div>
    <p class="note">Exposes the SD card to the Pi as a USB drive. The ESP cannot access the card while sharing.</p>
  </section>

  <!-- RIGSETUP -->
  <section class="card">
    <h2>📦 RIGSETUP</h2>
    <div class="row"><span class="dot" id="rs-dot"></span><span>Status:</span><b id="rs-st">—</b></div>
    <div class="row">
      <button class="btn btn-b" onclick="rsStart()">Start</button>
      <button class="btn btn-g" onclick="rsStop()">Stop</button>
    </div>
    <ul class="files" id="rs-files"></ul>
    <p class="note">Files sourced from /rigsetup/ on internal storage</p>
  </section>

  <!-- File manager -->
  <section class="card full">
    <h2>🗂 Files</h2>
    <div class="tabs">
      <div class="tab on" id="tab-ffat" data-fs="ffat">Internal (FFat)</div>
      <div class="tab" id="tab-sd" data-fs="sd">SD Card</div>
    </div>
    <div class="bar">
      <span class="cwd">Folder: <b id="cwd">/</b></span>
      <span style="flex:1"></span>
      <button class="btn btn-g" onclick="refresh()">↻ Refresh</button>
      <label class="btn btn-b" style="cursor:pointer">⬆ Upload<input type="file" id="up" multiple style="display:none" onchange="uploadFiles(this.files);this.value=''"></label>
    </div>
    <div id="tree"></div>
    <p class="note" style="margin-top:8px">Click a folder to make it the upload/copy target. Drag a file or folder onto the other tab to copy it there. Drop files from your computer onto the list to upload.</p>
  </section>
</main>
<div id="toast"></div>

<script>
const FS_LABEL = {ffat:'Internal (FFat)', sd:'SD Card'};
let cur = 'ffat';                    // active tab
const cwd = {ffat:'/', sd:'/'};      // per-tab target folder
const expanded = {ffat:{}, sd:{}};       // expanded folders
let entries = [];
let sdSharing = false;

function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function toast(m,ms){const t=document.getElementById('toast');t.textContent=m;t.style.display='block';
  clearTimeout(t._h);t._h=setTimeout(()=>t.style.display='none',ms||2500);}
function fmt(n){return n<1024?n+' B':n<1048576?(n/1024).toFixed(1)+' KB':(n/1048576).toFixed(1)+' MB';}
async function post(url, data){
  const r = await fetch(url,{method:'POST',body:new URLSearchParams(data||{})});
  const t = await r.text(); if(!r.ok) throw new Error(t||r.status); return t;
}

// ── SD Share ──
async function sdRefresh(){
  try{
    const j = await (await fetch('/api/sd/status')).json();
    sdSharing = j.sharing;
    document.getElementById('sd-dot').className = 'dot ' + (j.sharing?'warn':j.sdPresent?'ok':'bad');
    document.getElementById('sd-card').textContent = j.sdPresent ? 'SD card present' : 'No SD card';
    document.getElementById('sd-share-st').textContent = j.sharing ? 'ON (Pi owns the card)' : 'OFF';
    const b = document.getElementById('sd-btn');
    b.textContent = j.sharing ? 'Stop sharing' : 'Start sharing';
    b.className = 'btn ' + (j.sharing ? 'btn-r' : 'btn-b');
    b.disabled = !j.sdPresent && !j.sharing;
    document.getElementById('sd-boot').checked = j.onAtBoot;
  }catch(e){ document.getElementById('sd-card').textContent = 'Status unavailable'; }
}
async function sdToggle(){
  try{ await post(sdSharing?'/api/sd/share/stop':'/api/sd/share/start');
       toast(sdSharing?'SD Share stopped':'SD Share started'); }
  catch(e){ toast('SD Share: '+e.message,4000); }
  await sdRefresh(); rsRefresh(); if(cur==='sd') refresh();
}
async function sdBoot(on){
  try{ await post('/api/sd/boot',{enable:on?'1':'0'}); toast(on?'SD Share will start on boot':'SD Share on boot disabled'); }
  catch(e){ toast('Error: '+e.message,4000); }
  sdRefresh();
}

// ── RIGSETUP ──
async function rsRefresh(){
  try{
    const j = await (await fetch('/api/rigsetup/status')).json();
    document.getElementById('rs-dot').className = 'dot ' + (j.active?'ok':'');
    document.getElementById('rs-st').textContent = j.active ? 'Active (visible to Pi)' : 'Inactive';
    document.getElementById('rs-files').innerHTML = j.files.length
      ? j.files.map(f=>'<li>'+esc(f.name)+' — '+fmt(f.size)+'</li>').join('')
      : '<li>(no files — built-in image will be used)</li>';
  }catch(e){ document.getElementById('rs-st').textContent = 'Status unavailable'; }
}
async function rsStart(){ try{ await post('/api/rigsetup/start'); toast('RIGSETUP started'); }catch(e){ toast('Error: '+e.message,4000); } rsRefresh(); sdRefresh(); }
async function rsStop(){ try{ await post('/api/rigsetup/stop'); toast('RIGSETUP stopped'); }catch(e){ toast('Error: '+e.message,4000); } rsRefresh(); }

// ── File manager ──
function selectTab(fs){
  cur = fs;
  document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('on', t.dataset.fs===fs));
  refresh();
}
async function refresh(){
  const tree = document.getElementById('tree');
  document.getElementById('cwd').textContent = cwd[cur];
  tree.innerHTML = '<div class="empty">Loading…</div>';
  try{
    const r = await fetch('/api/storage/'+cur);
    if(!r.ok) throw new Error(await r.text());
    entries = await r.json();
    render();
  }catch(e){ entries=[]; tree.innerHTML = '<div class="empty">'+esc(e.message)+'</div>'; }
}
function parentOf(p){ const i=p.lastIndexOf('/'); return i<=0?'/':p.substring(0,i); }
function visible(e){            // all ancestor folders expanded?
  let p = parentOf(e.path);
  while(p !== '/'){ if(!expanded[cur][p]) return false; p = parentOf(p); }
  return true;
}
function render(){
  const tree = document.getElementById('tree');
  const list = entries.slice().sort((a,b)=>{
    // depth-first order: compare path segments, folders before files at each level
    const A=a.path.split('/'), B=b.path.split('/');
    for(let i=1;i<Math.min(A.length,B.length);i++){
      if(A[i]!==B[i]){
        const ad = i<A.length-1 || a.dir, bd = i<B.length-1 || b.dir;
        if(ad!==bd) return ad?-1:1;
        return A[i].localeCompare(B[i]);
      }
    }
    return A.length-B.length;
  }).filter(visible);
  if(!list.length){ tree.innerHTML = '<div class="empty">(empty)</div>'; return; }
  tree.innerHTML = list.map(e=>{
    const depth = e.path.split('/').length-2;
    const pad = 'style="padding-left:'+(8+depth*18)+'px"';
    const p = esc(e.path);
    const sel = e.dir && cwd[cur]===e.path ? ' sel' : '';
    const icon = e.dir ? (expanded[cur][e.path]?'📂':'📁') : '📄';
    return '<div class="it'+sel+'" '+pad+' draggable="true" data-path="'+p+'" data-name="'+esc(e.name)+'">'
      + '<span>'+icon+'</span>'
      + (e.dir ? '<span class="nm dir" onclick="toggleDir(this)">'+esc(e.name)+'</span><span class="sz"></span>'
               : '<span class="nm">'+esc(e.name)+'</span><span class="sz">'+fmt(e.size)+'</span>'
                 + '<button title="Download" onclick="dl(this)">⬇</button>')
      + '<button title="Delete" onclick="del(this)">🗑</button></div>';
  }).join('');
}
function pathOf(el){ return el.closest('.it').dataset.path; }
function toggleDir(el){
  const p = pathOf(el);
  if(cwd[cur]===p && expanded[cur][p]){ expanded[cur][p]=false; cwd[cur]=parentOf(p); }
  else { expanded[cur][p]=true; cwd[cur]=p; }
  document.getElementById('cwd').textContent = cwd[cur];
  render();
}
function dl(el){
  location.href = '/api/storage/download?fs='+cur+'&path='+encodeURIComponent(pathOf(el));
}
async function del(el){
  const it = el.closest('.it'), p = it.dataset.path;
  if(!confirm('Delete '+p+' from '+FS_LABEL[cur]+'?')) return;
  try{ await post('/api/storage/delete',{path:p,fs:cur}); toast('Deleted '+it.dataset.name);
       if(cwd[cur]===p || cwd[cur].startsWith(p+'/')) cwd[cur]=parentOf(p); }
  catch(e){ toast('Delete failed: '+e.message,4000); }
  refresh(); if(cur==='ffat') rsRefresh();
}
async function uploadFiles(files){
  for(const f of files){
    const fd = new FormData(); fd.append('file', f, f.name);
    try{
      const r = await fetch('/api/storage/upload?fs='+cur+'&dir='+encodeURIComponent(cwd[cur]),{method:'POST',body:fd});
      const t = await r.text(); if(!r.ok) throw new Error(t);
      toast('Uploaded '+f.name);
    }catch(e){ toast('Upload failed: '+e.message,4000); }
  }
  refresh(); if(cur==='ffat') rsRefresh();
}

// Drag and drop: rows → other tab (copy); OS files → list (upload)
let drag = null;
document.getElementById('tree').addEventListener('dragstart', e=>{
  const it = e.target.closest('.it'); if(!it) return;
  drag = {fs:cur, path:it.dataset.path, name:it.dataset.name};
  e.dataTransfer.effectAllowed = 'copy';
  e.dataTransfer.setData('text/plain', it.dataset.path);
});
document.addEventListener('dragend', ()=>{ drag=null; });
document.querySelectorAll('.tab').forEach(tab=>{
  tab.addEventListener('click', ()=>selectTab(tab.dataset.fs));
  tab.addEventListener('dragover', e=>{ if(drag && drag.fs!==tab.dataset.fs){ e.preventDefault(); tab.classList.add('drop'); } });
  tab.addEventListener('dragleave', ()=>tab.classList.remove('drop'));
  tab.addEventListener('drop', async e=>{
    e.preventDefault(); tab.classList.remove('drop');
    if(!drag || drag.fs===tab.dataset.fs) return;
    const src = drag; drag = null;
    const dstFs = tab.dataset.fs, dir = cwd[dstFs];
    const dstPath = (dir==='/'?'':dir) + '/' + src.name;
    if(!confirm('Copy '+src.name+' to '+FS_LABEL[dstFs]+':'+dir+'?')) return;
    toast('Copying '+src.name+'…', 10000);
    try{ await post('/api/storage/copy',{srcPath:src.path,srcFs:src.fs,dstPath:dstPath,dstFs:dstFs});
         toast('Copied '+src.name+' to '+FS_LABEL[dstFs]); }
    catch(err){ toast('Copy failed: '+err.message,4000); }
    if(dstFs==='ffat') rsRefresh();
  });
});
const treeEl = document.getElementById('tree');
treeEl.addEventListener('dragover', e=>{ if(!drag && e.dataTransfer.types.includes('Files')){ e.preventDefault(); treeEl.classList.add('drop'); } });
treeEl.addEventListener('dragleave', ()=>treeEl.classList.remove('drop'));
treeEl.addEventListener('drop', e=>{
  treeEl.classList.remove('drop');
  if(drag || !e.dataTransfer.files.length) return;
  e.preventDefault(); uploadFiles(e.dataTransfer.files);
});

sdRefresh(); rsRefresh(); refresh();
setInterval(()=>{ sdRefresh(); rsRefresh(); }, 5000);
</script>
</body>
</html>
)STORAGEHTML";
