/* Kopi IDE — logika UI. Semua akses berkas/compile lewat jembatan Kopi.* */
'use strict';

const $ = (id) => document.getElementById(id);
const K = () => window.Kopi;

const S = {
  root: '',
  openPath: null,
  projectDir: null,
  dirty: false,
  fontSize: 13,
  running: false,
  lastKbc: null,
  tabs: [],
  buffers: new Map(),
  kbc: new Map(),
  problems: [],
  info: null,
  openDirs: new Set(),
  selPath: null,
  selDir: true,
  clip: null,
  busy: false,
};

/* ───────────── util ───────────── */
function toast(msg, ms) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.remove('hidden');
  clearTimeout(toast._t);
  toast._t = setTimeout(() => t.classList.add('hidden'), ms || 2200);
}
function base(p) { const i = p.lastIndexOf('/'); return i >= 0 ? p.slice(i + 1) : p; }
function dirname(p) { const i = p.lastIndexOf('/'); return i > 0 ? p.slice(0, i) : '/'; }
function isErr(s) {
  if (typeof s !== 'string') return false;
  const t = s.trim();
  if (!t.startsWith('{')) return false;
  try {
    const o = JSON.parse(t);
    if (!o) return false;
    if (o.err === true) return true;
    // kompatibilitas: error bridge lama selalu {ok:false,msg:...} tanpa code/log
    return o.ok === false && o.msg != null && o.code === undefined && o.log === undefined;
  } catch (e) { return false; }
}
function errMsg(s) { try { return JSON.parse(s).msg || 'gagal'; } catch (e) { return 'gagal'; } }

async function ask(title, value, hint) {
  return new Promise((resolve) => {
    $('mtitle').textContent = title;
    $('mhint').textContent = hint || '';
    const inp = $('minput');
    inp.value = value || '';
    $('modal').classList.remove('hidden');
    setTimeout(() => inp.focus(), 30);
    const done = (v) => {
      $('modal').classList.add('hidden');
      $('mok').onclick = null; $('mcancel').onclick = null;
      resolve(v);
    };
    $('mok').onclick = () => done(inp.value);
    $('mcancel').onclick = () => done(null);
  });
}
async function askYes(title, msg) {
  return new Promise((resolve) => {
    $('mtitle').textContent = title;
    $('mhint').textContent = msg || '';
    $('minput').classList.add('hidden');
    $('modal').classList.remove('hidden');
    const done = (v) => {
      $('modal').classList.add('hidden');
      $('minput').classList.remove('hidden');
      $('mok').onclick = null; $('mcancel').onclick = null;
      resolve(v);
    };
    $('mok').onclick = () => done(true);
    $('mcancel').onclick = () => done(false);
  });
}

/* ───────────── editor ───────────── */
const ed = () => $('editor');

function refreshEditor() {
  const ta = ed();
  $('hlcode').innerHTML = highlight(ta.value) + '\n';
  const lines = ta.value.split('\n').length;
  let g = '';
  for (let i = 1; i <= lines; i++) g += '<div>' + i + '</div>';
  $('gutter').innerHTML = g;
  syncScroll();
  $('empty').classList.toggle('hidden', !!S.openPath);
}

function syncScroll() {
  const ta = ed();
  $('hl').scrollTop = ta.scrollTop;
  $('hl').scrollLeft = ta.scrollLeft;
  $('gutter').scrollTop = ta.scrollTop;
}

function caretPos() {
  const ta = ed();
  const upto = ta.value.slice(0, ta.selectionStart);
  const lines = upto.split('\n');
  return { ln: lines.length, col: lines[lines.length - 1].length + 1 };
}

function updateStatus() {
  const p = caretPos();
  $('stPos').textContent = 'Ln ' + p.ln + ', Col ' + p.col;
  $('stFile').textContent = S.openPath ? base(S.openPath) + (S.dirty ? ' •' : '') : '—';
}

/* ───────────── autocomplete ───────────── */
const COMP = { active: false, start: 0, index: 0, items: [] };

function hideComplete() {
  COMP.active = false;
  COMP.items = [];
  $('complete').classList.add('hidden');
}

function caretPoint() {
  const ta = ed();
  const cs = getComputedStyle(ta);
  const m = document.createElement('div');
  m.style.cssText = 'position:absolute;top:0;left:0;visibility:hidden;' +
    'white-space:pre;box-sizing:border-box;border:0;';
  m.style.fontFamily = cs.fontFamily;
  m.style.fontSize = cs.fontSize;
  m.style.fontWeight = cs.fontWeight;
  m.style.fontStyle = cs.fontStyle;
  m.style.lineHeight = cs.lineHeight;
  m.style.letterSpacing = cs.letterSpacing;
  m.style.tabSize = cs.tabSize;
  m.style.padding = cs.padding;
  m.textContent = ta.value.slice(0, ta.selectionStart);
  const mk = document.createElement('span');
  mk.textContent = '\u200b';
  m.appendChild(mk);
  document.body.appendChild(m);
  const x = mk.offsetLeft - ta.scrollLeft;
  const y = mk.offsetTop - ta.scrollTop;
  m.remove();
  return { x: x, y: y, lh: parseFloat(cs.lineHeight) || 19 };
}

