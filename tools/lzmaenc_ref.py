#!/usr/bin/env python3
"""lzmaenc_ref.py — encoder LZMA1 referensi untuk pembangkit uji.

Port greedy hash-chain yang sudah divalidasi terhadap modul Python `lzma`:
setiap stream keluaran didekode ulang oleh gen_lzmaencvec.py, sehingga
nilai harapan tidak pernah ditebak.

API: raw_compress(data, lc=3, lp=0, pb=2, dict_size=1<<16) -> bytes
"""

MASK32 = 0xFFFFFFFF
MASK64 = (1 << 64) - 1


class RC:
    def __init__(self):
        self.low = 0
        self.range = MASK32
        self.cache = 0
        self.cache_size = 1
        self.out = bytearray()

    def shift_low(self):
        if (self.low & 0xFF000000) != 0xFF000000 or (self.low >> 32) != 0:
            temp = self.cache
            while True:
                self.out.append((temp + ((self.low >> 32) & 0xFF)) & 0xFF)
                temp = 0xFF
                self.cache_size -= 1
                if self.cache_size == 0:
                    break
            self.cache = (self.low >> 24) & 0xFF
        self.cache_size += 1
        self.low = (self.low & 0xFFFFFF) << 8

    def bit(self, probs, i, bit):
        p = probs[i]
        bound = (self.range >> 11) * p
        if bit == 0:
            self.range = bound
            probs[i] = p + ((2048 - p) >> 5)
        else:
            self.low += bound
            self.range = (self.range - bound) & MASK32
            probs[i] = p - (p >> 5)
        while self.range < (1 << 24):
            self.range = (self.range << 8) & MASK32
            self.shift_low()

    def direct(self, value, n):
        for i in range(n - 1, -1, -1):
            self.range >>= 1
            if (value >> i) & 1:
                self.low += self.range
            while self.range < (1 << 24):
                self.range = (self.range << 8) & MASK32
                self.shift_low()

    def flush(self):
        for _ in range(5):
            self.shift_low()


def tree(rc, probs, base, nbits, value):
    m = 1
    for i in range(nbits - 1, -1, -1):
        b = (value >> i) & 1
        rc.bit(probs, base + m, b)
        m = (m << 1) | b


