/* Uji logika tab berkas app.js dengan DOM tiruan (tanpa browser). */
'use strict';
const fs = require('fs');
const path = require('path');

const ASSETS = path.join(__dirname, '..', 'app', 'assets');

/* ── DOM tiruan ── */
function El(tag) {
  const e = {
    tagName: (tag || 'div').toUpperCase(),
    children: [],
    style: {},
    dataset: {},
    _cls: new Set(),
    _text: '',
    value: '',
    selectionStart: 0,
    selectionEnd: 0,
    scrollTop: 0,
    scrollLeft: 0,
    offsetWidth: 200,
    offsetHeight: 40,
    offsetLeft: 0,
    offsetTop: 0,
    clientWidth: 400,
    clientHeight: 300,
    scrollHeight: 320,
    className: '',
    listeners: {},
    classList: {
      add: (c) => e._cls.add(c),
      remove: (c) => e._cls.delete(c),
      contains: (c) => e._cls.has(c),
      toggle: (c, f) => (f === undefined ? (e._cls.has(c) ? e._cls.delete(c) : e._cls.add(c)) : (f ? e._cls.add(c) : e._cls.delete(c))),
    },
    addEventListener: (t, f) => { (e.listeners[t] = e.listeners[t] || []).push(f); },
    removeEventListener: () => {},
    appendChild: (c) => { e.children.push(c); c.parentNode = e; return c; },
    append: (...cs) => { for (const c of cs) e.appendChild(c); },
    remove: () => {},
    setAttribute: () => {},
    getAttribute: () => null,
    querySelector: () => null,
    querySelectorAll: () => [],
    contains: () => false,
    focus: () => {},
    scrollIntoView: () => {},
    getComputedStyle: null,
  };
  Object.defineProperty(e, 'textContent', {
    get: () => e._text,
    set: (v) => { e._text = String(v); e.children = []; },
  });
  Object.defineProperty(e, 'innerHTML', {
    get: () => e._html || '',
    set: (v) => { e._html = String(v); e.children = []; },
  });
  return e;
}

const byId = new Map();
function el(id) {
  if (!byId.has(id)) byId.set(id, El('div'));
  return byId.get(id);
}
const document = {
  getElementById: el,
  createElement: (t) => El(t),
  body: El('body'),
  addEventListener: (t, f) => { (document._ls = document._ls || {})[t] = f; },
  querySelectorAll: () => [],
  querySelector: () => null,
  documentElement: El('html'),
};
const store = new Map();
const localStorage = {
  getItem: (k) => (store.has(k) ? store.get(k) : null),
  setItem: (k, v) => store.set(k, String(v)),
  removeItem: (k) => store.delete(k),
};
const rafQueue = [];
const requestAnimationFrame = (f) => { rafQueue.push(f); return rafQueue.length; };
const getComputedStyle = () => ({ fontFamily: 'mono', fontSize: '13px', fontWeight: '400', fontStyle: 'normal', lineHeight: '19px', letterSpacing: '0px', tabSize: '4', padding: '8px' });

/* berkas maya */
const ROOT = '/storage/emulated/0/KopiProject';
const PROJ = ROOT + '/P';
const SRC = PROJ + '/src';
const disk = {
  [SRC + '/A.kop']: 'class A { }\n',
  [SRC + '/B.kop']: 'class B {\n  // b\n}\n',
  [SRC + '/C.kop']: 'class C { }\n',
};
const dirs = new Set([ROOT, PROJ, SRC]);