function positionComplete() {
  const pop = $('complete');
  if (pop.classList.contains('hidden')) return;
  const box = $('editorBox');
  const pt = caretPoint();
  const w = pop.offsetWidth || 190;
  const h = pop.offsetHeight || 40;
  let x = pt.x;
  let y = pt.y + pt.lh + 2;
  if (x + w > box.clientWidth - 6) x = box.clientWidth - w - 6;
  if (x < 4) x = 4;
  if (y + h > box.clientHeight - 6) y = pt.y - h - 2;
  if (y < 4) y = 4;
  pop.style.left = x + 'px';
  pop.style.top = y + 'px';
}

function renderComplete() {
  const pop = $('complete');
  pop.innerHTML = '';
  COMP.items.forEach((it, i) => {
    const row = document.createElement('div');
    row.className = 'ci ' + it.kind + (i === COMP.index ? ' on' : '');
    const k = document.createElement('span');
    k.className = 'k';
    k.textContent = it.kind.charAt(0);
    const l = document.createElement('span');
    l.className = 'l';
    l.textContent = it.label;
    row.append(k, l);
    const pick = (e) => { e.preventDefault(); e.stopPropagation(); acceptComplete(i); };
    row.addEventListener('mousedown', pick);
    row.addEventListener('touchstart', pick, { passive: false });
    pop.appendChild(row);
  });
  pop.classList.remove('hidden');
}

function updateComplete(force) {
  const C = window.KopiComplete;
  if (!C || !S.openPath) { hideComplete(); return; }
  const ta = ed();
  const r = C.suggest(ta.value, ta.selectionStart, C.collectWords(ta.value), !!force);
  if (!r || !r.items.length) { hideComplete(); return; }
  // saran 1 huruf saat mengetik kata terlalu berisik; tetap tampil bila dipaksa
  if (!force && !r.recv && r.prefix.length < 1) { hideComplete(); return; }
  COMP.active = true;
  COMP.start = r.start;
  COMP.index = 0;
  COMP.items = r.items;
  renderComplete();
  positionComplete();
}

function acceptComplete(i) {
  const it = COMP.items[i];
  if (!it) return;
  const ta = ed();
  const end = ta.selectionStart;
  const ins = it.insert;
  const caret = it.caret == null ? ins.length : it.caret;
  ta.value = ta.value.slice(0, COMP.start) + ins + ta.value.slice(end);
  ta.selectionStart = ta.selectionEnd = COMP.start + caret;
  markDirty();
  hideComplete();
  refreshEditor();
  updateStatus();
  ta.focus();
}


function setFont(px) {
  px = Math.max(10, Math.min(30, px));
  S.fontSize = px;
  try { localStorage.setItem('kopi.font', String(px)); } catch (e) {}
  const lh = (px * 1.45).toFixed(2) + 'px';
  [ed(), $('hl'), $('gutter')].forEach((el) => {
    el.style.fontSize = px + 'px';
    el.style.lineHeight = lh;
  });
  $('stZoom').textContent = Math.round(px / 13 * 100) + '%';
  hideComplete();
  refreshEditor();
}

function insertText(txt) {
  const ta = ed();
  const a = ta.selectionStart, b = ta.selectionEnd;
  ta.value = ta.value.slice(0, a) + txt + ta.value.slice(b);
  ta.selectionStart = ta.selectionEnd = a + txt.length;
  markDirty();
  refreshEditor();
  updateStatus();
}

function jumpToLine(n) {
  const ta = ed();
  const lines = ta.value.split('\n');
  n = Math.max(1, Math.min(lines.length, n));
  let idx = 0;
  for (let i = 0; i < n - 1; i++) idx += lines[i].length + 1;
  ta.focus();
  ta.selectionStart = ta.selectionEnd = idx;
  const lh = parseFloat(getComputedStyle(ta).lineHeight) || 19;
  ta.scrollTop = Math.max(0, (n - 4) * lh);
  refreshEditor();
  updateStatus();
}

/* ───────────── sidebar / pohon berkas ───────────── */
function listDir(path) {
  const r = K().listDir(path);
  if (isErr(r)) return [];
  try { const a = JSON.parse(r); return Array.isArray(a) ? a : []; } catch (e) { return []; }
}

function makeRow(it, depth) {
  const row = document.createElement('div');
  const cut = S.clip && S.clip.mode === 'cut' && it.p === S.clip.path;
  row.className = 'row' + (it.d ? ' dir' : '') +
    (!it.d && it.p === S.openPath ? ' active' : '') +
    (it.p === S.selPath ? ' sel' : '') + (cut ? ' cut' : '');
  row.style.paddingLeft = (10 + depth * 14) + 'px';
  const arrow = document.createElement('span');
  arrow.className = 'arrow';
  arrow.textContent = it.d ? (S.openDirs.has(it.p) ? '▾' : '▸') : '';
  const ic = document.createElement('span');
  ic.className = 'ic';
  ic.textContent = it.d ? '📁' : (it.n.endsWith('.kop') ? '☕' : '📄');
  const nm = document.createElement('span');
  nm.className = 'nm';
  nm.textContent = it.n;
  const more = document.createElement('button');
  more.className = 'rowmenu';
  more.textContent = '⋯';
  more.title = 'Menu berkas';
  row.append(arrow, ic, nm, more);

  let timer = null, long = false;
  const openMenu = () => {
    clearTimeout(timer);
    long = true;
    selectRow(it);
    fileMenu(it);
  };
  more.addEventListener('click', (e) => { e.stopPropagation(); openMenu(); });

  row.addEventListener('touchstart', () => {
    long = false;
    timer = setTimeout(openMenu, 620);
  }, { passive: true });
  row.addEventListener('touchend', () => { clearTimeout(timer); }, { passive: true });
  row.addEventListener('touchcancel', () => { clearTimeout(timer); }, { passive: true });
  row.addEventListener('touchmove', () => { clearTimeout(timer); }, { passive: true });
  row.addEventListener('contextmenu', (e) => { e.preventDefault(); openMenu(); });

  row.addEventListener('click', () => {
    if (long) { long = false; return; }
    selectRow(it, false);
    if (it.d) {
      if (S.openDirs.has(it.p)) S.openDirs.delete(it.p);
      else S.openDirs.add(it.p);
      renderTree();
    } else {
      openFile(it.p);
      closeSidebar();
    }
  });
  return row;
}

