/* highlight.js — tokenizer Kopi (dipakai IDE + uji node) */
const KW = new Set(('class public private static final int boolean void String long double float ' +
  'uint8 uint16 uint32 uint64 byte short int8 int16 if else while do for return true false ' +
  'break continue import fn json switch case default malloc new null').split(' '));

function esc(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function highlight(src) {
  let out = '', i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i];
    if (c === '/' && src[i + 1] === '/') {
      let j = src.indexOf('\n', i); if (j < 0) j = n;
      out += '<span class="cm">' + esc(src.slice(i, j)) + '</span>'; i = j; continue;
    }
    if (c === '/' && src[i + 1] === '*') {
      let j = src.indexOf('*/', i + 2); j = j < 0 ? n : j + 2;
      out += '<span class="cm">' + esc(src.slice(i, j)) + '</span>'; i = j; continue;
    }
    if (c === '"' || c === "'") {
      let j = i + 1;
      while (j < n) {
        if (src[j] === '\\') { j += 2; continue; }
        if (src[j] === c) { j++; break; }
        if (src[j] === '\n') break;
        j++;
      }
      out += '<span class="st">' + esc(src.slice(i, j)) + '</span>'; i = j; continue;
    }
    if (c === '`') {
      let j = i + 1;
      while (j < n) {
        if (src[j] === '\\') { j += 2; continue; }
        if (src[j] === '`') { j++; break; }
        j++;
      }
      out += '<span class="tpl">' + esc(src.slice(i, j)) + '</span>'; i = j; continue;
    }
    if (c >= '0' && c <= '9') {
      let j = i + 1;
      while (j < n && /[0-9a-fA-FxXbBoO._]/.test(src[j])) j++;
      out += '<span class="nm">' + esc(src.slice(i, j)) + '</span>'; i = j; continue;
    }
    if (/[A-Za-z_$#]/.test(c)) {
      let j = i + 1;
      while (j < n && /[A-Za-z0-9_$#]/.test(src[j])) j++;
      const w = src.slice(i, j);
      let cls = 'id';
      if (KW.has(w)) cls = 'kw';
      else if (/^[A-Z]/.test(w)) cls = 'cls';
      else if (src[j] === '(') cls = 'fn';
      out += '<span class="' + cls + '">' + esc(w) + '</span>'; i = j; continue;
    }
    if ('+-*/%=<>!&|^~?:'.indexOf(c) >= 0) {
      let j = i + 1;
      while (j < n && '+-*/%=<>!&|^~?:'.indexOf(src[j]) >= 0) j++;
      out += '<span class="op">' + esc(src.slice(i, j)) + '</span>'; i = j; continue;
    }
    out += esc(c);
    i++;
  }
  return out;
}


if (typeof module !== 'undefined') module.exports = { highlight: highlight, KW: KW };
