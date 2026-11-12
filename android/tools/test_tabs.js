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
const disk = {
  '/storage/emulated/0/KopiProject/P/src/A.kop': 'class A { }\n',
  '/storage/emulated/0/KopiProject/P/src/B.kop': 'class B {\n  // b\n}\n',
  '/storage/emulated/0/KopiProject/P/src/C.kop': 'class C { }\n',
};
const K = {
  info: () => JSON.stringify({ projects: '/storage/emulated/0/KopiProject', libs: [], sdk: 33, targetSdk: 28, kopModules: 42, storage: true }),
  listDir: () => JSON.stringify([]),
  readFile: (p) => (disk[p] != null ? disk[p] : JSON.stringify({ ok: false, err: true, msg: 'tidak ada' })),
  writeFile: (p, t) => { disk[p] = t; return JSON.stringify({ ok: true }); },
  compile: () => JSON.stringify({ ok: true, kbc: '/storage/emulated/0/KopiProject/P/out/A.kbc', ms: 3 }),
  run: () => JSON.stringify({ ok: true }),
  stop: () => {},
  selfTest: () => JSON.stringify({ libs: [], e2e: { ok: true } }),
  delete: () => JSON.stringify({ ok: true }),
  rename: () => JSON.stringify({ ok: true }),
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
const sandbox = new Function('window', 'document', 'localStorage', 'requestAnimationFrame', 'getComputedStyle', 'navigator', 'location', src + '\n;return {S, openFile, activateTab, closeTab, saveFile, renderTabs, build, run, init};');
const api = sandbox(global.window, global.document, global.localStorage, global.requestAnimationFrame, global.getComputedStyle, { userAgent: 'node' }, { href: 'file:///app/index.html' });

const { S, openFile, activateTab, closeTab, saveFile } = api;

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

console.log('--- selesai ---');