function selectRow(it, rerender) {
  S.selPath = it.p;
  S.selDir = !!it.d;
  if (rerender !== false) renderTree();
}

function insidePath(p, root) {
  return !!p && !!root && (p === root || p.indexOf(root + '/') === 0);
}

function currentDir() {
  if (!S.selPath) return S.root;
  return S.selDir ? S.selPath : dirname(S.selPath);
}

function validName(n) {
  return !!n && n !== '.' && n !== '..' &&
    n.indexOf('/') < 0 && n.indexOf('\\') < 0 && n.indexOf('\0') < 0;
}

function fmtSize(n) {
  n = Number(n) || 0;
  if (n < 1024) return n + ' B';
  const u = ['KB', 'MB', 'GB', 'TB'];
  let i = -1;
  do { n /= 1024; i++; } while (n >= 1024 && i < u.length - 1);
  return n.toFixed(1) + ' ' + u[i];
}

function fmtTime(ms) {
  const n = Number(ms) || 0;
  if (!n) return '—';
  const d = new Date(n);
  try { return d.toLocaleString('id-ID'); } catch (e) { return d.toString(); }
}

function fmtPerm(o) {
  return (o.r ? 'r' : '-') + (o.w ? 'w' : '-') + (o.x ? 'x' : '-');
}

/* ── bottom sheet aksi ── */
function closeSheet() {
  $('sheet').classList.add('hidden');
  $('sdetail').classList.add('hidden');
  $('sdetail').textContent = '';
  $('slist').innerHTML = '';
}

function sheet(title, items, detail) {
  $('stitle').textContent = title;
  const box = $('slist');
  box.innerHTML = '';
  const pre = $('sdetail');
  if (detail) { pre.textContent = detail; pre.classList.remove('hidden'); }
  else { pre.textContent = ''; pre.classList.add('hidden'); }
  for (const it of (items || [])) {
    const b = document.createElement('button');
    b.className = 'sitem' + (it.danger ? ' danger' : '') + (it.disabled ? ' off' : '');
    b.disabled = !!it.disabled;
    const i = document.createElement('span');
    i.className = 'si';
    i.textContent = it.icon || '';
    const l = document.createElement('span');
    l.className = 'sl';
    l.textContent = it.label;
    b.append(i, l);
    b.addEventListener('click', () => {
      closeSheet();
      try { const r = it.run(); if (r && r.catch) r.catch((e) => toast(String(e))); }
      catch (e) { toast(String(e)); }
    });
    box.appendChild(b);
  }
  $('sheet').classList.remove('hidden');
}

function fileMenu(it) {
  const items = [];
  const isRoot = it.p === S.root;
  if (it.d) {
    items.push({ icon: '📄', label: 'Berkas baru', run: () => newFile(it.p) });
    items.push({ icon: '📁', label: 'Folder baru', run: () => newFolder(it.p) });
  }
  if (!isRoot) {
    items.push({ icon: '✏️', label: 'Ganti nama', run: () => renameItem(it) });
    items.push({ icon: '📋', label: 'Salin', run: () => setClip(it.p, 'copy', !!it.d) });
    items.push({ icon: '✂️', label: 'Pindah (cut)', run: () => setClip(it.p, 'cut', !!it.d) });
  }
  items.push({
    icon: '📥',
    label: S.clip ? 'Tempel: ' + base(S.clip.path) : 'Tempel',
    disabled: !S.clip,
    run: () => pasteInto(it.d ? it.p : dirname(it.p))
  });
  items.push({ icon: 'ℹ️', label: 'Detail', run: () => showDetails(it.p) });
  if (it.p !== S.root) {
    items.push({ icon: '🗑️', label: 'Hapus', danger: true, run: () => deleteItem(it) });
  }
  sheet(it.n, items);
}

async function newFile(dir) {
  let nm = await ask('Berkas baru', 'Baru.kop', 'dibuat di ' + base(dir));
  if (nm == null) return;
  nm = nm.trim();
  if (!validName(nm)) { toast('nama berkas tidak valid'); return; }
  if (nm.indexOf('.') < 0) nm += '.kop';
  const path = dir + '/' + nm;
  const r = K().createFile(path);
  if (isErr(r)) { toast(errMsg(r)); return; }
  S.openDirs.add(dir);
  S.selPath = path;
  S.selDir = false;
  renderTree();
  if (nm.endsWith('.kop')) openFile(path);
  toast('dibuat: ' + nm);
}

