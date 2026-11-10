#!/usr/bin/env python3
"""gen_chacha: tulis test/chacha.kop — vektor RFC 7539 §2.4.2."""
import sys

KEY   = bytes(range(32))
NONCE = bytes.fromhex("000000000000004a00000000")   # 7 nol lalu 4a (RFC 7539 2.4.2)
PLAIN = (b"Ladies and Gentlemen of the class of '99: If I could offer you "
         b"only one tip for the future, sunscreen would be it.")
CIPH  = bytes.fromhex(
    "6e2e359a2568f98041ba0728dd0d6981"
    "e97e7aec1d4360c20a27afccfd9fae0b"
    "f91b65c5524733ab8f593dabcd62b357"
    "1639d624e65152ab8f530c359f0861d8"
    "07ca0dbf500d6a6156a38e088a22b65e"
    "52bc514d16ccf806818ce91ab7793736"
    "5af90bbf74a35be6b40b8eedf2785e42"
    "874d")


def hlp(name, b):
    body = "    static int[] %s() {\n        int[] v = new int[%d];\n" % (name, len(b))
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body


src = """import ChaCha;
import Hash;

class Main {
    public static void main(String[] args) {
        int[] c = ChaCha.xctr(key(), nonce(), 1, plain());
        System.out.println(Hash.eq(c, ciphRef()));
        int[] back = ChaCha.xctr(key(), nonce(), 1, c);
        System.out.println(Hash.eq(back, plain()));
    }
""" + hlp("key", KEY) + "\n" + hlp("nonce", NONCE) + "\n" + \
      hlp("plain", PLAIN) + "\n" + hlp("ciphRef", CIPH) + "}\n"
path = sys.argv[1] if len(sys.argv) > 1 else "/data/data/com.termux/files/usr/tmp/kopi/chacha.kop"
open(path, "w").write(src)
print("wrote", path)
