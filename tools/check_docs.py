#!/usr/bin/env python3
"""check_docs.py — verifikasi situs HTML dokumentasi (GitHub Pages).

Cek: semua halaman .md punya .html, link internal ada, tidak ada sisa
penanda Markdown di luar blok kode, jumlah blok kode sesuai, dan
`.nojekyll` ada.
"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import md2html  # noqa: E402

ROOT = md2html.ROOT
DOCS = md2html.DOCS
bad = []


def check_pages():
    for src, out, _title, _label in md2html.PAGES:
        mp = os.path.join(DOCS, src)
        hp = os.path.join(DOCS, out)
        if not os.path.isfile(hp):
            bad.append("hilang: docs/%s" % out)
            continue
        md = open(mp, encoding="utf-8").read()
        h = open(hp, encoding="utf-8").read()
        if "<title>" not in h or "<main" not in h:
            bad.append("struktur: docs/%s" % out)
        if 'href="style.css"' not in h:
            bad.append("css: docs/%s" % out)
        fences = md.count("```")
        if fences % 2:
            bad.append("fence ganjil: docs/%s" % src)
        if h.count("<pre>") != fences // 2:
            bad.append("blok kode beda: docs/%s (%d vs %d)"
                       % (src, h.count("<pre>"), fences // 2))
        plain = re.sub(r"<pre>.*?</pre>", "", h, flags=re.S)
        plain = re.sub(r"<code>.*?</code>", "", plain, flags=re.S)
        for marker in ("**", "```"):
            if marker in plain:
                bad.append("sisa markdown %r: docs/%s" % (marker, out))
        # deteksi HTML kedaluwarsa (md berubah tapi belum di-generate)
        body, toc = md2html.convert(md, _title)
        if toc:
            k = body.find("</h1>")
            if k >= 0:
                body = body[:k + 5] + "\n" + toc + body[k + 5:]
        expect = md2html.page(out, _title, body, "", src)
        if h != expect:
            bad.append("kedaluwarsa: docs/%s — jalankan 'make docs'" % out)


def check_links():
    for hp in glob.glob(os.path.join(DOCS, "*.html")):
        s = open(hp, encoding="utf-8").read()
        for href in re.findall(r'href="([^"]+)"', s):
            if href.startswith(("http://", "https://", "#", "mailto:")):
                continue
            target = os.path.join(DOCS, href.split("#")[0])
            if href.split("#")[0] and not os.path.exists(target):
                bad.append("link mati: %s → %s"
                           % (os.path.basename(hp), href))


def main():
    if not os.path.isfile(os.path.join(DOCS, ".nojekyll")):
        bad.append("docs/.nojekyll tidak ada (jalankan tools/md2html.py)")
    check_pages()
    check_links()
    if bad:
        for b in bad:
            print("GAGAL:", b)
        print("ringkas: %d masalah" % len(bad))
        return 1
    print("docs HTML OK — %d halaman, %d file"
          % (len(md2html.PAGES), len(glob.glob(os.path.join(DOCS, "*.html")))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