function exists(p) { return disk[p] != null || dirs.has(p); }
function isDir(p) { return dirs.has(p); }
function keysUnder(p) {
  return Object.keys(disk).filter((k) => k === p || k.indexOf(p + '/') === 0);
}
function rmKey(p) {
  for (const k of keysUnder(p)) delete disk[k];
  for (const d of Array.from(dirs)) if (d === p || d.indexOf(p + '/') === 0) dirs.delete(d);
}
function unique(p) {
  if (!exists(p)) return p;
  const dot = p.lastIndexOf('.');
  const slash = p.lastIndexOf('/');
  const base = dot > slash ? p.slice(0, dot) : p;
  const ext = dot > slash ? p.slice(dot) : '';
  for (let i = 2; i < 100; i++) if (!exists(base + '-' + i + ext)) return base + '-' + i + ext;
  return p + '-' + Date.now();
}
const K = {
  info: () => JSON.stringify({ projects: '/storage/emulated/0/KopiProject', libs: [], sdk: 33, targetSdk: 28, kopModules: 42, storage: true }),
  listDir: () => JSON.stringify([]),
  readFile: (p) => (disk[p] != null ? disk[p] : JSON.stringify({ ok: false, err: true, msg: 'tidak ada' })),
  writeFile: (p, t) => { disk[p] = t; return JSON.stringify({ ok: true }); },
  listDir: (p) => JSON.stringify([]),
  stat: (p) => {
    if (!exists(p)) return JSON.stringify({ ok: false, err: true, msg: 'tidak ada' });
    const d = isDir(p) ? 1 : 0;
    return JSON.stringify({ n: p.slice(p.lastIndexOf('/') + 1), p: p, d: d,
      s: d ? 0 : disk[p].length, mtime: 1700000000000, r: true, w: true, x: false, count: 0 });
  },
  createFile: (p) => {
    if (exists(p)) return JSON.stringify({ ok: false, err: true, msg: 'sudah ada' });
    disk[p] = '';
    return JSON.stringify({ ok: true, msg: 'dibuat' });
  },
  mkdirs: (p) => { dirs.add(p); return JSON.stringify({ ok: true, msg: 'ok' }); },
  copyTo: (from, toDir) => {
    const dst = unique(toDir + '/' + from.slice(from.lastIndexOf('/') + 1));
    if (isDir(from)) {
      dirs.add(dst);
      for (const k of keysUnder(from)) {
        const nk = dst + k.slice(from.length);
        if (disk[k] != null) disk[nk] = disk[k];
      }
    } else {
      disk[dst] = disk[from];
    }
    return JSON.stringify({ ok: true, msg: 'disalin', path: dst });
  },
  moveTo: (from, toDir) => {
    const dst = toDir + '/' + from.slice(from.lastIndexOf('/') + 1);
    if (exists(dst)) return JSON.stringify({ ok: false, err: true, msg: 'sudah ada' });
    if (isDir(from)) {
      dirs.add(dst);
      for (const k of keysUnder(from)) {
        const nk = dst + k.slice(from.length);
        if (disk[k] != null) disk[nk] = disk[k];
      }
      rmKey(from);
    } else {
      disk[dst] = disk[from];
      delete disk[from];
    }
    return JSON.stringify({ ok: true, msg: 'dipindah', path: dst });
  },
  delete: (p) => { rmKey(p); return JSON.stringify({ ok: true, msg: 'terhapus' }); },
  rename: (a, b) => {
    if (isDir(a)) {
      dirs.delete(a);
      dirs.add(b);
      for (const k of keysUnder(a)) {
        const nk = b + k.slice(a.length);
        disk[nk] = disk[k];
        delete disk[k];
      }
    } else {
      disk[b] = disk[a];
      delete disk[a];
    }
    return JSON.stringify({ ok: true, msg: 'diubah nama' });
  },
  compile: () => JSON.stringify({ ok: true, kbc: '/storage/emulated/0/KopiProject/P/out/A.kbc', ms: 3 }),
  run: () => JSON.stringify({ ok: true }),
  stop: () => {},
  selfTest: () => JSON.stringify({ libs: [], e2e: { ok: true } }),
  requestStorage: () => {},
  exitApp: () => {},
};

global.window = { Kopi: K, KopiComplete: null,
  addEventListener: () => {}, removeEventListener: () => {}, innerWidth: 400, innerHeight: 800 };
global.document = document;
global.localStorage = localStorage;
global.requestAnimationFrame = requestAnimationFrame;
global.getComputedStyle = getComputedStyle;


/* muat app.js */
const src = fs.readFileSync(path.join(ASSETS, 'highlight.js'), 'utf8') + '\n' +
            fs.readFileSync(path.join(ASSETS, 'app.js'), 'utf8');
const sandbox = new Function('window', 'document', 'localStorage', 'requestAnimationFrame', 'getComputedStyle', 'navigator', 'location', src + '\n;return {S, openFile, activateTab, closeTab, saveFile, renderTabs, build, run, init,' +
  'newFile, newFolder, renameItem, deleteItem, setClip, pasteInto, showDetails,' +
  'currentDir, retargetTabs, dropTabsUnder, insidePath, selectRow, fileMenu};');
const api = sandbox(global.window, global.document, global.localStorage, global.requestAnimationFrame, global.getComputedStyle, { userAgent: 'node' }, { href: 'file:///app/index.html' });

const { S, openFile, activateTab, closeTab, saveFile } = api;
const $ = (id) => document.getElementById(id);

