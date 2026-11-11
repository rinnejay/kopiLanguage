#!/usr/bin/env python3
"""md2html.py — generator situs dokumentasi Kopi (GitHub Pages).

Tanpa dependensi: hanya pustaka standar Python. Mendukung subset Markdown
yang dipakai docs/:
  heading #..######, paragraf, **bold**, *italic*, `code`, link,
  fenced code (```), tabel pipa, daftar -/1., blockquote >, hr ---.

Hasil: docs/*.html + docs/style.css (dibuat terpisah), .nojekyll.

Pakai: python3 tools/md2html.py
"""
import html
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs")

PAGES = [
    ("README.md", "index.html", "Dokumentasi Kopi", "Beranda"),
    ("01-mulai.md", "01-mulai.html", "01 — Mulai", "Mulai"),
    ("02-bahasa.md", "02-bahasa.html", "02 — Bahasa", "Bahasa"),
    ("03-pustaka.md", "03-pustaka.html", "03 — Pustaka", "Pustaka"),
    ("04-server.md", "04-server.html", "04 — Server", "Server"),
    ("05-developer.md", "05-developer.html", "05 — Developer", "Developer"),
    ("06-batas-roadmap.md", "06-batas-roadmap.html", "06 — Batas & roadmap", "Batas"),
]

GITHUB = "https://github.com/rinnejay/kopiLanguage"


def slug(text, seen):
    t = re.sub(r"`([^`]*)`", r"\1", text)
    t = re.sub(r"<[^>]+>", "", t)
    t = t.lower()
    t = re.sub(r"[^a-z0-9]+", "-", t).strip("-")
    if not t:
        t = "bagian"
    base = t
    n = 2
    while t in seen:
        t = "%s-%d" % (base, n)
        n += 1
    seen.add(t)
    return t


def esc(s):
    return html.escape(s, quote=False)


def rewrite_href(href):
    # docs/README.md → index.html ; *.md → *.html ; pertahankan fragmen
    frag = ""
    if "#" in href:
        href, frag = href.split("#", 1)
        frag = "#" + frag
    if href.endswith("README.md") and ("/" not in href or href.startswith("docs/")):
        href = "index.html"
    elif href.endswith(".md"):
        href = href[:-3] + ".html"
    return href + frag


def inline(s):
    codes = []

    def keep_code(m):
        codes.append(m.group(1))
        return "\x00C%d\x00" % (len(codes) - 1)

    s = re.sub(r"`([^`]*)`", keep_code, s)
    s = esc(s)
    s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)",
               lambda m: '<a href="%s">%s</a>'
               % (html.escape(rewrite_href(m.group(2)), quote=True), m.group(1)), s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"(?<!\*)\*([^*\n]+)\*(?!\*)", r"<em>\1</em>", s)

    def restore(m):
        raw = codes[int(m.group(1))]
        return "<code>%s</code>" % esc(raw)

    return re.sub(r"\x00C(\d+)\x00", restore, s)


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    out, cur, code = [], [], False
    for ch in line:
        if ch == "`":
            code = not code
            cur.append(ch)
        elif ch == "|" and not code:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    out.append("".join(cur).strip())
    return out


def is_table_delim(line):
    s = line.strip()
    if "|" not in s:
        return False
    s = s.strip("|").strip()
    if not s:
        return False
    return re.fullmatch(r"[:\-\s|]+", s) is not None and "-" in s


def list_match(line):
    return re.match(r"^(\s*)([-*+]|\d+\.)\s+(.*)$", line)