async function newFolder(dir) {
  let nm = await ask('Folder baru', 'Folder', 'dibuat di ' + base(dir));
  if (nm == null) return;
  nm = nm.trim();
  if (!validName(nm)) { toast('nama folder tidak valid'); return; }
  const path = dir + '/' + nm;
  const r = K().mkdirs(path);
  if (isErr(r)) { toast(errMsg(r)); return; }
  S.openDirs.add(dir);
  S.selPath = path;
  S.selDir = true;
  renderTree();
  toast('folder dibuat: ' + nm);
}

async function renameItem(it) {
  const nm = await ask('Ganti nama', it.n, it.d ? 'nama folder' : 'nama berkas');
  if (nm == null || nm.trim() === it.n) return;
  if (!validName(nm.trim())) { toast('nama tidak valid'); return; }
  const dst = dirname(it.p) + '/' + nm.trim();
  const r = K().rename(it.p, dst);
  if (isErr(r)) { toast(errMsg(r)); return; }
  retargetTabs(it.p, dst);
  S.selPath = dst;
  S.selDir = !!it.d;
  renderTree();
  toast('nama diubah');
}

async function deleteItem(it) {
  const msg = it.d ? 'Folder beserta seluruh isinya akan dihapus.' : 'Berkas akan dihapus.';
  if (!(await askYes('Hapus ' + it.n + '?', msg + '\n' + it.p))) return;
  const r = K().delete(it.p);
  if (isErr(r)) { toast(errMsg(r)); return; }
  dropTabsUnder(it.p);
  if (S.clip && insidePath(S.clip.path, it.p)) S.clip = null;
  if (insidePath(S.selPath, it.p)) { S.selPath = null; S.selDir = true; }
  renderTree();
  toast('terhapus: ' + it.n);
}

function setClip(path, mode, isDir) {
  S.clip = { path: path, mode: mode, dir: !!isDir };
  renderTree();
  toast(mode === 'cut'
    ? '✂️ siap dipindah: ' + base(path) + ' — pilih folder lalu Tempel'
    : '📋 disalin: ' + base(path) + ' — pilih folder lalu Tempel', 3400);
}

async function pasteInto(dir) {
  if (!S.clip) { toast('clipboard kosong'); return; }
  const c = S.clip;
  if (dirname(c.path) === dir) { toast('sudah berada di folder itu'); return; }
  if (c.dir && insidePath(dir, c.path)) { toast('tidak bisa menempel ke dalam folder itu'); return; }
  const r = c.mode === 'cut' ? K().moveTo(c.path, dir) : K().copyTo(c.path, dir);
  if (isErr(r)) { toast(errMsg(r)); return; }
  let o;
  try { o = JSON.parse(r); } catch (e) { toast('jawaban rusak'); return; }
  const dst = o.path || (dir + '/' + base(c.path));
  const wasDir = c.dir;
  if (c.mode === 'cut') {
    retargetTabs(c.path, dst);
    S.clip = null;
  }
  S.openDirs.add(dir);
  S.selPath = dst;
  S.selDir = wasDir;
  renderTree();
  toast((c.mode === 'cut' ? 'dipindah' : 'disalin') + ': ' + base(dst));
}

function showDetails(path) {
  const r = K().stat(path);
  if (isErr(r)) { toast(errMsg(r)); return; }
  let o;
  try { o = JSON.parse(r); } catch (e) { toast('detail rusak'); return; }
  const lines = [
    'Nama    : ' + o.n,
    'Tipe    : ' + (o.d ? 'Folder' : 'Berkas'),
    'Ukuran  : ' + (o.d ? fmtSize(o.s) + ' \u00b7 ' + o.count + ' item'
                       : fmtSize(o.s) + ' (' + o.s + ' byte)'),
    'Diubah  : ' + fmtTime(o.mtime),
    'Izin    : ' + fmtPerm(o),
    '',
    'Jalur   :',
    o.p
  ];
  sheet('Detail', [{ icon: '✖', label: 'Tutup', run: () => {} }], lines.join('\n'));
}

function dropTabsUnder(path) {
  const kill = S.tabs.filter((p) => insidePath(p, path));
  if (!kill.length) return;
  const wasOpen = S.openPath && insidePath(S.openPath, path);
  for (const p of kill) { S.buffers.delete(p); S.kbc.delete(p); }
  S.tabs = S.tabs.filter((p) => !insidePath(p, path));
  if (wasOpen) {
    const next = S.tabs[S.tabs.length - 1];
    S.openPath = null;
    if (next) activateTab(next); else clearEditor();
  }
  renderTabs();
  saveTabsState();
}

function retargetTabs(oldPath, newPath) {
  const map = (p) => {
    if (!p) return p;
    if (p === oldPath) return newPath;
    if (p.indexOf(oldPath + '/') === 0) return newPath + p.slice(oldPath.length);
    return p;
  };
  const nb = new Map();
  S.buffers.forEach((b, p) => nb.set(map(p), b));
  S.buffers = nb;
  const nk = new Map();
  S.kbc.forEach((k, p) => nk.set(map(p), k));
  S.kbc = nk;
  S.tabs = S.tabs.map(map);
  const op = map(S.openPath);
  if (op !== S.openPath) {
    S.openPath = op;
    S.projectDir = projectOf(op);
    $('projName').textContent = base(S.projectDir);
  }
  const nd = new Set();
  S.openDirs.forEach((d) => nd.add(map(d)));
  S.openDirs = nd;
  if (S.clip) S.clip.path = map(S.clip.path);
  S.selPath = map(S.selPath);
  renderTabs();
  saveTabsState();
}

