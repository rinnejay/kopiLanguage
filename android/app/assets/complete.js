/* complete.js — mesin saran otomatis (autocomplete) untuk bahasa Kopi.
 *
 * Murni fungsi (tanpa DOM) supaya bisa diuji dengan node:
 *   const C = require('./complete.js');
 *   C.suggest('System.out.pr', 13, []);
 *
 * suggest(text, pos, extraWords) → { start, prefix, recv, items[] } | null
 * item = { label, kind, insert, caret }
 */
'use strict';

(function (root, factory) {
  var api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (typeof window !== 'undefined') window.KopiComplete = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {

  const KEYWORDS = [
    'class', 'public', 'private', 'static', 'final', 'int', 'boolean', 'void',
    'String', 'long', 'double', 'float', 'uint8', 'uint16', 'uint32', 'uint64',
    'byte', 'short', 'int8', 'int16', 'if', 'else', 'while', 'do', 'for',
    'return', 'true', 'false', 'break', 'continue', 'import', 'fn', 'json',
    'switch', 'case', 'default', 'malloc', 'new', 'null'
  ];

  const TYPES = ['int', 'boolean', 'void', 'String', 'long', 'double', 'float',
    'uint8', 'uint16', 'uint32', 'uint64', 'byte', 'short', 'int8', 'int16'];

  const MODULES = ['Str', 'Net', 'Http', 'Json', 'File', 'Bytes', 'Math'];

  /* anggota per receiver. 1 = fungsi (disisipi "()"), 0 = field */
  const MEMBERS = {
    'System': { out: 0 },
    'System.out': { println: 1, print: 1 },

    'Str': {
      fromInt: 1, fromLong: 1, fromDouble: 1, boolText: 1, cat: 1, copy: 1,
      slice: 1, sub: 1, substring: 1, trim: 1, upper: 1, lower: 1, repeat: 1,
      padLeft: 1, splitAt: 1, splitCount: 1, equals: 1, equalsIgnoreCase: 1,
      toInt: 1, indexOf: 1, isEmpty: 1, replaceSeq: 1, utf8Count: 1, countByte: 1
    },

    'String': {
      length: 0, trim: 1, upper: 1, lower: 1, substring: 1, sub: 1, charAt: 1,
      indexOf: 1, contains: 1, startsWith: 1, endsWith: 1, equals: 1,
      equalsIgnoreCase: 1, isEmpty: 1, repeat: 1, toInt: 1, replaceSeq: 1,
      padLeft: 1, countByte: 1, utf8Count: 1, slice: 1, splitAt: 1, splitCount: 1
    },

    'Net': {
      socket: 1, bind: 1, listen: 1, accept: 1, accept4: 1, connect: 1,
      sendb: 1, recv: 1, recv1: 1, recvb: 1, recvfrom: 1, sendto: 1, close: 1,
      shutdown: 1, setsockopt: 1, settimeout: 1, setnonblock: 1, pollfd: 1,
      sleep: 1, time: 1, mono: 1, exit: 1, mkstr: 1, strbytes: 1, bytes: 1,
      openr: 1, openb: 1, rdb: 1, wrb: 1, seek: 1, ttyRaw: 1, memMark: 1,
      memReset: 1, argc: 1, argv: 1, getenv: 1, key: 1, cat: 1, sequal: 1,
      sne: 1, localport: 1
    },

    'Http': {
      getFull: 1, getWith: 1, getInto: 1, bodyOf: 1, payload: 1, statusOf: 1,
      hasHeader: 1, headerValue: 1, basic: 1, follow: 1, formPost: 1,
      jsonPost: 1, exchange: 1
    },

    'Json': {
      parse: 1, get: 1, at: 1, put: 1, add: 1, del: 1, count: 1, asInt: 1,
      asDouble: 1, str: 1, boolV: 1, isNull: 1, line: 1, num: 1, write: 1,
      print: 1, obj4: 1
    },

    'File': {
      openRead: 1, create: 1, writeAll: 1, writeText: 1, readText: 1,
      readInto: 1, size: 1, close: 1, seekSet: 1, mkdir: 1, remove: 1,
      renameTo: 1, kop: 1
    },

    'Bytes': { cat: 1, cmp: 1, indexOf: 1 },

    'Math': {
      sqrt: 1, pow: 1, powMod: 1, gcd: 1, lcm: 1, factorial: 1, fib: 1,
      isPrime: 1, min: 1, max: 1, abs: 1
    },

    /* rantai yang sering dipakai langsung */
    'System.out.println': {},
    'System.out.print': {}
  };

  const FIELD_NAMES = { out: 1, length: 1 };

  /* ── deteksi posisi di dalam kode (bukan komentar/string) ── */
  function isCode(text, pos) {
    let i = 0;
    const n = Math.min(pos, text.length);
    while (i < n) {
      const c = text[i];
      if (c === '/' && text[i + 1] === '/') {
        const j = text.indexOf('\n', i + 2);
        if (j < 0 || j >= n) return false;
        i = j;
        continue;
      }
      if (c === '/' && text[i + 1] === '*') {
        const j = text.indexOf('*/', i + 2);
        if (j < 0 || j + 2 > n) return false;
        i = j + 2;
        continue;
      }
      if (c === '"' || c === "'") {
        let j = i + 1, closed = false;
        while (j < n) {
          if (text[j] === '\\') { j += 2; continue; }
          if (text[j] === c) { j++; closed = true; break; }
          j++;
        }
        if (!closed) return false;
        i = j;
        continue;
      }
      if (c === '`') {
        let j = i + 1, closed = false;
        while (j < n) {
          if (text[j] === '\\') { j += 2; continue; }
          if (text[j] === '`') { j++; closed = true; break; }
          j++;
        }
        if (!closed) return false;
        i = j;
        continue;
      }
      i++;
    }
    return true;
  }

  function isWordChar(c) {
    return /[A-Za-z0-9_$#]/.test(c);
  }

  function contextAt(text, pos) {
    if (pos < 0 || pos > text.length) return null;
    if (!isCode(text, pos)) return null;

    let start = pos;
    while (start > 0 && isWordChar(text[start - 1])) start--;
    const prefix = text.slice(start, pos);

    let recv = '';
    if (start > 0 && text[start - 1] === '.') {
      let e = start - 1, s = e;
      while (s > 0 && /[A-Za-z0-9_$#.]/.test(text[s - 1])) s--;
      recv = text.slice(s, e);
    }

    if (!prefix && !recv) return null;
    return { start, prefix, recv };
  }

  function collectWords(text) {
    const seen = new Set();
    const re = /[A-Za-z_$#][A-Za-z0-9_$#]*/g;
    let m;
    while ((m = re.exec(text)) !== null) {
      if (m[0].length >= 2) seen.add(m[0]);
    }
    return Array.from(seen);
  }

  const KIND_RANK = { keyword: 0, type: 1, module: 2, method: 3, field: 4, word: 5, snippet: 6 };

  function item(label, kind, insert, caret) {
    return {
      label: label,
      kind: kind,
      insert: insert == null ? label : insert,
      caret: caret == null ? null : caret
    };
  }

  function methodItem(name) {
    return item(name, 'method', name + '()', name.length + 1);
  }

  function cmp(a, b) {
    const ra = KIND_RANK[a.kind] == null ? 9 : KIND_RANK[a.kind];
    const rb = KIND_RANK[b.kind] == null ? 9 : KIND_RANK[b.kind];
    if (ra !== rb) return ra - rb;
    if (a.label.length !== b.label.length) return a.label.length - b.label.length;
    return a.label < b.label ? -1 : a.label > b.label ? 1 : 0;
  }

  /**
   * Saran untuk posisi kursor. `extra` = kata tambahan dari berkas aktif.
   */
  function suggest(text, pos, extra, force) {
    let ctx = contextAt(text, pos);
    if (!ctx) {
      if (isCode(text, pos)) {
        const head = text.slice(Math.max(0, pos - 12), pos);
        if (force || /\b(import|new)\s+$/.test(head)) ctx = { start: pos, prefix: '', recv: '' };
      }
      if (!ctx) return null;
    }

    const prefix = ctx.prefix;
    const low = prefix.toLowerCase();
    const out = [];
    const seen = new Set();

    const push = (it) => {
      if (!it || seen.has(it.label)) return;
      seen.add(it.label);
      out.push(it);
    };

    if (ctx.recv) {
      const tbl = MEMBERS[ctx.recv] || MEMBERS['String'];
      if (tbl) {
        for (const name of Object.keys(tbl)) {
          if (low && name.toLowerCase().indexOf(low) !== 0) continue;
          push(tbl[name] ? methodItem(name) : item(name, 'field'));
        }
      }
      // receiver tak dikenal (mis. variabel String/array): tawarkan
      // anggota String yang umum sebagai tebakan terbaik.
      if (!MEMBERS[ctx.recv]) {
        for (const name of ['length', 'trim', 'upper', 'lower', 'substring', 'sub',
          'charAt', 'indexOf', 'contains', 'startsWith', 'endsWith', 'equals',
          'equalsIgnoreCase', 'isEmpty', 'repeat', 'toInt', 'splitAt', 'slice']) {
          if (low && name.toLowerCase().indexOf(low) !== 0) continue;
          push(FIELD_NAMES[name] ? item(name, 'field') : methodItem(name));
        }
      }
      return out.length
        ? { start: ctx.start, prefix: prefix, recv: ctx.recv, items: out.sort(cmp).slice(0, 8) }
        : null;
    }

    // setelah "import " → modul; setelah "new " → tipe
    const before = text.slice(Math.max(0, ctx.start - 12), ctx.start);
    if (/\bimport\s+$/.test(before)) {
      for (const m of MODULES) {
        if (!low || m.toLowerCase().indexOf(low) === 0) push(item(m, 'module'));
      }
    } else if (/\bnew\s+$/.test(before)) {
      for (const t of TYPES) {
        if (!low || t.toLowerCase().indexOf(low) === 0) push(item(t, 'type'));
      }
    } else {
      for (const k of KEYWORDS) {
        if (!low || k.toLowerCase().indexOf(low) === 0) push(item(k, 'keyword'));
      }
      for (const m of MODULES) {
        if (!low || m.toLowerCase().indexOf(low) === 0) push(item(m, 'module'));
      }
      if ('system.out.println'.indexOf(low) === 0 && low.length >= 2) {
        push(item('System.out.println', 'snippet', 'System.out.println()', 'System.out.println('.length));
        push(item('System.out.print', 'snippet', 'System.out.print()', 'System.out.print('.length));
      }
      if (extra) {
        for (const w of extra) {
          if (low && w.toLowerCase().indexOf(low) !== 0) continue;
          if (KEYWORDS.indexOf(w) >= 0) continue;
          push(item(w, 'word'));
        }
      }
    }

    if (!out.length) return null;
    out.sort(cmp);
    return { start: ctx.start, prefix: prefix, recv: ctx.recv, items: out.slice(0, 8) };
  }

  return {
    suggest: suggest,
    contextAt: contextAt,
    isCode: isCode,
    collectWords: collectWords,
    KEYWORDS: KEYWORDS,
    MODULES: MODULES,
    MEMBERS: MEMBERS
  };
});