def render_table(lines, i):
    head = split_row(lines[i])
    align = split_row(lines[i + 1])
    i += 2
    rows = []
    while i < len(lines) and lines[i].strip() and "|" in lines[i]:
        rows.append(split_row(lines[i]))
        i += 1

    def al(n):
        if n < len(align):
            a = align[n]
            if a.startswith(":") and a.endswith(":"):
                return ' style="text-align:center"'
            if a.endswith(":"):
                return ' style="text-align:right"'
            if a.startswith(":"):
                return ' style="text-align:left"'
        return ""

    out = ["<div class=\"table-wrap\"><table>", "<thead><tr>"]
    for n, c in enumerate(head):
        out.append("<th%s>%s</th>" % (al(n), inline(c)))
    out.append("</tr></thead><tbody>")
    for row in rows:
        out.append("<tr>")
        for n in range(len(head)):
            cell = row[n] if n < len(row) else ""
            out.append("<td%s>%s</td>" % (al(n), inline(cell)))
        out.append("</tr>")
    out.append("</tbody></table></div>")
    return "\n".join(out), i


def render_list(lines, i, indent):
    m = list_match(lines[i])
    ordered = bool(re.match(r"\d+\.", m.group(2)))
    tag = "ol" if ordered else "ul"
    out = ["<%s>" % tag]
    while i < len(lines):
        m = list_match(lines[i])
        if not m or len(m.group(1)) != indent:
            break
        if bool(re.match(r"\d+\.", m.group(2))) != ordered:
            break
        content = m.group(3)
        i += 1
        # lanjutan: baris menjorok lebih dalam dan bukan item baru
        sub = []
        while i < len(lines) and lines[i].strip():
            m2 = list_match(lines[i])
            if m2 and len(m2.group(1)) <= indent:
                break
            if m2 and len(m2.group(1)) > indent:
                html_sub, i = render_list(lines, i, len(m2.group(1)))
                sub.append(html_sub)
                continue
            if len(lines[i]) - len(lines[i].lstrip()) > indent:
                content += " " + lines[i].strip()
                i += 1
                continue
            break
        out.append("<li>%s%s</li>" % (inline(content), "".join(sub)))
    out.append("</%s>" % tag)
    return "\n".join(out), i


def convert(md, page_title):
    lines = md.replace("\r\n", "\n").split("\n")
    out = []
    toc = []
    seen = set()
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]
        if not line.strip():
            i += 1
            continue

        # fenced code
        m = re.match(r"^```([A-Za-z0-9_+-]*)\s*$", line)
        if m:
            lang = m.group(1)
            i += 1
            buf = []
            while i < n and not re.match(r"^```\s*$", lines[i]):
                buf.append(lines[i])
                i += 1
            i += 1
            cls = ' class="language-%s"' % lang if lang else ""
            out.append("<pre><code%s>%s</code></pre>" %
                       (cls, esc("\n".join(buf))))
            continue

        # heading
        m = re.match(r"^(#{1,6})\s+(.*)$", line)
        if m:
            lvl = len(m.group(1))
            text = m.group(2).strip()
            sid = slug(text, seen)
            inner = inline(text)
            anchor = '<a class="anchor" href="#%s" aria-label="tautan">#</a>' % sid
            out.append('<h%d id="%s">%s%s</h%d>' % (lvl, sid, inner, anchor, lvl))
            if 2 <= lvl <= 3:
                plain = re.sub(r"`([^`]*)`", r"\1", text)
                plain = re.sub(r"[*_]", "", plain)
                toc.append((lvl, sid, plain))
            i += 1
            continue

        # tabel
        if "|" in line and i + 1 < n and is_table_delim(lines[i + 1]):
            body, i = render_table(lines, i)
            out.append(body)
            continue

        # hr
        if re.fullmatch(r"\s*---+\s*", line):
            out.append("<hr>")
            i += 1
            continue

        # blockquote
        if line.startswith(">"):
            buf = []
            while i < n and lines[i].startswith(">"):
                buf.append(re.sub(r"^>\s?", "", lines[i]))
                i += 1
            out.append("<blockquote>%s</blockquote>" %
                       inline(" ".join(x.strip() for x in buf if x.strip())))
            continue

        # daftar
        m = list_match(line)
        if m:
            body, i = render_list(lines, i, len(m.group(1)))
            out.append(body)
            continue

        # paragraf
        buf = [line.strip()]
        i += 1
        while i < n and lines[i].strip():
            nxt = lines[i]
            if re.match(r"^```", nxt) or re.match(r"^#{1,6}\s", nxt) \
               or re.fullmatch(r"\s*---+\s*", nxt) or nxt.startswith(">") \
               or list_match(nxt):
                break
            if "|" in nxt and i + 1 < n and is_table_delim(lines[i + 1]):
                break
            buf.append(nxt.strip())
            i += 1
        out.append("<p>%s</p>" % inline(" ".join(buf)))

    toc_html = ""
    if toc:
        items = []
        for lvl, sid, text in toc:
            items.append('<li class="toc-h%d"><a href="#%s">%s</a></li>'
                         % (lvl, sid, esc(text)))
        toc_html = ('<nav class="toc"><h2>Isi halaman</h2><ul>%s</ul></nav>'
                    % "".join(items))

    return "\n".join(out), toc_html


