#!/usr/bin/env python3
"""gen_numvec: uji tipe numerik baru (long/float/double/uint8..64) vs oracle python.

Menghasilkan dua berkas: program .kop dan .ref berisi output yang diharapkan.
Oracle memakai semantik yang sama dengan compiler:
  - int/long: 32/64-bit bertanda, meluap membungkus
  - uint8/16/32/64: unsigned, meluap membungkus pada lebarnya
  - float: IEEE binary32; double: binary64
  - cetak: %ld, %lu, %u, %.9g (float), %.15g (double)
  - promosi: int+uint32→long, uint32+uint32→uint32, dst (lihat ast.h)
"""
import struct
import sys

M32 = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF


def i32(x):
    x &= M32
    return x - 0x100000000 if x >= 0x80000000 else x


def i64(x):
    x &= M64
    return x - 0x10000000000000000 if x >= 0x8000000000000000 else x


def u8(x):
    return x & 0xFF


def u16(x):
    return x & 0xFFFF


def u32(x):
    return x & M32


def u64(x):
    return x & M64


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def fmt_f(x):
    return "%.9g" % f32(x)


def fmt_d(x):
    return "%.15g" % x


def tdiv(a, b):
    """pembagian Java/Kopi: trunc ke arah nol"""
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def tmod(a, b):
    return a - tdiv(a, b) * b


def trunc_float_to_int(x):
    return int(x)          # python int() truncates toward zero


CASES = []   # (kode ekspresi, nilai harian sebagai string)


def case(expr, expected):
    CASES.append((expr, str(expected)))


def bln(expr, val):
    case(expr, 1 if val else 0)


# ── literal & tipe dasar ────────────────────────────────────────────────
case("5000000000L", 5000000000)
case("-5000000000L", -5000000000)
case("9223372036854775807L", 9223372036854775807)
case("255", 255)
bln("255 == 255U", True)
case("4294967295U", 4294967295)
case("18446744073709551615UL", 18446744073709551615)
case("0x1F", 31)
case("0b101010", 42)
case("1.5", fmt_d(1.5))
case("0.1", fmt_d(0.1))
case("1.0 / 3.0", fmt_d(1.0 / 3.0))
case("1.5f", fmt_f(1.5))
case("0.1f", fmt_f(0.1))
case("1.0f / 3.0f", fmt_f(1.0 / 3.0))

# ── aritmetika long ──────────────────────────────────────────────────────
case("5000000000L + 1234567890123L", 5000000000 + 1234567890123)
case("-7L / 2L", tdiv(-7, 2))
case("-7L % 2L", tmod(-7, 2))
case("7L % -2L", tmod(7, -2))
case("100000L * 100000L", 100000 * 100000)
case("(1L << 40)", 1 << 40)
case("(1L << 63)", i64(1 << 63))
case("(-8L >> 2)", -8 >> 2)
case("(-8L >>> 60)", (M64 & -8) >> 60)

# ── unsigned wrap ────────────────────────────────────────────────────────
case("(uint8)(250 + 10)", u8(260))
case("(uint8)(200 * 2)", u8(400))
case("(uint16)(5 - 10)", u16(-5))
case("4294967290U + 10U", u32(4294967290 + 10))
case("4294967295U * 3U", u32(4294967295 * 3))
case("18446744073709551615UL + 1UL", u64(M64 + 1))
case("(uint8)(1 << 9)", u8(1 << 9))
case("(uint32)(1 << 31)", u32(1 << 31))

# ── promosi campuran ────────────────────────────────────────────────────
case("-1 + 1U", -1 + 1)                       # int+uint32 → long
bln("4294967295U > 0U", True)
bln("-1L > 0", False)
bln("18446744073709551615UL > 1UL", True)

