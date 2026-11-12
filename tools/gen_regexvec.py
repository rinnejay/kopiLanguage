#!/usr/bin/env python3
"""gen_regexvec.py — oracle python (re) untuk test/regexvec.kop.

Urutan baris harus sama dengan test/regexvec.kop.
"""
import re

out = []


def s(pat, txt):
    return "1" if re.search(pat, txt) else "0"


def m(pat, txt):
    return "1" if re.fullmatch(pat, txt) else "0"


def g(pat, txt):
    mm = re.search(pat, txt)
    if not mm:
        return "0"
    return "|".join(mm.group(i) if mm.group(i) is not None else "" for i in range(0, mm.re.groups + 1))


def fa(pat, txt):
    return ",".join(str(x.start()) for x in re.finditer(pat, txt))


def rp(pat, txt, rep):
    return re.sub(pat, rep, txt)


out.append(s("abc", "xxabcyy"))
out.append(s("abc", "xx"))
out.append(m("a.c", "abc"))
out.append(m("a.c", "abcd"))
out.append(s("[0-9]+", "abc123def"))
out.append(s("^abc$", "abc"))
out.append(s("^abc$", "xabc"))
out.append(g("([a-z]+)@([a-z]+)", "user@host"))
out.append(g(r"(\d{4})-(\d{2})", "tahun 2024-05"))
out.append(g("(a|b)+", "abab"))
out.append(fa(r"\d+", "a1b22c333"))
out.append(fa("[a-z]+", "ab cd ef"))
out.append(rp(r"\s+", "a   b  c", "-"))
out.append(rp("[aeiou]", "makan nasi", "*"))
out.append(s("colou?r", "color"))
out.append(s("colou?r", "colour"))
out.append(s("a+b", "aaab"))
out.append(s(r"\w+@\w+", "halo@dunia"))
out.append(s("a{2}", "aa"))
out.append(s("a{2}", "a"))
out.append(s("a{2,}", "aaa"))
out.append(s("a{2,3}b", "aaab"))
out.append(s("^a.*z$", "abcz"))
out.append(s("[^abc]+", "xyz"))
out.append(s(r"a\.c", "a.c"))
out.append(s(r"a\.c", "abc"))
out.append(str(len(re.findall(r"\d", "a1b2"))))

print("\n".join(out))