def nav(current):
    parts = ['<a class="brand" href="index.html">☕ Kopi</a>']
    for _src, outname, _title, label in PAGES:
        cls = ' class="active"' if outname == current else ""
        parts.append('<a%s href="%s">%s</a>' % (cls, outname, label))
    parts.append('<a class="gh" href="%s">GitHub</a>' % GITHUB)
    return "\n".join(parts)


def page(outname, title, body, toc, srcfile):
    return """<!doctype html>
<!-- dibuat otomatis oleh tools/md2html.py; sunting file .md -->
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%s · Kopi</title>
<meta name="description" content="Dokumentasi bahasa Kopi: tutorial, pustaka, server, dan arsitektur compiler.">
<link rel="stylesheet" href="style.css">
</head>
<body>
<header class="top">
  <div class="wrap">
    <nav class="nav">%s</nav>
  </div>
</header>
<div class="wrap layout">
  <main class="content">
%s
<footer class="foot">
  <span>Dokumentasi Kopi v1.11 · lisensi MIT</span>
  <a href="%s/blob/main/docs/%s">Sunting halaman ini</a>
</footer>
  </main>
</div>
</body>
</html>
""" % (esc(title), nav(outname), body, GITHUB, srcfile)


CSS = """/* style.css — tampilan dokumentasi Kopi (dibuat untuk GitHub Pages) */
:root {
  --bg: #0f1115; --panel: #161a22; --text: #e6e9ef; --muted: #9aa4b2;
  --line: #2a3340; --accent: #e8b45a; --code: #10141b; --link: #8ab4ff;
}
@media (prefers-color-scheme: light) {
  :root {
    --bg: #ffffff; --panel: #f6f7f9; --text: #1c2330; --muted: #5b6676;
    --line: #dfe4ea; --accent: #a15c00; --code: #f2f4f7; --link: #0b57d0;
  }
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body {
  margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.65 ui-sans-serif, system-ui, -apple-system, "Segoe UI",
        Roboto, "Noto Sans", sans-serif;
}
.wrap { max-width: 1180px; margin: 0 auto; padding: 0 20px; }
.top {
  position: sticky; top: 0; z-index: 10;
  background: color-mix(in srgb, var(--bg) 88%, transparent);
  backdrop-filter: blur(8px); border-bottom: 1px solid var(--line);
}
.nav { display: flex; gap: 14px; align-items: center; flex-wrap: wrap; padding: 10px 0; }
.nav a { color: var(--muted); text-decoration: none; font-size: 14px; padding: 4px 6px; border-radius: 6px; }
.nav a:hover { color: var(--text); background: var(--panel); }
.nav a.active { color: var(--text); background: var(--panel); }
.nav .brand { font-weight: 700; color: var(--text); font-size: 16px; margin-right: 6px; }
.nav .gh { margin-left: auto; border: 1px solid var(--line); }
.layout { display: block; }
.content { max-width: 900px; padding: 32px 0 72px; }
h1, h2, h3, h4 { line-height: 1.25; margin: 1.6em 0 .55em; }
h1 { font-size: 2rem; margin-top: .6em; letter-spacing: -.02em; }
h2 { font-size: 1.45rem; padding-bottom: .25em; border-bottom: 1px solid var(--line); }
h3 { font-size: 1.15rem; }
h4 { font-size: 1rem; }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
.anchor { opacity: 0; margin-left: .35em; color: var(--muted); font-weight: 400; }
h1:hover .anchor, h2:hover .anchor, h3:hover .anchor { opacity: 1; }
p { margin: .7em 0; }
ul, ol { padding-left: 1.45em; }
li { margin: .25em 0; }
code {
  background: var(--code); border: 1px solid var(--line); border-radius: 5px;
  padding: .08em .35em; font: .9em/1.5 ui-monospace, SFMono-Regular, Menlo,
  Consolas, "Liberation Mono", monospace;
}
pre {
  background: var(--code); border: 1px solid var(--line); border-radius: 10px;
  padding: 14px 16px; overflow: auto; line-height: 1.5;
}
pre code { border: 0; padding: 0; background: none; font-size: .88em; }
blockquote {
  margin: 1em 0; padding: .6em .9em; color: var(--muted);
  border-left: 3px solid var(--accent); background: var(--panel);
  border-radius: 0 8px 8px 0;
}
hr { border: 0; border-top: 1px solid var(--line); margin: 2em 0; }
.table-wrap { overflow-x: auto; margin: 1em 0; }
table { border-collapse: collapse; width: 100%; font-size: .93em; }
th, td { border: 1px solid var(--line); padding: 7px 10px; text-align: left; vertical-align: top; }
th { background: var(--panel); }
tbody tr:nth-child(even) { background: color-mix(in srgb, var(--panel) 55%, transparent); }
.toc {
  background: var(--panel); border: 1px solid var(--line); border-radius: 10px;
  padding: 10px 16px; margin: 20px 0 28px; font-size: .92em;
}
.toc h2 { font-size: .82em; text-transform: uppercase; letter-spacing: .08em;
  color: var(--muted); border: 0; margin: 4px 0 8px; }
.toc ul { list-style: none; padding: 0; margin: 0; columns: 2; column-gap: 28px; }
.toc li { margin: 2px 0; break-inside: avoid; }
.toc .toc-h3 { padding-left: 12px; font-size: .95em; }
.toc a { color: var(--text); }
.foot {
  display: flex; justify-content: space-between; gap: 12px; flex-wrap: wrap;
  margin-top: 56px; padding-top: 16px; border-top: 1px solid var(--line);
  color: var(--muted); font-size: 13px;
}
.foot a { color: var(--muted); }
@media (max-width: 760px) {
  .toc ul { columns: 1; }
  .content { padding-top: 20px; }
  h1 { font-size: 1.65rem; }
}
"""