function check(name, cond, extra) {
  console.log((cond ? 'PASS ' : 'FAIL ') + name + (cond ? '' : '  ' + (extra || '')));
  if (!cond) process.exitCode = 1;
}
function ta() { return document.getElementById('editor'); }

/* init seperti app dibuka */
document._ls.DOMContentLoaded();

/* mulai dari kosong */
S.tabs.length = 0;
S.buffers.clear();
S.kbc.clear();
S.openPath = null;

openFile('/storage/emulated/0/KopiProject/P/src/A.kop');
check('buka A → 1 tab', S.tabs.length === 1 && S.openPath.endsWith('A.kop'), JSON.stringify(S.tabs));
check('editor berisi A', ta().value.indexOf('class A') >= 0);

openFile('/storage/emulated/0/KopiProject/P/src/B.kop');
check('buka B → 2 tab', S.tabs.length === 2, JSON.stringify(S.tabs));
check('editor berisi B', ta().value.indexOf('class B') >= 0);

/* edit B lalu pindah tab → buffer tersimpan */
ta().value = 'class B { int x = 1; }\n';
ta().selectionStart = 7;
S.dirty = false;
// simulasi event input
ta().listeners.input[0]();
check('edit menandai dirty', S.dirty === true);
check('buffer B dirty', (S.buffers.get('/storage/emulated/0/KopiProject/P/src/B.kop') || {}).dirty === true);

activateTab('/storage/emulated/0/KopiProject/P/src/A.kop');
check('pindah ke A', S.openPath.endsWith('A.kop'));
check('editor kembali ke A', ta().value.indexOf('class A') >= 0);
check('dirty A bersih', S.dirty === false);
const bBuf = S.buffers.get('/storage/emulated/0/KopiProject/P/src/B.kop');
check('teks B tersimpan di buffer', bBuf.text.indexOf('int x = 1') >= 0, JSON.stringify(bBuf));

activateTab('/storage/emulated/0/KopiProject/P/src/B.kop');
check('kembali ke B membawa editan', ta().value.indexOf('int x = 1') >= 0);
check('dirty B pulih', S.dirty === true);
check('kursor pulih', ta().selectionStart === 7, String(ta().selectionStart));

/* tab state tersimpan */
const saved = JSON.parse(localStorage.getItem('kopi.tabs') || '[]');
check('localStorage menyimpan 2 tab', saved.length === 2, JSON.stringify(saved));

/* tutup tab B → auto-save */
closeTab('/storage/emulated/0/KopiProject/P/src/B.kop');
check('tutup B → 1 tab', S.tabs.length === 1, JSON.stringify(S.tabs));
check('B tersimpan otomatis ke disk', (disk['/storage/emulated/0/KopiProject/P/src/B.kop'] || '').indexOf('int x = 1') >= 0,
  JSON.stringify(disk['/storage/emulated/0/KopiProject/P/src/B.kop']));
check('pindah ke A setelah tutup', S.openPath.endsWith('A.kop'));

/* tab terakhir ditutup → editor kosong */
closeTab('/storage/emulated/0/KopiProject/P/src/A.kop');
check('tutup semua → 0 tab', S.tabs.length === 0);
check('editor dikosongkan', ta().value === '');
check('openPath null', S.openPath === null);

/* buka A, simpan → dirty bersih & tab dirender */
openFile('/storage/emulated/0/KopiProject/P/src/A.kop');
ta().value = 'class A { int y; }\n';
ta().listeners.input[0]();
saveFile(false);
check('save membersihkan dirty', S.dirty === false);
check('save menulis disk', (disk['/storage/emulated/0/KopiProject/P/src/A.kop'] || '').indexOf('int y') >= 0);
check('tab tidak ada penanda dirty', !(S.buffers.get('/storage/emulated/0/KopiProject/P/src/A.kop').dirty));

/* build → kbc per tab */
api.build();
check('build menyimpan kbc per tab', S.kbc.get('/storage/emulated/0/KopiProject/P/src/A.kop') != null,
  String(S.kbc.get('/storage/emulated/0/KopiProject/P/src/A.kop')));