function renderTree() {
  const t = $('tree');
  t.innerHTML = '';
  const rootIt = { n: base(S.root) || 'KopiProject', d: 1, p: S.root };
  t.appendChild(makeRow(rootIt, 0));
  S.openDirs.add(S.root);
  const walk = (dir, depth) => {
    if (!S.openDirs.has(dir)) return;
    for (const it of listDir(dir)) {
      t.appendChild(makeRow(it, depth));
      if (it.d) walk(it.p, depth + 1);
    }
  };
  walk(S.root, 1);
  $('sidePath').textContent = S.root;
}

function openSidebar() { $('sidebar').classList.add('open'); $('scrim').classList.remove('hidden'); }
function closeSidebar() { $('sidebar').classList.remove('open'); $('scrim').classList.add('hidden'); }

/* ───────────── tab berkas ───────────── */
function saveTabsState() {
  try {
    localStorage.setItem('kopi.tabs', JSON.stringify(S.tabs));
    localStorage.setItem('kopi.last', S.openPath || '');
  } catch (e) {}
}

function stashBuffer() {
  if (!S.openPath) return;
  const ta = ed();
  S.buffers.set(S.openPath, {
    text: ta.value,
    dirty: S.dirty,
    sel: ta.selectionStart,
    scrollTop: ta.scrollTop,
    scrollLeft: ta.scrollLeft,
  });
}

function markDirty() {
  S.dirty = true;
  const b = S.buffers.get(S.openPath);
  if (b) { b.dirty = true; b.text = ed().value; }
}

function renderTabs() {
  const bar = $('tabs');
  bar.innerHTML = '';
  for (const p of S.tabs) {
    const b = S.buffers.get(p) || {};
    const t = document.createElement('div');
    t.className = 'ftab' + (p === S.openPath ? ' active' : '');
    t.title = p;
    const nm = document.createElement('span');
    nm.className = 'tn';
    nm.textContent = (b.dirty ? '● ' : '') + base(p);
    const x = document.createElement('button');
    x.className = 'tx';
    x.textContent = '×';
    x.setAttribute('aria-label', 'Tutup ' + base(p));
    x.addEventListener('click', (e) => { e.stopPropagation(); closeTab(p); });
    t.append(nm, x);
    t.addEventListener('click', () => activateTab(p));
    bar.appendChild(t);
  }
  // geser bar tab secukupnya agar tab aktif terlihat (tanpa scroll halaman)
  const act = bar.querySelector('.ftab.active');
  if (act) {
    const left = act.offsetLeft;
    const right = left + act.offsetWidth;
    if (left < bar.scrollLeft) bar.scrollLeft = left;
    else if (right > bar.scrollLeft + bar.clientWidth) bar.scrollLeft = right - bar.clientWidth;
  }
}

function clearEditor() {
  S.openPath = null;
  S.projectDir = null;
  S.dirty = false;
  ed().value = '';
  $('projName').textContent = 'Kopi IDE';
  refreshEditor();
  updateStatus();
  hideComplete();
  renderTabs();
}

function activateTab(path) {
  if (path === S.openPath) { renderTabs(); return; }
  stashBuffer();
  if (S.tabs.indexOf(path) < 0) S.tabs.push(path);
  let b = S.buffers.get(path);
  if (!b) {
    const txt = K().readFile(path);
    if (isErr(txt)) {
      toast(errMsg(txt));
      S.tabs = S.tabs.filter((x) => x !== path);
      renderTabs();
      return;
    }
    b = { text: txt, dirty: false, sel: 0, scrollTop: 0, scrollLeft: 0 };
    S.buffers.set(path, b);
  }
  S.openPath = path;
  S.projectDir = projectOf(path);
  S.problems = [];
  renderProblems();
  $('projName').textContent = base(S.projectDir);
  const ta = ed();
  ta.value = b.text;
  S.dirty = !!b.dirty;
  refreshEditor();
  updateStatus();
  ta.selectionStart = ta.selectionEnd = Math.min(b.sel || 0, ta.value.length);
  ta.scrollTop = b.scrollTop || 0;
  ta.scrollLeft = b.scrollLeft || 0;
  syncScroll();
  hideComplete();
  renderTabs();
  renderTree();
  saveTabsState();
}

function closeTab(path) {
  if (path === S.openPath) stashBuffer();
  const b = S.buffers.get(path);
  if (b && b.dirty) {
    const r = K().writeFile(path, b.text);
    if (isErr(r)) { toast(errMsg(r)); return; }
  }
  const idx = S.tabs.indexOf(path);
  S.tabs = S.tabs.filter((p) => p !== path);
  S.buffers.delete(path);
  S.kbc.delete(path);
  if (S.openPath === path) {
    const next = S.tabs[Math.min(idx, S.tabs.length - 1)];
    S.openPath = null;
    if (next) activateTab(next); else clearEditor();
  }
  renderTabs();
  saveTabsState();
  renderTree();
}

/* ───────────── buka / simpan ───────────── */
function projectOf(path) {
  const i = path.indexOf('/src/');
  if (i > 0) return path.slice(0, i);
  return dirname(path);
}

