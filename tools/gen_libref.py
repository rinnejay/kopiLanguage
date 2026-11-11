#!/usr/bin/env python3
"""gen_libref.py — hasilkan referensi API dari sumber lib/*.kop.

Output:
  docs/api.md          indeks semua modul + pola penerapan
  docs/api-core.md     referensi lib/core
  docs/api-net.md      referensi lib/net
  docs/api-crypto.md   referensi lib/crypto

Signature diekstrak langsung dari source agar tidak ada fungsi yang
terlewat. Deskripsi memakai komentar sumber, metadata libmeta.py, lalu
kamus nama umum.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from libmeta import META, COMMON, EXTRA  # noqa: E402

CATS = [
    ("core", "Core", "Tipe dasar, teks, matematika, koleksi, file, waktu"),
    ("net", "Net", "TCP/UDP, HTTP, WebSocket, URL, auth, DNS, TLS"),
    ("crypto", "Crypto", "Hash, PBKDF2, cipher, X25519, AEAD, DER"),
]


def clean_comment(buf):
    txt = " ".join(buf)
    txt = re.sub(r"/\*+", " ", txt)
    txt = re.sub(r"\*+/", " ", txt)
    txt = txt.replace("*", " ")
    txt = re.sub(r"\s+", " ", txt).strip()
    return txt


def is_banner(txt):
    """Komentar pembatas bagian source: ── nama bagian ────."""
    return bool(re.match(r"^[─━]{2,}.*[─━]{2,}$", txt))


def parse_file(path):
    src = open(path, encoding="utf-8").read()
    cls = re.search(r"class\s+(\w+)", src)
    cls = cls.group(1) if cls else os.path.basename(path)[:-4]
    lines = src.split("\n")
    funcs, consts = [], []
    pending = []
    i = 0
    while i < len(lines):
        raw = lines[i]
        ln = raw.strip()
        if ln.startswith("/*"):
            buf = [ln]
            while "*/" not in buf[-1] and i + 1 < len(lines):
                i += 1
                buf.append(lines[i].strip())
            pending = [clean_comment(buf)]
            i += 1
            continue
        if ln.startswith("//"):
            pending.append(ln[2:].strip())
            i += 1
            continue
        m = re.match(r"^(public\s+)?static\s+", ln)
        if m and "(" in ln:
            sig = ln
            j = i
            while "{" not in sig and ";" not in sig and j + 1 < len(lines):
                j += 1
                sig += " " + lines[j].strip()
            sig = sig.split("{")[0].split(";")[0].strip()
            sig = re.sub(r"^public\s+", "", sig)
            sig = re.sub(r"\s+", " ", sig)
            fname = re.search(r"(\w+)\s*\(", sig)
            fname = fname.group(1) if fname else ""
            is_const = bool(re.fullmatch(r"[A-Z][A-Z0-9_]*", fname)) and "()" in sig.replace(" ", "")
            doc = clean_comment(pending)
            if is_banner(doc):
                doc = ""
            pending = []
            item = {"sig": sig, "name": fname, "doc": doc}
            (consts if is_const else funcs).append(item)
            i = j + 1
            continue
        if ln and not ln.startswith("*") and not ln.startswith("}"):
            pending = []
        i += 1
    return cls, funcs, consts


def desc_for(cls, fn):
    d = META.get(cls, {}).get("descs", {}).get(fn["name"], "")
    if not d:
        d = EXTRA.get(cls, {}).get("descs", {}).get(fn["name"], "")
    if d:
        return d
    if fn["doc"]:
        d = fn["doc"]
        return d if len(d) <= 180 else d[:177].rstrip() + "…"
    d = COMMON.get(fn["name"], "")
    return d if d else "—"


def cell(s):
    return s.replace("|", "/").replace("\n", " ").strip()


def module_md(cls, path, funcs, consts):
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    meta = META.get(cls, {})
    out = []
    out.append("## %s" % cls)
    out.append("")
    out.append("`%s` · %d fungsi" % (rel, len(funcs)))
    out.append("")
    purpose = meta.get("purpose") or "Modul pustaka Kopi."
    out.append(purpose)
    out.append("")
    if meta.get("example"):
        out.append("```java")
        out.append(meta["example"])
        out.append("```")
        out.append("")
    if meta.get("notes"):
        out.append("**Catatan**")
        out.append("")
        for n in meta["notes"]:
            out.append("- %s" % n)
        out.append("")
    if funcs:
        out.append("| Fungsi | Keterangan |")
        out.append("|---|---|")
        for fn in funcs:
            out.append("| `%s` | %s |" % (cell(fn["sig"]), cell(desc_for(cls, fn))))
        out.append("")
    if consts:
        out.append("**Konstanta**")
        out.append("")
        out.append("| Nama | Keterangan |")
        out.append("|---|---|")
        for fn in consts:
            out.append("| `%s` | %s |" % (cell(fn["sig"]), cell(desc_for(cls, fn))))
        out.append("")
    return "\n".join(out)


def build_category(cat, label, tagline, files):
    out = ["# Referensi API — %s" % label, ""]
    out.append(tagline + ". Semua signature di bawah diekstrak langsung dari source "
               "`lib/%s/`; deskripsi berasal dari komentar sumber atau metadata." % cat)
    out.append("")
    out.append("> Impor dengan `import %s;` — nama class sama dengan nama file. "
               "Contoh pada tiap modul adalah pola pakai, bukan program utuh."
               % "{Nama}")
    out.append("")
    for path in files:
        cls, funcs, consts = parse_file(path)
        out.append(module_md(cls, path, funcs, consts))
        out.append("---")
        out.append("")
    return "\n".join(out)


def shorten(text, limit=110):
    """Potong teks pada batas kata tanpa memutus pasangan backtick."""
    if len(text) <= limit:
        return text
    cut = text.rfind(" ", 0, limit - 3)
    if cut <= 0:
        cut = limit - 3
    while text[:cut].count("`") % 2:
        prev = text.rfind("`", 0, cut)
        if prev <= 0:
            cut = 0
            break
        cut = prev
    if cut <= 0:
        return text
    return text[:cut].rstrip() + "…"


def build_index(files_by_cat):
    out = ["# Referensi pustaka", ""]
    out.append("Ringkasan semua modul di `lib/`. Setiap modul adalah class Kopi "
               "dengan method static; impor memakai nama class.")
    out.append("")
    out.append("| Kelompok | Folder | Modul |")
    out.append("|---|---|---|")
    for cat, label, tagline in CATS:
        names = ", ".join("`%s`" % c for c in files_by_cat[cat][1])
        out.append("| **%s** | `lib/%s/` | %s |" % (label, cat, names))
    out.append("")
    out.append("## Daftar modul")
    out.append("")
    for cat, label, tagline in CATS:
        page, rows = files_by_cat[cat][0], files_by_cat[cat][2]
        out.append("### %s — [buka referensi](%s)" % (label, page))
        out.append("")
        out.append("| Modul | File | Fungsi | Ringkasan |")
        out.append("|---|---|---|---|")
        for cls, rel, n in rows:
            purpose = META.get(cls, {}).get("purpose", "—")
            short = shorten(purpose)
            out.append("| [`%s`](%s#%s) | `%s` | %d | %s |"
                       % (cls, page, cls.lower(), rel, n, cell(short)))
        out.append("")
    out.append("## Cara menerapkan")
    out.append("")
    out.append("| Kebutuhan | Modul yang dipakai | Pola singkat |")
    out.append("|---|---|---|")
    out.append("| CLI & pengolah teks | `Str`, `File`, `Path`, `Arrays` | baca file → `Str.trim`/`splitAt` → cetak |")
    out.append("| Matematika & statistik | `Math`, `Digits`, `Stats`, `Matrix` | fungsi murni, hasil int/long |")
    out.append("| Data terstruktur | `Json`, `Map`, `MapStr`, `Set`, `Deque` | `Json.parse` / map untuk lookup |")
    out.append("| Server HTTP | `Tcp`, `HttpServer`, `Static`, `Mime`, `Url` | accept → `readRequest` → route → response |")
    out.append("| Banyak klien | `Net.pollfd` + `HttpServer` | event loop non-blocking (`ServerEvent`) |")
    out.append("| WebSocket | `WebSocket`, `Hash`, `Cipher` | handshake → decode frame → balas |")
    out.append("| Klien HTTP | `Http`, `Url`, `Resolv` | `getFull`/`follow` → `payload` |")
    out.append("| Keamanan | `Pbkdf2`, `Jwt`, `Auth`, `Session`, `Rand` | hash password, token, cookie sesi |")
    out.append("| TLS | `Tls`, `X25519`, `Aead`, `Der` | handshake pin SPKI → `writeApp`/`readApp` |")
    out.append("| Game realtime | `Udp`, `Math`, `Map` | loop tick + snapshot (lihat `examples/game`) |")
    out.append("")
    out.append("**Penting:** modul jaringan memakai socket blocking kecuali Anda "
               "memasang `Net.setnonblock`/`Net.settimeout`; lihat "
               "[04 — Server](04-server.html) untuk pola event loop dan batas.")
    out.append("")
    out.append("## Builtin `Net.*`")
    out.append("")
    out.append("Fungsi bawaan bahasa (tanpa impor) — pembungkus syscall native.")
    out.append("")
    out.append("| Fungsi | Keterangan |")
    out.append("|---|---|")
    for name, desc in NETBI:
        out.append("| `Net.%s` | %s |" % (name, desc))
    out.append("")
    out.append("> `System.out.println/print` juga builtin; `Net.bytes(\"teks\")`")
    out.append("> mengubah string literal menjadi `int[]` byte saat kompilasi.")
    out.append("")
    return "\n".join(out)


NETBI = [
    ("socket(fd_type, proto)", "buat socket (TCP/UDP); deskriptor"),
    ("connect(fd, ip, port)", "hubungkan ke alamat (oktet di int)"),
    ("rdb(fd, buf)", "baca byte mentah ke buffer; jumlah terbaca"),
    ("wrb(fd, buf)", "tulis byte mentah; jumlah tertulis"),
    ("close(fd)", "tutup deskriptor"),
    ("bytes(\"teks\")", "literal string → array byte (saat kompilasi)"),
    ("openr(path)", "buka file hanya-baca"),
    ("recvb(fd, buf)", "baca dari koneksi (blocking/timeout)"),
    ("sendb(fd, buf)", "kirim seluruh buffer ke koneksi"),
    ("bind(fd, ip, port)", "ikat alamat/port"),
    ("sendto(fd, ip, port, buf)", "kirim datagram UDP"),
    ("recvfrom(fd, buf, from)", "terima datagram; from = int[2] (ip, port)"),
    ("shutdown(fd, how)", "matikan satu/dua arah koneksi"),
    ("listen(fd, backlog)", "jadikan socket server"),
    ("accept4(fd)", "terima koneksi baru (syscall accept4, Android aman)"),
    ("setsockopt(fd, level, opt, val)", "set opsi socket (SO_REUSEADDR, dll.)"),
    ("memMark()", "tandai batas arena memori saat ini"),
    ("memReset(mark)", "kembali ke tanda; byte > mark bebas dipakai lagi"),
    ("argc()", "jumlah argumen program"),
    ("argv(i)", "argumen ke-i sebagai byte"),
    ("getenv(name)", "nilai variabel lingkungan (byte kosong bila tidak ada)"),
    ("recv1(fd, buf)", "satu panggilan read saja (tanpa loop)"),
    ("setnonblock(fd)", "socket non-blocking"),
    ("key()", "byte tombol stdin (mode raw)"),
    ("ttyRaw(on)", "aktif/matikan mode raw terminal"),
    ("localport(fd)", "port lokal socket terikat"),
    ("settimeout(fd, ms)", "timeout operasi socket dalam ms"),
    ("pollfd(fds, revents, ms)", "poll(2); revents[i] 0/1 siap baca/2 error"),
    ("time()", "epoch detik (32-bit)"),
    ("mono()", "waktu monotonic"),
    ("sleep(ms)", "tidur dalam milidetik"),
    ("exit(code)", "keluar program (buffer stdio tidak di-flush)"),
    ("cat(a, b)", "gabung dua array byte"),
    ("sequal(a, b)", "true bila isi dua array sama"),
    ("sne(a, b)", "true bila isi berbeda"),
]


def main():
    files_by_cat = {}
    for cat, label, tagline in CATS:
        files = sorted(glob.glob(os.path.join(ROOT, "lib", cat, "*.kop")))
        rows = []
        names = []
        for p in files:
            cls, funcs, _consts = parse_file(p)
            rel = os.path.relpath(p, ROOT).replace("\\", "/")
            rows.append((cls, rel, len(funcs)))
            names.append(cls)
        page = "api-%s.md" % cat
        files_by_cat[cat] = (page, names, rows)
        md = build_category(cat, label, tagline, files)
        open(os.path.join(DOCS, page), "w", encoding="utf-8").write(md)
        print("libref: docs/%s (%d modul, %d fungsi)"
              % (page, len(rows), sum(r[2] for r in rows)))

    idx = build_index(files_by_cat)
    open(os.path.join(DOCS, "api.md"), "w", encoding="utf-8").write(idx)
    print("libref: docs/api.md")


if __name__ == "__main__":
    main()