class Enc:
    def __init__(self, data, lc=3, lp=0, pb=2, dict_size=1 << 16):
        self.data = data
        self.n = len(data)
        self.lc, self.lp, self.pb = lc, lp, pb
        self.pos_states = 1 << pb
        self.lp_mask = (1 << lp) - 1
        self.lit_states = 1 << (lc + lp)
        self.dict_size = max(dict_size, 4096)
        self.rc = RC()
        self.state = 0
        self.reps = [1, 1, 1, 1]
        self.prev = 0
        self.pos = 0
        self.is_match = [1024] * 192
        self.is_rep = [1024] * 12
        self.is_rep_g0 = [1024] * 12
        self.is_rep_g1 = [1024] * 12
        self.is_rep_g2 = [1024] * 12
        self.is_rep0_long = [1024] * 192
        self.pos_slot = [1024] * 256
        self.spec_pos = [1024] * 128
        self.align = [1024] * 16
        ls = 2 + self.pos_states * 16 + 256
        self.match_len = [1024] * ls
        self.rep_len = [1024] * ls
        self.literal = [1024] * (self.lit_states * 768)
        self.hbits = 16
        self.head = [-1] * (1 << self.hbits)
        self.prevpos = [-1] * self.n

    def hash3(self, p):
        a = self.data[p] & 255
        b = self.data[p + 1] & 255 if p + 1 < self.n else 0
        c = self.data[p + 2] & 255 if p + 2 < self.n else 0
        return ((a << 16) ^ (b << 8) ^ c) & ((1 << self.hbits) - 1)

    def insert(self, p):
        if p + 2 >= self.n:
            return
        h = self.hash3(p)
        self.prevpos[p] = self.head[h]
        self.head[h] = p

    def find(self):
        p = self.pos
        maxlen = min(273, self.n - p)
        best_len = 0
        best_dist = 0
        if maxlen >= 2:
            cand = self.head[self.hash3(p)] if p + 2 < self.n else -1
            chain = 0
            while cand >= 0 and chain < 128:
                dist = p - cand
                if dist > self.dict_size:
                    break
                l = 0
                while l < maxlen and self.data[cand + l] == self.data[p + l]:
                    l += 1
                if l > best_len:
                    best_len = l
                    best_dist = dist
                    if l == maxlen:
                        break
                cand = self.prevpos[cand]
                chain += 1
        rep_len = 0
        rep_idx = 0
        for r in range(4):
            d = self.reps[r]
            if d > p:
                continue
            l = 0
            while l < maxlen and self.data[p - d + l] == self.data[p + l]:
                l += 1
            if l > rep_len:
                rep_len = l
                rep_idx = r
        return best_len, best_dist, rep_len, rep_idx

    def encode_len(self, probs, length, pos_state):
        if length < 10:
            self.rc.bit(probs, 0, 0)
            tree(self.rc, probs, 2 + pos_state * 8, 3, length - 2)
        elif length < 18:
            self.rc.bit(probs, 0, 1)
            self.rc.bit(probs, 1, 0)
            tree(self.rc, probs, 2 + self.pos_states * 8 + pos_state * 8, 3, length - 10)
        else:
            self.rc.bit(probs, 0, 1)
            self.rc.bit(probs, 1, 1)
            tree(self.rc, probs, 2 + self.pos_states * 16, 8, length - 18)

    def encode_match(self, length, dist, pos_state):
        self.rc.bit(self.is_match, (self.state << 4) | pos_state, 1)
        self.rc.bit(self.is_rep, self.state, 0)
        self.encode_len(self.match_len, length, pos_state)
        d = dist - 1
        if d < 4:
            slot = d
        else:
            nb = d.bit_length() - 1
            slot = (nb << 1) | ((d >> (nb - 1)) & 1)
        len_state = min(length - 2, 3)
        tree(self.rc, self.pos_slot, len_state * 64, 6, slot)
        if slot < 4:
            pass
        else:
            nb = (slot >> 1) - 1
            if slot < 14:
                base = (2 | (slot & 1)) << nb
                footer = d - base
                t = base - slot - 1
                m = 1
                for k in range(nb):
                    b = (footer >> k) & 1
                    self.rc.bit(self.spec_pos, t + m, b)
                    m = (m << 1) | b
            else:
                nd = nb - 4
                direct = (d >> 4) & ((1 << nd) - 1)
                self.rc.direct(direct, nd)
                al = d & 15
                m = 1
                for k in range(4):
                    b = (al >> k) & 1
                    self.rc.bit(self.align, m, b)
                    m = (m << 1) | b
        self.reps = [dist, self.reps[0], self.reps[1], self.reps[2]]
        self.state = 7 if self.state < 7 else 10

    def encode_rep(self, length, idx, pos_state):
        self.rc.bit(self.is_match, (self.state << 4) | pos_state, 1)
        self.rc.bit(self.is_rep, self.state, 1)
        if idx == 0:
            self.rc.bit(self.is_rep_g0, self.state, 0)
            self.rc.bit(self.is_rep0_long, (self.state << 4) | pos_state, 1)
        else:
            self.rc.bit(self.is_rep_g0, self.state, 1)
            if idx == 1:
                self.rc.bit(self.is_rep_g1, self.state, 0)
            else:
                self.rc.bit(self.is_rep_g1, self.state, 1)
                if idx == 2:
                    self.rc.bit(self.is_rep_g2, self.state, 0)
                else:
                    self.rc.bit(self.is_rep_g2, self.state, 1)
        # update reps seperti decoder
        if idx == 0:
            dist = self.reps[0]
        elif idx == 1:
            dist = self.reps[1]
            self.reps[1] = self.reps[0]
            self.reps[0] = dist
        elif idx == 2:
            dist = self.reps[2]
            self.reps[2] = self.reps[1]
            self.reps[1] = self.reps[0]
            self.reps[0] = dist
        else:
            dist = self.reps[3]
            self.reps[3] = self.reps[2]
            self.reps[2] = self.reps[1]
            self.reps[1] = self.reps[0]
            self.reps[0] = dist
        self.encode_len(self.rep_len, length, pos_state)
        self.state = 8 if self.state < 7 else 11

    def encode_shortrep(self, pos_state):
        self.rc.bit(self.is_match, (self.state << 4) | pos_state, 1)
        self.rc.bit(self.is_rep, self.state, 1)
        self.rc.bit(self.is_rep_g0, self.state, 0)
        self.rc.bit(self.is_rep0_long, (self.state << 4) | pos_state, 0)
        self.state = 9 if self.state < 7 else 11

    def encode_literal(self, pos_state):
        self.rc.bit(self.is_match, (self.state << 4) | pos_state, 0)
        p = self.pos
        byte = self.data[p] & 255
        lit_state = ((p & self.lp_mask) << self.lc) | (self.prev >> (8 - self.lc))
        base = lit_state * 768
        sym = 1
        if self.state < 7:
            for i in range(7, -1, -1):
                b = (byte >> i) & 1
                self.rc.bit(self.literal, base + sym, b)
                sym = (sym << 1) | b
            self.state = 0 if self.state < 4 else (self.state - 3 if self.state < 10 else self.state - 6)
        else:
            match_byte = self.data[p - self.reps[0]] & 255
            offs = 0x100
            for i in range(7, -1, -1):
                match_byte <<= 1
                bitv = offs
                offs &= match_byte
                b = (byte >> i) & 1
                self.rc.bit(self.literal, base + offs + bitv + sym, b)
                sym = (sym << 1) | b
                if b == 0:
                    offs ^= bitv
            self.state = self.state - 3 if self.state < 10 else self.state - 6
        self.prev = byte

    def end_marker(self, pos_state):
        self.rc.bit(self.is_match, (self.state << 4) | pos_state, 1)
        self.rc.bit(self.is_rep, self.state, 0)
        self.encode_len(self.match_len, 2, pos_state)
        tree(self.rc, self.pos_slot, 0, 6, 63)
        self.rc.direct((1 << 26) - 1, 26)
        m = 1
        for k in range(4):
            self.rc.bit(self.align, m, 1)
            m = (m << 1) | 1

    def encode_tokens(self):
        while self.pos < self.n:
            pos_state = self.pos & (self.pos_states - 1)
            bl, bd, rl, ri = self.find()
            if rl >= 2 and rl + 1 >= bl:
                self.encode_rep(rl, ri, pos_state)
                ln = rl
            elif bl >= 2:
                self.encode_match(bl, bd, pos_state)
                ln = bl
            elif rl >= 1 and ri == 0:
                self.encode_shortrep(pos_state)
                ln = 1
            else:
                self.encode_literal(pos_state)
                ln = 1
            for k in range(ln):
                self.insert(self.pos + k)
            self.pos += ln
            self.prev = self.data[self.pos - 1] & 255

    def encode(self):
        self.encode_tokens()
        self.end_marker(self.pos & (self.pos_states - 1))
        self.rc.flush()
        return bytes(self.rc.out)

    def encode_no_end(self):
        """LZMA1 tanpa end marker — dipakai untuk chunk LZMA2."""
        self.encode_tokens()
        self.rc.flush()
        return bytes(self.rc.out)


def raw_compress(data, lc=3, lp=0, pb=2, dict_size=1 << 16):
    return Enc(data, lc, lp, pb, dict_size).encode()


def chunk_compress(data, lc=3, lp=0, pb=2, dict_size=1 << 16):
    return Enc(data, lc, lp, pb, dict_size).encode_no_end()