function openFile(path) {
  if (S.tabs.indexOf(path) >= 0) { activateTab(path); return; }
  const txt = K().readFile(path);
  if (isErr(txt)) { toast(errMsg(txt)); return; }
  stashBuffer();
  S.tabs.push(path);
  S.buffers.set(path, { text: txt, dirty: false, sel: 0, scrollTop: 0, scrollLeft: 0 });
  S.openPath = null;
  activateTab(path);
}

function saveFile(showToast) {
  if (!S.openPath) return true;
  const r = K().writeFile(S.openPath, ed().value);
  if (isErr(r)) { toast(errMsg(r)); return false; }
  S.dirty = false;
  const b = S.buffers.get(S.openPath);
  if (b) { b.dirty = false; b.text = ed().value; }
  renderTabs();
  updateStatus();
  if (showToast) toast('tersimpan');
  return true;
}

/* ───────────── panel log ───────────── */
function appendLog(text, cls) {
  const log = $('log');
  const atBottom = log.scrollTop + log.clientHeight >= log.scrollHeight - 24;
  const span = document.createElement('span');
  if (cls) span.className = cls;
  else {
    const t = text.toLowerCase();
    if (t.indexOf('error') >= 0 || t.indexOf('tidak cocok') >= 0 || t.indexOf('gagal') >= 0) span.className = 'e';
    else if (text.startsWith('✓')) span.className = 's';
    else if (t.indexOf('warning') >= 0 || t.indexOf('peringatan') >= 0) span.className = 'w';
    else if (text.startsWith('▸') || text.startsWith('▶')) span.className = 'meta';
  }
  span.textContent = text;
  log.appendChild(span);
  if (atBottom) log.scrollTop = log.scrollHeight;
}

function setTab(name) {
  document.querySelectorAll('.tab').forEach((b) => b.classList.toggle('active', b.dataset.tab === name));
  $('log').classList.toggle('active', name === 'out');
  $('problems').classList.toggle('active', name === 'prob');
}

function parseProblems(logText) {
  const list = [];
  const re = /baris\s+(\d+):\s*([^\n]+)/g;
  let m;
  while ((m = re.exec(logText)) !== null) {
    list.push({ line: parseInt(m[1], 10), msg: m[2].trim() });
  }
  S.problems = list;
  renderProblems();
}

function renderProblems() {
  const box = $('problems');
  box.innerHTML = '';
  $('probCount').classList.toggle('hidden', S.problems.length === 0);
  $('probCount').textContent = String(S.problems.length);
  if (!S.problems.length) {
    box.innerHTML = '<div class="empty">Tidak ada masalah.</div>';
    return;
  }
  for (const p of S.problems) {
    const d = document.createElement('div');
    d.className = 'pr';
    d.innerHTML = '<span class="no">' + p.line + '</span><span class="tx"></span>';
    d.querySelector('.tx').textContent = p.msg;
    d.addEventListener('click', () => { setTab('prob'); jumpToLine(p.line); });
    box.appendChild(d);
  }
}

function openPanel() { $('panel').classList.remove('closed'); }
function togglePanel() { $('panel').classList.toggle('closed'); }

/* ───────────── build & run ───────────── */
function build() {
  if (!S.openPath) { toast('Buka berkas .kop dulu'); return; }
  if (!saveFile(false)) return;
  const proj = projectOf(S.openPath);
  const rel = S.openPath.slice(proj.length + 1);
  S.kbc.delete(S.openPath);
  openPanel();
  setTab('out');
  appendLog('\n▸ Build ' + rel + '  ·  kopicc -t vm\n', 'meta');
  const r = K().compile(proj, rel);
  if (isErr(r)) { appendLog('✗ ' + errMsg(r) + '\n', 'e'); return; }
  let o;
  try { o = JSON.parse(r); } catch (e) { appendLog('✗ jawaban compiler rusak\n', 'e'); return; }
  if (o.log) appendLog(o.log);
  if (o.ok) {
    S.lastKbc = o.kbc;
    S.kbc.set(S.openPath, o.kbc);
    S.problems = [];
    renderProblems();
    appendLog('✓ build sukses (' + o.ms + ' ms) → out/' + base(o.kbc) + '\n', 's');
  } else {
    parseProblems(o.log || '');
    appendLog('✗ build gagal (kode ' + o.code + ')\n', 'e');
    if (!o.log) appendLog('  ((compiler tidak menulis pesan; sumber: ' + o.src + ')\n', 'w');
    setTab('prob');
  }
}

function run() {
  if (S.running) { K().stop(); return; }
  if (!S.openPath) { toast('Buka berkas .kop dulu'); return; }
  let kbc = S.kbc.get(S.openPath);
  if (!kbc || kbc.indexOf(base(S.openPath).replace(/\.kop$/, '')) < 0) {
    build();
    kbc = S.kbc.get(S.openPath);
  }
  if (!kbc) return;
  const proj = projectOf(S.openPath);
  openPanel();
  setTab('out');
  const r = K().run(proj, kbc);
  if (isErr(r)) { appendLog('✗ ' + errMsg(r) + '\n', 'e'); return; }
  S.running = true;
  $('btnRun').textContent = '■';
}