# ── cast ────────────────────────────────────────────────────────────────
case("(uint8)300", u8(300))
case("(uint16)70000", u16(70000))
case("(uint32)-1", u32(-1))
case("(uint64)-1", u64(-1))
case("(int)4294967295U", i32(u32(4294967295)))
case("(long)4294967295U", 4294967295)
case("(int)5000000000L", i32(5000000000))
case("(int)3.99", trunc_float_to_int(3.99))
case("(int)-3.99", trunc_float_to_int(-3.99))
case("(long)-2.9", trunc_float_to_int(-2.9))
case("(double)7 / 2.0", fmt_d(7 / 2.0))
case("(float)(1.0 / 3.0)", fmt_f(1.0 / 3.0))

# ── float/double aritmetika ─────────────────────────────────────────────
case("1.5f + 2.25f", fmt_f(1.5 + 2.25))
case("1.5f * 2.0f", fmt_f(1.5 * 2.0))
case("7.5 % 2.0", fmt_d(tmod(7.5, 2.0)))
case("-7.5 % 2.0", fmt_d(tmod(-7.5, 2.0)))
case("2.0 / 4.0", fmt_d(0.5))
bln("1.5 < 2.0", True)
bln("1.5 == 1.5", True)
bln("0.0 / 0.0 == 0.0 / 0.0", False)      # NaN != NaN
bln("0.0 / 0.0 != 0.0 / 0.0", True)

# ── fungsi: parameter & return tiap tipe ────────────────────────────────
case("jumlahLong(5000000000L, 42L)", 5000000000 + 42)
case("jumlahU32(4294967295U, 2U)", u32(4294967295 + 2))
case("bagiDouble(7.0, 2)", fmt_d(7.0 / 2))
case("setengahFloat(3.5f)", fmt_f(3.5 / 2.0))
case("faktorialLong(10)", 3628800)
case("fibLong(50)", 12586269025)
case("(uint8)kaliU8(200, 2)", u8(200 * 2))

# ── perulangan & compound ───────────────────────────────────────────────
case("jumlahSampai(100)", 5050)
case("jumlahKuadratLong(10)", sum(i * i for i in range(1, 11)))


def main():
    kpath = sys.argv[1] if len(sys.argv) > 1 else "/tmp/numvec.kop"
    rpath = sys.argv[2] if len(sys.argv) > 2 else "/tmp/numvec.ref"

    lines = []
    for expr, expected in CASES:
        lines.append("        System.out.println(%s);" % expr)
        # NaN case: expected direpresentasikan lewat ekspresi boolean langsung
    # kasus NaN di atas sudah berupa boolean (bukan string angka)
    ref = "\n".join(e for _, e in CASES) + "\n"

    src = """class Main {
    static long jumlahLong(long a, long b) { return a + b; }
    static uint32 jumlahU32(uint32 a, uint32 b) { return a + b; }
    static double bagiDouble(double a, int b) { return a / b; }
    static float setengahFloat(float x) { return x / 2.0f; }
    static long faktorialLong(long n) {
        long r = 1L;
        long i = 2L;
        while (i <= n) { r = r * i; i = i + 1L; }
        return r;
    }
    static long fibLong(long n) {
        long a = 0L;
        long b = 1L;
        long i = 0L;
        while (i < n) {
            long c = a + b;
            a = b;
            b = c;
            i = i + 1L;
        }
        return a;
    }
    static uint8 kaliU8(uint8 a, uint8 b) { return (uint8)(a * b); }
    static long jumlahSampai(long n) {
        long s = 0L;
        long i = 1L;
        while (i <= n) { s = s + i; i = i + 1L; }
        return s;
    }
    static long jumlahKuadratLong(long n) {
        long s = 0L;
        long i = 1L;
        while (i <= n) { s = s + i * i; i = i + 1L; }
        return s;
    }

    public static void main(String[] args) {
%s
    }
}
""" % ("\n".join(lines))
    open(kpath, "w").write(src)
    open(rpath, "w").write(ref)
    print("kasus: %d -> %s / %s" % (len(CASES), kpath, rpath))


if __name__ == "__main__":
    main()
