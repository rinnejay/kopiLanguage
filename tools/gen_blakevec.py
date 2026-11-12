#!/usr/bin/env python3
"""gen_blakevec.py — oracle python untuk test/blakevec.kop (BLAKE2b + scrypt)."""
import hashlib
import os
import sys

out = []
out.append(hashlib.blake2b(b"").hexdigest())
out.append(hashlib.blake2b(b"abc").hexdigest())
out.append(hashlib.blake2b(b"abc", digest_size=32).hexdigest())
out.append(hashlib.blake2b(b"abc", key=b"kunci").hexdigest())
out.append(hashlib.blake2b(b"x" * 200).hexdigest())

out.append(hashlib.scrypt(b"password", salt=b"NaCl", n=16, r=8, p=1, dklen=32).hex())
out.append(hashlib.scrypt(b"password", salt=b"NaCl", n=16, r=8, p=2, dklen=32).hex())
out.append(hashlib.scrypt(b"", salt=b"", n=16, r=1, p=1, dklen=64).hex())

print("\n".join(out))