window.IDE = {
  onRunStart(label) {
    appendLog('\n▶ run ' + label + '\n', 'meta');
  },
  onOutput(text) {
    appendLog(text);
  },
  onExit(code, stopped) {
    S.running = false;
    $('btnRun').textContent = '▶';
    appendLog((stopped ? '■ dihentikan' : '— keluar (kode ' + code + ')') + '\n', code === 0 || stopped ? 'meta' : 'e');
  },
  handleBack() {
    if (!$('sheet').classList.contains('hidden')) { closeSheet(); return true; }
    if ($('sidebar').classList.contains('open')) { closeSidebar(); return true; }
    if (!$('panel').classList.contains('closed')) { togglePanel(); return true; }
    if (K() && K().exitApp) K().exitApp();
    return false;
  },
};

/* ───────────── proyek baru ───────────── */
async function newProject() {
  const name = await ask('Proyek baru', 'Proyek' + (Math.floor(Math.random() * 900) + 100), 'huruf/angka/_/-');
  if (!name) return;
  const r = K().createProject(name);
  if (isErr(r)) { toast(errMsg(r)); return; }
  let o;
  try { o = JSON.parse(r); } catch (e) { toast('gagal membuat proyek'); return; }
  S.openDirs.add(o.project);
  renderTree();
  openFile(o.main);
  toast('proyek dibuat: ' + base(o.project));
}

/* ───────────── diagnostik biner ───────────── */
function testBinaries() {
  if (!K() || !K().selfTest) { toast('self-test hanya tersedia di app'); return; }
  openPanel();
  setTab('out');
  appendLog('\n▸ Tes biner…\n', 'meta');
  const r = K().selfTest();
  if (isErr(r)) { appendLog('✗ ' + errMsg(r) + '\n', 'e'); return; }
  let o;
  try { o = JSON.parse(r); } catch (e) { appendLog('✗ jawaban rusak\n', 'e'); return; }
  appendLog('  targetSdk ' + o.targetSdk + ' · Android SDK ' + o.sdk + '\n', 'meta');
  appendLog('  nativeLibraryDir: ' + o.nativeDir + '\n', 'meta');
  appendLog('  modul Kopi: ' + o.kopModules + ' .kop di ' + o.kopLib + '\n', 'meta');
  for (const l of (o.libs || [])) {
    appendLog((l.ok ? '✓ ' : '✗ ') + l.name + (l.code != null ? ' (kode ' + l.code + ')' : '') + '\n',
      l.ok ? 's' : 'e');
    if (l.out) appendLog('  ' + String(l.out).split('\n').join('\n  ') + '\n', l.ok ? '' : 'e');
  }
  const e = o.e2e;
  if (e) {
    appendLog((e.ok ? '✓ ' : '✗ ') + 'uji end-to-end (import Str; compile + run)\n',
      e.ok ? 's' : 'e');
    if (e.buildLog) appendLog('  ' + String(e.buildLog).split('\n').join('\n  ') + '\n', e.ok ? '' : 'e');
    if (e.runLog) appendLog('  ' + String(e.runLog).split('\n').join('\n  ') + '\n', e.ok ? '' : 'e');
    if (e.buildCode != null) appendLog('  kode build ' + e.buildCode +
      (e.runCode != null ? ', kode run ' + e.runCode : '') + '\n', 'meta');
  }
  const fs = o.fs;
  if (fs) {
    appendLog((fs.ok ? '✓ ' : '✗ ') + 'uji berkas (buat/salin/pindah/detail/hapus)\n',
      fs.ok ? 's' : 'e');
    if (fs.unique) appendLog('  nama bentrok → ' + fs.unique + '\n', 'meta');
    if (fs.msg) appendLog('  ' + String(fs.msg) + '\n', 'e');
  }
}

/* ───────────── init ───────────── */
function init() {
  if (!K()) {
    toast('Mode browser: jembatan Kopi tidak ada (compile/run nonaktif)', 5000);
    $('projName').textContent = 'Kopi IDE (demo)';
    refreshEditor();
    return;
  }
  let info = null;
  try { info = JSON.parse(K().info()); } catch (e) {}
  S.info = info;
  S.root = (info && info.projects) || '/storage/emulated/0/KopiProject';
  $('sideInfo').textContent = info && info.libs
    ? 'v' + (info.version || '?') + ' · ' +
      info.libs.map((l) => l.name + (l.exists ? ' ✓' : ' ✗')).join(' · ') +
      ' · modul ' + (info.kopModules || 0) + ' · SDK ' + info.sdk + ' · tsdk ' + info.targetSdk
    : '';
  if (info && !info.storage) $('storagebar').classList.remove('hidden');

  try {
    const f = localStorage.getItem('kopi.font');
    if (f) S.fontSize = parseFloat(f) || 13;
  } catch (e) {}

  renderTree();
  setFont(S.fontSize);
  refreshEditor();
  updateStatus();

  // pulihkan tab berkas dari sesi sebelumnya
  let saved = [];
  try { saved = JSON.parse(localStorage.getItem('kopi.tabs') || '[]'); } catch (e) { saved = []; }
  if (!Array.isArray(saved)) saved = [];
  for (const p of saved) {
    if (typeof p !== 'string' || S.tabs.indexOf(p) >= 0) continue;
    const t = K().readFile(p);
    if (isErr(t)) continue;
    S.tabs.push(p);
    S.buffers.set(p, { text: t, dirty: false, sel: 0, scrollTop: 0, scrollLeft: 0 });
  }
  let last = null;
  try { last = localStorage.getItem('kopi.last'); } catch (e) {}
  const target = (last && S.tabs.indexOf(last) >= 0) ? last : S.tabs[S.tabs.length - 1];
  S.openPath = null;
  if (target) activateTab(target);
  else { renderTabs(); refreshEditor(); updateStatus(); }
}

