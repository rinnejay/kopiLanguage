#!/usr/bin/env python3
"""gen_urlvec.py — oracle Python untuk test/urltest.kop (RFC 3986).

Mencetak hasil yang diharapkan, satu baris per System.out.println di
urltest.kop. Sumber kebenaran: urllib.parse (quote/unquote_plus/urlsplit/
urljoin/urlunsplit) + posixpath.normpath.
"""
import posixpath
from urllib.parse import quote, unquote_plus, urlsplit, urljoin

def port_of(url, default):
    p = urlsplit(url).port
    return default if p is None else p

def ip4(s):
    parts = s.split(".")
    if len(parts) != 4:
        return 0
    v = []
    for p in parts:
        if not p.isdigit() or len(p) > 3 or int(p) > 255:
            return 0
        v.append(int(p))
    return v[0] * 16777216 + v[1] * 65536 + v[2] * 256 + v[3]

out = []
out.append(quote("a b/c?d=1&e", safe=""))
out.append(quote("A-Z_a~z.0", safe=""))
out.append(quote("100%", safe=""))
out.append(quote("", safe=""))
out.append(quote("a b+c", safe="").replace("%20", "+"))
out.append(unquote_plus("a+b%20c"))
out.append(unquote_plus("%41%42%zz"))
# pair + appendQuery
out.append(quote("q", safe="").replace("%20", "+") + "=" + quote("a b&c", safe="").replace("%20", "+"))
out.append("a=1" + "&" + "q=a%20b")

u = "http://user@h:8080/p/x?a=1&b=2#frag"
sp = urlsplit(u)
out.append(sp.scheme)
out.append(sp.hostname or "")
out.append(str(port_of(u, 80)))
out.append(sp.path)
out.append(sp.query)
out.append(sp.fragment)
out.append(str(port_of("http://h/x", 80)))

base = "http://a/b/c/d;p?q"
for rel in ["g:h", "g", "./g", "g/", "/g", "//g", "?y", "g?y",
            "#s", "g#s", "", "..", "../g", "../../g"]:
    out.append(urljoin(base, rel))

out.append(posixpath.normpath("/a/b/../c/./d"))
out.append(posixpath.normpath("/x/../../y"))

out.append(str(ip4("127.0.0.1")))
out.append(str(ip4("999.1.1.1")))
out.append(str(ip4("a.b.c.d")))

print("\n".join(out))