/* ── operasi berkas: tambah, rename, copy/cut/paste, detail, hapus ── */
async function fileOps() {
  function modalOk(value) {
    $('minput').value = value;
    $('mok').onclick();
  }

  /* tambah berkas */
  let p = api.newFile(SRC);
  modalOk('D.kop');
  await p;
  check('berkas baru dibuat', disk[SRC + '/D.kop'] === '', JSON.stringify(Object.keys(disk)));
  check('berkas baru langsung dibuka', S.openPath === SRC + '/D.kop' && S.tabs.indexOf(SRC + '/D.kop') >= 0,
    String(S.openPath));

  /* tambah folder */
  p = api.newFolder(SRC);
  modalOk('Sub');
  await p;
  check('folder baru dibuat', dirs.has(SRC + '/Sub'));
  check('folder baru terpilih', S.selPath === SRC + '/Sub' && S.selDir === true);

  /* berkas di dalam folder, lalu rename folder → tab ikut pindah */
  p = api.newFile(SRC + '/Sub');
  modalOk('F.kop');
  await p;
  check('berkas dalam subfolder terbuka', S.openPath === SRC + '/Sub/F.kop');

  const sub = { n: 'Sub', d: 1, p: SRC + '/Sub' };
  p = api.renameItem(sub);
  modalOk('Sub2');
  await p;
  check('folder di-rename di disk', dirs.has(SRC + '/Sub2') && !dirs.has(SRC + '/Sub'));
  check('tab berkas dalam folder ikut di-rename',
    S.openPath === SRC + '/Sub2/F.kop' && S.tabs.indexOf(SRC + '/Sub2/F.kop') >= 0,
    JSON.stringify(S.tabs));
  check('buffer & kbc ikut di-rename', S.buffers.has(SRC + '/Sub2/F.kop'));

  /* copy → paste */
  api.setClip(SRC + '/D.kop', 'copy', false);
  check('clipboard copy', S.clip && S.clip.mode === 'copy');
  await api.pasteInto(SRC + '/Sub2');
  check('hasil copy ada di folder tujuan', disk[SRC + '/Sub2/D.kop'] === '');
  check('copy tidak memindahkan asal', disk[SRC + '/D.kop'] === '');
  check('clipboard copy tetap ada', S.clip && S.clip.mode === 'copy');

  /* cut → paste: tab & path ikut pindah (dari subfolder ke src) */
  const before = S.openPath;
  api.setClip(before, 'cut', false);
  await api.pasteInto(SRC);
  const moved = SRC + '/F.kop';
  check('cut memindahkan berkas', disk[moved] === '' && disk[before] == null,
    JSON.stringify({ before, moved, keys: Object.keys(disk) }));
  check('tab mengikuti hasil cut', S.openPath === moved && S.tabs.indexOf(moved) >= 0,
    String(S.openPath));
  check('clipboard cut dikosongkan', S.clip === null);

  /* detail berkas */
  api.showDetails(SRC + '/D.kop');
  const detail = $('sdetail').textContent;
  check('detail menampilkan nama/tipe/izin',
    detail.indexOf('D.kop') >= 0 && detail.indexOf('Berkas') >= 0 && detail.indexOf('Izin') >= 0,
    detail.slice(0, 120));
  check('detail tidak tersembunyi', !$('sdetail').classList.contains('hidden'));

  /* detail folder */
  api.showDetails(SRC + '/Sub2');
  check('detail folder', $('sdetail').textContent.indexOf('Folder') >= 0);

  /* hapus folder → tab di dalamnya ikut ditutup */
  openFile(SRC + '/A.kop');
  const openInside = SRC + '/Sub2/D.kop';
  openFile(openInside);
  check('dua tab sebelum hapus folder', S.tabs.length >= 2, JSON.stringify(S.tabs));
  const del = api.deleteItem({ n: 'Sub2', d: 1, p: SRC + '/Sub2' });
  $('mok').onclick();
  await del;
  check('folder terhapus dari disk', !dirs.has(SRC + '/Sub2') && disk[openInside] == null);
  check('tab di dalam folder tertutup',
    S.tabs.indexOf(openInside) < 0 && S.openPath !== openInside, JSON.stringify(S.tabs));
  check('masih ada tab lain', !!S.openPath && S.openPath !== openInside && S.tabs.length >= 2,
    String(S.openPath) + ' ' + JSON.stringify(S.tabs));

  /* nama tidak valid ditolak */
  p = api.newFile(SRC);
  modalOk('../jahat.kop');
  await p;
  check('nama dengan / ditolak', !disk[PROJ + '/jahat.kop'] && disk[SRC + '/../jahat.kop'] == null);

  console.log('--- selesai ---');
}

fileOps().catch((e) => { console.log('FAIL fileOps: ' + e.stack); process.exitCode = 1; });