def main():
    os.makedirs(DOCS, exist_ok=True)
    with open(os.path.join(DOCS, "style.css"), "w", encoding="utf-8") as f:
        f.write(CSS)
    with open(os.path.join(DOCS, ".nojekyll"), "w", encoding="utf-8") as f:
        f.write("")

    for src, outname, title, _label in PAGES:
        path = os.path.join(DOCS, src)
        with open(path, encoding="utf-8") as f:
            md = f.read()
        body, toc = convert(md, title)
        if toc:
            k = body.find("</h1>")
            if k >= 0:
                body = body[:k + 5] + "\n" + toc + body[k + 5:]
        html_doc = page(outname, title, body, "", src)
        with open(os.path.join(DOCS, outname), "w", encoding="utf-8") as f:
            f.write(html_doc)
        print("html: docs/%s" % outname)

    # 404 sederhana
    with open(os.path.join(DOCS, "404.html"), "w", encoding="utf-8") as f:
        f.write("""<!doctype html><html lang="id"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Tidak ditemukan · Kopi</title><link rel="stylesheet" href="style.css">
</head><body><div class="wrap content" style="padding-top:64px">
<h1>404 — halaman tidak ditemukan</h1>
<p><a href="index.html">Kembali ke dokumentasi Kopi</a></p>
</div></body></html>""")

    print("html: docs/404.html, docs/style.css, docs/.nojekyll")


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        sys.exit(0)
