#!/usr/bin/env python3
"""gen_x509vec: oracle X.509 (parser DER python) + file uji .kop/.ref.

Membaca sertifikat PEM dari test/data/, menghitung nilai yang diharapkan
(subject, issuer, serial, masa berlaku, pin SPKI, kunci publik, OID
tanda tangan) lalu menulis program uji .kop dan berkas .ref.
"""
import base64
import calendar
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SHORT = {
    "2.5.4.3": "CN", "2.5.4.4": "SN", "2.5.4.5": "serialNumber", "2.5.4.6": "C",
    "2.5.4.7": "L", "2.5.4.8": "ST", "2.5.4.9": "STREET", "2.5.4.10": "O",
    "2.5.4.11": "OU", "2.5.4.12": "title", "2.5.4.42": "GN",
    "0.9.2342.19200300.100.1.25": "DC", "0.9.2342.19200300.100.1.1": "UID",
    "1.2.840.113549.1.9.1": "emailAddress",
}


def tlv(buf, pos):
    tag = buf[pos]
    p = pos + 1
    first = buf[p]
    p += 1
    if first & 0x80:
        nb = first & 0x7f
        ln = int.from_bytes(buf[p:p + nb], 'big')
        p += nb
    else:
        ln = first
    return tag, p, ln, p + ln


def oid_str(buf, pos, ln):
    first = buf[pos]
    a = first // 40
    b = first % 40
    if a > 2:
        a = 2
        b = first - 80
    parts = [str(a), str(b)]
    i = 1
    while i < ln:
        v = 0
        while i < ln:
            by = buf[pos + i]
            i += 1
            v = v * 128 + (by & 0x7f)
            if by < 0x80:
                break
        parts.append(str(v))
    return ".".join(parts)


def name_of(buf, pos, ln):
    out = []
    end = pos + ln
    p = pos
    while p < end:
        tag, cpos, clen, nxt = tlv(buf, p)
        if tag != 0x31:
            break
        q = cpos
        while q < nxt:
            tag2, c2, l2, n2 = tlv(buf, q)
            if tag2 != 0x30:
                break
            r = c2
            tag3, c3, l3, n3 = tlv(buf, r)
            key = SHORT.get(oid_str(buf, c3, l3), oid_str(buf, c3, l3))
            r = n3
            tag4, c4, l4, n4 = tlv(buf, r)
            out.append("%s=%s" % (key, buf[c4:c4 + l4].decode('utf-8', 'replace')))
            q = n2
        p = nxt
    return ",".join(out)


def time_epoch(buf, pos, ln, tag):
    raw = buf[pos:pos + ln].decode('ascii')
    digits = "".join(ch for ch in raw if ch.isdigit())
    if tag == 0x17:
        yy = int(digits[0:2])
        year = 2000 + yy if yy < 50 else 1900 + yy
        mon, day = int(digits[2:4]), int(digits[4:6])
        hh, mm = int(digits[6:8]), int(digits[8:10])
        ss = int(digits[10:12]) if len(digits) >= 12 else 0
    else:
        year = int(digits[0:4])
        mon, day = int(digits[4:6]), int(digits[6:8])
        hh, mm = int(digits[8:10]), int(digits[10:12])
        ss = int(digits[12:14]) if len(digits) >= 14 else 0
    return calendar.timegm((year, mon, day, hh, mm, ss, 0, 0, 0))


def fields(der):
    """posisi: serial, issuer, validity, subject, spki"""
    tag, cpos, clen, nxt = tlv(der, 0)
    tag, cpos, clen, tend = tlv(der, cpos)
    res = {}
    p = cpos
    seq = 0
    while p < tend:
        start = p
        tag, cpos2, clen2, nxt2 = tlv(der, p)
        if tag >= 0xa0:
            p = nxt2
        elif tag == 0x02:
            res['serial'] = (cpos2, clen2)
            p = nxt2
        elif tag == 0x30:
            seq += 1
            if seq == 2:
                res['issuer'] = (cpos2, clen2)
            elif seq == 3:
                res['validity'] = (cpos2, clen2)
            elif seq == 4:
                res['subject'] = (cpos2, clen2)
            elif seq == 5:
                res['spki'] = (start, nxt2 - start)
            p = nxt2
        else:
            p = nxt2
    return res


