#!/usr/bin/env python3
"""gen_bigintvec.py — oracle python untuk test/bigintvec.kop (lib/core/BigInt)."""
import math

A = 123456789012345678901234567890
B = 987654321098765432109876543210
M = 1000000007

out = []
out.append(str(A + B))
out.append(str(B - A))
out.append(str(A * B))
q, r = divmod(A, M)
out.append(str(q))
out.append(str(r))
out.append(str(pow(2, 1000, M)))
out.append(str(pow(3, -1, M)))
out.append(str(pow(3, -1, 7)))
out.append(str(pow(17, -1, 31)))
out.append(str(math.gcd(A, B)))
out.append(str(A << 10))
out.append(str(A >> 10))
out.append(str(A.bit_length()))
out.append(str(int.from_bytes(A.to_bytes(32, 'big'), 'big')))

print("\n".join(out))
