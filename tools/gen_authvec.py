#!/usr/bin/env python3
"""gen_authvec.py — oracle Python untuk test/authvec.kop."""
import base64
import hashlib
import hmac
import json


def b64u(b):
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


out = []
for it, dk in [(1, 32), (2, 32), (4096, 32)]:
    out.append(hashlib.pbkdf2_hmac("sha256", b"password", b"salt", it, dk).hex())
out.append(hashlib.pbkdf2_hmac(
    "sha256", b"passwordPASSWORDpassword",
    b"saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 40).hex())

header = b'{"alg":"HS256","typ":"JWT"}'
payload = b'{"sub":"budi","n":1}'
secret = b"rahasia"
signing = b64u(header) + "." + b64u(payload)
sig = b64u(hmac.new(secret, signing.encode(), hashlib.sha256).digest())
token = signing + "." + sig
out.append(token)
out.append("1")
out.append("0")
out.append("-1")
out.append(payload.decode())

out += ["1", "42", "-1", "1", "-1", "1", "0", "tok.abc", "Bearer",
        'WWW-Authenticate: Basic realm="admin"\r', ""]
print("\n".join(out))