def analyze(path):
    pem = open(path, encoding="utf-8").read()
    body = pem.split("-----BEGIN ")[1].split("-----")[1].split("-----END ")[0]
    der = base64.b64decode("".join(body.split()))
    f = fields(der)
    s_pos, s_len = f['serial']
    i_pos, i_len = f['issuer']
    v_pos, v_len = f['validity']
    su_pos, su_len = f['subject']
    sp_pos, sp_len = f['spki']
    spki = der[sp_pos:sp_pos + sp_len]
    # notBefore / notAfter (v_pos = isi SEQUENCE validity)
    tag1, c1, l1, n1 = tlv(der, v_pos)
    tag2, c2, l2, n2 = tlv(der, n1)
    nb = time_epoch(der, c1, l1, tag1)
    na = time_epoch(der, c2, l2, tag2)
    # kunci publik: isi BIT STRING di SPKI
    tag, cpos, clen, nxt = tlv(spki, 0)
    tag, cpos, clen, nxt = tlv(spki, cpos)
    tag, cpos, clen, nxt = tlv(spki, nxt)
    pub = spki[cpos + 1:cpos + clen]
    # OID tanda tangan: AlgorithmIdentifier pertama di tbs
    tag, cpos, clen, tend = tlv(der, 0)
    tag, cpos, clen, tend = tlv(der, cpos)
    p = cpos
    seq = 0
    sigoid = ""
    while p < tend:
        tag, c2, l2, n2 = tlv(der, p)
        if tag == 0x30:
            seq += 1
            if seq == 1:
                t3, c3, l3, n3 = tlv(der, c2)
                sigoid = oid_str(der, c3, l3)
        p = n2
    return {
        "der_len": len(der),
        "label": pem.split("-----BEGIN ")[1].split("-----")[0].strip(),
        "subject": name_of(der, su_pos, su_len),
        "issuer": name_of(der, i_pos, i_len),
        "serial": der[s_pos:s_pos + s_len].hex(),
        "not_before": nb,
        "not_after": na,
        "pin": hashlib.sha256(spki).hexdigest(),
        "spki_len": len(spki),
        "pub_len": len(pub),
        "sig_oid": sigoid,
    }


def main():
    kop_path = sys.argv[1]
    ref_path = sys.argv[2]
    certs = [("test/data/cert-rsa.pem", "certRsa"), ("test/data/cert-ec.pem", "certEc")]
    body = ""
    ref = []
    for path, var in certs:
        info = analyze(os.path.join(ROOT, path))
        lines = [
            '        String pem%s = File.readText("%s");' % (var, path),
            '        int[] der%s = Pem.decode(pem%s);' % (var, var),
            '        System.out.println(Pem.label(pem%s));' % var,
            '        System.out.println(der%s.length);' % var,
            '        System.out.println(X509.subject(der%s));' % var,
            '        System.out.println(X509.issuer(der%s));' % var,
            '        System.out.println(X509.serialHex(der%s));' % var,
            '        System.out.println(X509.notBefore(der%s));' % var,
            '        System.out.println(X509.notAfter(der%s));' % var,
            '        System.out.println(Cipher.hexenc(X509.pin(der%s)));' % var,
            '        System.out.println(X509.spki(der%s).length);' % var,
            '        System.out.println(X509.pubKey(der%s).length);' % var,
            '        System.out.println(X509.sigAlg(der%s));' % var,
        ]
        body += "\n".join(lines) + "\n"
        ref += [info["label"], str(info["der_len"]), info["subject"], info["issuer"],
                info["serial"], str(info["not_before"]), str(info["not_after"]),
                info["pin"], str(info["spki_len"]), str(info["pub_len"]), info["sig_oid"]]
    src = ("import Pem;\nimport X509;\nimport Cipher;\nimport File;\n\nclass Main {\n"
           "    public static void main(String[] args) {\n" + body + "    }\n}\n")
    open(kop_path, "w").write(src)
    open(ref_path, "w").write("\n".join(ref) + "\n")
    print("wrote", kop_path, "dan", ref_path)


if __name__ == "__main__":
    main()