/* ───────────── event binding ───────────── */
document.addEventListener('DOMContentLoaded', () => {
  $('btnMenu').addEventListener('click', () => {
    if ($('sidebar').classList.contains('open')) closeSidebar(); else openSidebar();
  });
  $('scrim').addEventListener('click', closeSidebar);
  $('btnRefresh').addEventListener('click', () => { renderTree(); toast('dimuat ulang'); });
  $('btnNewFile').addEventListener('click', () => newFile(currentDir()));
  $('btnNewFolder').addEventListener('click', () => newFolder(currentDir()));
  $('scancel').addEventListener('click', closeSheet);
  $('sheet').addEventListener('click', (e) => { if (e.target === $('sheet')) closeSheet(); });
  $('btnNew').addEventListener('click', newProject);
  $('btnNew2').addEventListener('click', newProject);
  $('btnSave').addEventListener('click', () => saveFile(true));
  $('btnBuild').addEventListener('click', build);
  $('btnRun').addEventListener('click', run);
  $('btnClear').addEventListener('click', () => { $('log').textContent = ''; S.problems = []; renderProblems(); });
  $('btnPanel').addEventListener('click', togglePanel);
  $('panelHandle').addEventListener('click', togglePanel);
  $('btnGrant').addEventListener('click', () => K().requestStorage());
  $('btnAuto').addEventListener('click', () => updateComplete(true));
  $('btnTest').addEventListener('click', testBinaries);
  document.addEventListener('pointerdown', (e) => {
    if (COMP.active && !$('complete').contains(e.target)) hideComplete();
  }, true);
  document.querySelectorAll('.tab').forEach((b) => b.addEventListener('click', () => setTab(b.dataset.tab)));

  const ta = ed();
  let raf = 0;
  ta.addEventListener('input', () => {
    markDirty();
    if (!raf) raf = requestAnimationFrame(() => {
      raf = 0;
      refreshEditor();
      updateStatus();
      renderTabs();
      updateComplete();
    });
  });
  ta.addEventListener('scroll', () => { syncScroll(); positionComplete(); });
  ta.addEventListener('keyup', updateStatus);
  ta.addEventListener('click', updateStatus);
  ta.addEventListener('keydown', (e) => {
    if (COMP.active) {
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        e.preventDefault();
        COMP.index = (COMP.index + (e.key === 'ArrowDown' ? 1 : COMP.items.length - 1)) % COMP.items.length;
        renderComplete();
        const on = $('complete').querySelector('.ci.on');
        if (on) on.scrollIntoView({ block: 'nearest' });
        return;
      }
      if (e.key === 'Enter' || e.key === 'Tab') {
        e.preventDefault();
        acceptComplete(COMP.index);
        return;
      }
      if (e.key === 'Escape') {
        e.preventDefault();
        hideComplete();
        return;
      }
    }
    if (e.key === 'Tab') {
      e.preventDefault();
      insertText('    ');
    } else if (e.key === 'Enter') {
      // auto-indent: salin spasi awal baris sekarang
      const pos = ta.selectionStart;
      const lineStart = ta.value.lastIndexOf('\n', pos - 1) + 1;
      const m = /^[ \t]*/.exec(ta.value.slice(lineStart, pos));
      const indent = m ? m[0] : '';
      const extra = ta.value.slice(0, pos).trimEnd().endsWith('{') ? '    ' : '';
      if (indent || extra) {
        e.preventDefault();
        insertText('\n' + indent + extra);
      }
    }
  });

  // pinch zoom pada area editor
  const wrap = $('editorWrap');
  let pinch = 0;
  const dist = (t) => Math.hypot(t[0].clientX - t[1].clientX, t[0].clientY - t[1].clientY);
  wrap.addEventListener('touchstart', (e) => {
    if (e.touches.length === 2) pinch = dist(e.touches);
  }, { passive: true });
  wrap.addEventListener('touchmove', (e) => {
    if (e.touches.length === 2 && pinch > 0) {
      const d = dist(e.touches);
      if (d > 10) setFont(S.fontSize * (d / pinch));
      pinch = d;
      e.preventDefault();
    }
  }, { passive: false });
  wrap.addEventListener('touchend', () => { pinch = 0; }, { passive: true });

  // panel bawah bisa di-drag naik/turun
  const handle = $('panelHandle');
  let dragY = 0, dragH = 0;
  handle.addEventListener('touchstart', (e) => {
    dragY = e.touches[0].clientY;
    dragH = $('panel').offsetHeight;
    $('panel').classList.remove('closed');
  }, { passive: true });
  handle.addEventListener('touchmove', (e) => {
    const dy = dragY - e.touches[0].clientY;
    const h = Math.max(90, Math.min(window.innerHeight * 0.72, dragH + dy));
    $('panel').style.height = h + 'px';
    e.preventDefault();
  }, { passive: false });

  window.addEventListener('resize', syncScroll);
  document.addEventListener('visibilitychange', () => {
    if (document.hidden && S.dirty) saveFile(false);
  });

  init();
});
