#!/usr/bin/env python3
"""gen_mathvec: uji menyeluruh Math/Digits/Stats/Matrix melawan oracle python.

Setiap kasus menghasilkan satu baris `1` bila hasil Kopi == nilai oracle.
Oracle dihitung murni di python (int32 wrap untuk operasi yang memang
membungkus). Generator TIDAK memakai mesin Kopi sebagai referensi.
"""
import sys

M32 = 0xFFFFFFFF


def s32(x):
    """pemetaan ke int32 bertanda (semantik Java/Kopi)"""
    x &= M32
    return x - 0x100000000 if x >= 0x80000000 else x


def avg_floor(a, b):
    return a + true_div_java(b - a, 2)


def true_div_java(a, b):
    """pembagian int Java/Kopi: trunc ke arah nol"""
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return q


def mul_mod(a, b, m):
    return (a % m) * (b % m) % m


def primes_up_to(n):
    if n < 2:
        return []
    sieve = [True] * (n + 1)
    sieve[0] = sieve[1] = False
    for p in range(2, int(n ** 0.5) + 1):
        if sieve[p]:
            for j in range(p * p, n + 1, p):
                sieve[j] = False
    return [i for i in range(n + 1) if sieve[i]]


def factorize(n):
    if n < 2:
        return []
    out = []
    m = n
    while m % 2 == 0:
        out.append(2)
        m //= 2
    p = 3
    while p * p <= m:
        while m % p == 0:
            out.append(p)
            m //= p
        p += 2
    if m > 1:
        out.append(m)
    return out


def num_divisors(n):
    if n < 1:
        return 0
    c = 0
    for i in range(1, int(n ** 0.5) + 1):
        if n % i == 0:
            c += 1 if i * i == n else 2
    return c


def sum_divisors(n):
    if n < 1:
        return 0
    s = 0
    for i in range(1, n + 1):
        if n % i == 0:
            s += i
    return s


def euler_phi(n):
    if n < 1:
        return 0
    r = n
    m = n
    p = 2
    while p * p <= m:
        if m % p == 0:
            while m % p == 0:
                m //= p
            r -= r // p
        p += 1
    if m > 1:
        r -= r // m
    return r


def radical(n):
    if n < 1:
        return 0
    r = 1
    for p in set(factorize(n)):
        r *= p
    return r


def mobius(n):
    if n < 1:
        return 0
    fs = factorize(n)
    if len(fs) != len(set(fs)):
        return 0
    return -1 if len(fs) % 2 else 1


def is_prime(n):
    if n < 2:
        return 0
    if n < 4:
        return 1
    if n % 2 == 0:
        return 0
    i = 3
    while i * i <= n:
        if n % i == 0:
            return 0
        i += 2
    return 1


def next_prime(n):
    x = max(n + 1, 2)
    while not is_prime(x):
        x += 1
    return x


def prev_prime(n):
    x = n - 1
    while x >= 2:
        if is_prime(x):
            return x
        x -= 1
    return -1


def nth_prime(k):
    if k < 1:
        return 0
    c = 0
    x = 1
    while c < k:
        x += 1
        if is_prime(x):
            c += 1
    return x


def extended_gcd(a, b):
    old_r, r = a, b
    old_s, s = 1, 0
    old_t, t = 0, 1
    while r != 0:
        q = true_div_java(old_r, r)
        old_r, r = r, old_r - q * r
        old_s, s = s, old_s - q * s
        old_t, t = t, old_t - q * t
    if old_r < 0:
        old_r, old_s, old_t = -old_r, -old_s, -old_t
    return [old_r, old_s, old_t]


def mod_inverse(a, m):
    if m <= 1:
        return -1
    g, x, _ = extended_gcd(a, m)
    if g != 1:
        return -1
    return x % m


def binomial(n, k):
    if n < 0 or k < 0 or k > n:
        return 0
    k = min(k, n - k)
    r = 1
    for i in range(1, k + 1):
        r = r * (n - k + i) // i
    return r


def catalan(n):
    return binomial(2 * n, n) // (n + 1)


def fib(n):
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a


def lucas(n):
    if n == 0:
        return 2
    a, b = 2, 1
    for _ in range(1, n):
        a, b = b, a + b
    return b


def tribonacci(n):
    if n == 0:
        return 0
    if n <= 2:
        return 1
    a, b, c = 0, 0, 1
    for _ in range(3, n + 1):
        a, b, c = b, c, a + b + c
    return c


def derangement(n):
    d0, d1 = 1, 0
    if n == 0:
        return 1
    for i in range(2, n + 1):
        d0, d1 = d1, (i - 1) * (d1 + d0)
    return d1


def stirling2(n, k):
    if n < 0 or k < 0 or k > n:
        return 0
    prev = [0] * (k + 1)
    prev[0] = 1
    for i in range(1, n + 1):
        cur = [0] * (k + 1)
        for j in range(1, min(k, i) + 1):
            cur[j] = prev[j - 1] + j * prev[j]
        prev = cur
    return prev[k]


def bell(n):
    return sum(stirling2(n, k) for k in range(n + 1))


def sqrt_floor(n):
    if n < 0:
        return -1
    if n < 2:
        return n
    x = n
    y = (x + 1) // 2
    while y < x:
        x = y
        y = (x + n // x) // 2
    return x


def cbrt(n):
    neg = n < 0
    m = -n if neg else n
    lo, hi = 0, 1290
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if mid == 0 or mid <= m // mid // mid:
            lo = mid
        else:
            hi = mid - 1
    return -lo if neg else lo


def popcount(n):
    return bin(n & M32).count("1")


def bit_length(n):
    if n == 0:
        return 0
    if n < 0:
        return 32
    return n.bit_length()


def highest_bit(n):
    return -1 if n == 0 else (n.bit_length() - 1)


def lowest_bit(n):
    if n == 0:
        return -1
    return (n & -n).bit_length() - 1


def reverse_bits(n):
    return s32(int(format(n & M32, "032b")[::-1], 2))


def next_pow2(n):
    if n <= 1:
        return 1
    r = 1
    while r < n:
        r *= 2
    return r


def prev_pow2(n):
    if n < 1:
        return 0
    r = 1
    while r <= n // 2:
        r *= 2
    return r


def digit_reverse(n):
    neg = n < 0
    n = abs(n)
    r = 0
    while n > 0:
        r = r * 10 + n % 10
        n //= 10
    return -r if neg else r


def digit_sum(n):
    return sum(int(c) for c in str(abs(n)))


def digit_count(n):
    return len(str(abs(n)))


def pow10(n):
    n = abs(n)
    d = 1
    while n >= 10:
        d *= 10
        n //= 10
    return d


def digit_product(n):
    n = abs(n)
    r = 1
    for c in str(n):
        r *= int(c)
    return r


def is_armstrong(n):
    k = digit_count(n)
    return int(sum(int(c) ** k for c in str(n)) == n)


def digital_root(n):
    n = abs(n)
    return 0 if n == 0 else 1 + (n - 1) % 9


def sum_square_digits(n):
    return sum(int(c) ** 2 for c in str(abs(n)))


def sum_cube_digits(n):
    return sum(int(c) ** 3 for c in str(abs(n)))


# ── kumpulan kasus (kode Kopi, nilai oracle) ──────────────────────────────
KASUS = []


def i(code, val):
    KASUS.append(("int", code, s32(int(val))))


def a(code, arr):
    KASUS.append(("arr", code, [s32(int(x)) for x in arr]))


# dasar
i("Math.abs(-5)", 5)
i("Math.abs(5)", 5)
i("Math.sign(-9)", -1)
i("Math.sign(0)", 0)
i("Math.sign(3)", 1)
i("Math.min(3, -2)", -2)
i("Math.max(3, -2)", 3)
i("Math.min3(5, 2, 9)", 2)
i("Math.max3(5, 2, 9)", 9)
i("Math.clamp(15, 0, 10)", 10)
i("Math.clamp(-5, 0, 10)", 0)
i("Math.diff(3, 9)", 6)
i("Math.sumTo(100)", 5050)
i("Math.avgFloor(-7, 2)", avg_floor(-7, 2))
i("Math.avgFloor(7, 2)", avg_floor(7, 2))

# bit
i("Math.isEven(10)", 1)
i("Math.isOdd(7)", 1)
i("Math.popcount(255)", popcount(255))
i("Math.popcount(-1)", popcount(-1))
i("Math.highestBit(1000)", highest_bit(1000))
i("Math.lowestBit(40)", lowest_bit(40))
i("Math.bitLength(255)", bit_length(255))
i("Math.bitLength(-1)", bit_length(-1))
i("Math.isPowerOfTwo(64)", 1)
i("Math.isPowerOfTwo(63)", 0)
i("Math.nextPow2(70)", next_pow2(70))
i("Math.prevPow2(70)", prev_pow2(70))
i("Math.reverseBits(2)", reverse_bits(2))
i("Math.reverseBits(15)", reverse_bits(15))

# pangkat/akar/log/modular
i("Math.pow(2, 10)", 1024)
i("Math.pow(3, 5)", 243)
i("Math.pow(5, 0)", 1)
i("Math.mod(-7, 3)", 2)
i("Math.mod(7, 3)", 1)
i("Math.addMod(2000000000, 2000000000, 1000000007)",
  (2000000000 + 2000000000) % 1000000007)
i("Math.mulMod(123456789, 987654321, 1000000007)",
  mul_mod(123456789, 987654321, 1000000007))
i("Math.powMod(2, 100, 1000000007)", pow(2, 100, 1000000007))
i("Math.sqrt(0)", 0)
i("Math.sqrt(1)", 1)
i("Math.sqrt(2)", 1)
i("Math.sqrt(1000000)", 1000)
i("Math.sqrt(2147395600)", 46340)
i("Math.cbrt(27)", 3)
i("Math.cbrt(-27)", -3)
i("Math.cbrt(28)", 3)
i("Math.cbrt(1000000)", 100)
i("Math.log2(1024)", 10)
i("Math.log10(999)", 2)
i("Math.log10(1000)", 3)
i("Math.isPerfectSquare(144)", 1)
i("Math.isPerfectSquare(145)", 0)
i("Math.isPerfectCube(27)", 1)
i("Math.isPerfectCube(-27)", 1)

# gcd/lcm/bezout
i("Math.gcd(48, 18)", 6)
i("Math.gcd3(12, 18, 24)", 6)
i("Math.lcm(4, 6)", 12)
i("Math.lcm3(2, 3, 4)", 12)
i("Math.isCoprime(9, 28)", 1)
i("Math.isCoprime(9, 27)", 0)
a("Math.extendedGcd(240, 46)", extended_gcd(240, 46))
i("Math.modInverse(3, 11)", mod_inverse(3, 11))
i("Math.modInverse(4, 8)", mod_inverse(4, 8))

# prima & faktor
i("Math.isPrime(2)", 1)
i("Math.isPrime(1)", 0)
i("Math.isPrime(97)", 1)
i("Math.isPrime(91)", 0)
i("Math.isPrime(2147483647)", 1)
i("Math.nextPrime(100)", next_prime(100))
i("Math.prevPrime(100)", prev_prime(100))
i("Math.prevPrime(2)", -1)
a("Math.primesUpTo(30)", primes_up_to(30))
i("Math.primeCount(100)", len(primes_up_to(100)))
i("Math.nthPrime(10)", nth_prime(10))
i("Math.nthPrime(1)", 2)
a("Math.factorize(360)", factorize(360))
a("Math.factorize(97)", factorize(97))
a("Math.factorize(1)", factorize(1))
i("Math.numDivisors(360)", num_divisors(360))
i("Math.sumDivisors(360)", sum_divisors(360))
i("Math.aliquotSum(28)", sum_divisors(28) - 28)
i("Math.isPerfect(28)", 1)
i("Math.isPerfect(12)", 0)
i("Math.isAbundant(12)", 1)
i("Math.isDeficient(8)", 1)
i("Math.eulerPhi(36)", euler_phi(36))
i("Math.radical(360)", radical(360))
i("Math.mobius(30)", mobius(30))
i("Math.mobius(6)", mobius(6))
i("Math.mobius(12)", mobius(12))

# kombinatorika
i("Math.factorial(0)", 1)
i("Math.factorial(5)", 120)
i("Math.factorial(10)", 3628800)
i("Math.binomial(5, 2)", binomial(5, 2))
i("Math.binomial(10, 5)", binomial(10, 5))
i("Math.binomial(52, 5)", binomial(52, 5))
i("Math.permutation(5, 2)", 20)
i("Math.catalan(5)", catalan(5))
i("Math.fib(0)", 0)
i("Math.fib(10)", fib(10))
i("Math.fib(20)", fib(20))
i("Math.lucas(10)", lucas(10))
i("Math.tribonacci(10)", tribonacci(10))
i("Math.triangular(10)", 55)
i("Math.derangement(5)", derangement(5))
i("Math.stirling2(5, 2)", stirling2(5, 2))
i("Math.bell(5)", bell(5))

# geometri
i("Math.distanceSq(0, 0, 3, 4)", 25)
i("Math.manhattan(0, 0, 3, 4)", 7)
i("Math.rectArea(6, 7)", 42)
i("Math.rectPerimeter(6, 7)", 26)
i("Math.isRightTriangle(3, 4, 5)", 1)
i("Math.isRightTriangle(2, 3, 4)", 0)
i("Math.heron16(3, 4, 5)", 576)
i("Math.isTriangle(3, 4, 5)", 1)
i("Math.isTriangle(1, 2, 5)", 0)

# Digits
i("Digits.reverse(12345)", digit_reverse(12345))
i("Digits.reverse(-120)", digit_reverse(-120))
i("Digits.digitSum(907)", digit_sum(907))
i("Digits.digitCount(12345)", digit_count(12345))
i("Digits.pow10(12345)", pow10(12345))
i("Digits.digitProduct(12345)", digit_product(12345))
i("Digits.isPalindrome(12321)", 1)
i("Digits.isPalindrome(123)", 0)
i("Digits.isArmstrong(153)", is_armstrong(153))
i("Digits.isArmstrong(154)", is_armstrong(154))
i("Digits.digitalRoot(9875)", digital_root(9875))
i("Digits.sumSquareDigits(123)", sum_square_digits(123))
i("Digits.sumCubeDigits(123)", sum_cube_digits(123))

# Stats — xs = [3,1,4,1,5,9,2,6]
XS = [3, 1, 4, 1, 5, 9, 2, 6]
i("Stats.sum(xs())", sum(XS))
i("Stats.product(xs())", __import__("functools").reduce(lambda a, b: a * b, XS, 1))
i("Stats.min(xs())", min(XS))
i("Stats.max(xs())", max(XS))
i("Stats.range(xs())", max(XS) - min(XS))
i("Stats.mean(xs())", true_div_java(sum(XS), len(XS)))
i("Stats.sumSquares(xs())", sum(x * x for x in XS))
i("Stats.sumCubes(xs())", sum(x ** 3 for x in XS))
srt = sorted(XS)
i("Stats.median(xs())", true_div_java(srt[3] + srt[4], 2))
i("Stats.mode(xs())", 1)
i("Stats.variance(xs())", true_div_java(sum(x * x for x in XS), len(XS))
  - true_div_java(sum(XS), len(XS)) ** 2)
i("Stats.stddev(xs())", sqrt_floor(true_div_java(sum(x * x for x in XS), len(XS))
  - true_div_java(sum(XS), len(XS)) ** 2))
i("Stats.countEven(xs())", sum(1 for x in XS if x % 2 == 0))
i("Stats.countOdd(xs())", sum(1 for x in XS if x % 2 != 0))

# Matrix — A 2x2 {1,2,3,4}, B {5,6,7,8}, C 3x3
MA = [1, 2, 3, 4]
MB = [5, 6, 7, 8]
MC = [2, 0, 1, 1, 3, 2, 1, 1, 4]
a("Matrix.identity(2)", [1, 0, 0, 1])
a("Matrix.add(ma(), mb())", [MA[k] + MB[k] for k in range(4)])
a("Matrix.sub(ma(), mb())", [MA[k] - MB[k] for k in range(4)])
a("Matrix.scale(ma(), 3)", [3 * x for x in MA])
a("Matrix.transpose(2, ma())", [MA[0], MA[2], MA[1], MA[3]])
a("Matrix.mul(2, ma(), mb())",
  [MA[0] * MB[0] + MA[1] * MB[2], MA[0] * MB[1] + MA[1] * MB[3],
   MA[2] * MB[0] + MA[3] * MB[2], MA[2] * MB[1] + MA[3] * MB[3]])
i("Matrix.trace(2, ma())", 5)
i("Matrix.det2(ma())", -2)
i("Matrix.det3(mc())",
  MC[0] * (MC[4] * MC[8] - MC[5] * MC[7])
  - MC[1] * (MC[3] * MC[8] - MC[5] * MC[6])
  + MC[2] * (MC[3] * MC[7] - MC[4] * MC[6]))
i("Matrix.det(2, ma())", -2)
i("Matrix.isIdentity(2, Matrix.identity(2))", 1)
i("Matrix.isIdentity(2, ma())", 0)


def hlp(name, vals):
    body = "    static int[] %s() {\n        int[] v = new int[%d];\n" % (name, len(vals))
    for k, x in enumerate(vals):
        body += "        v[%d] = %d;\n" % (k, x)
    body += "        return v;\n    }"
    return body


BOOL_PREFIX = (
    "Math.isEven(", "Math.isOdd(", "Math.isPowerOfTwo(",
    "Math.isPerfectSquare(", "Math.isPerfectCube(", "Math.isCoprime(",
    "Math.isPrime(", "Math.isPerfect(", "Math.isAbundant(",
    "Math.isDeficient(", "Math.isRightTriangle(", "Math.isTriangle(",
    "Digits.isPalindrome(", "Digits.isArmstrong(", "Matrix.isIdentity(",
)


def main():
    lines = []
    helpers = []
    ai = 0
    for kind, code, val in KASUS:
        if kind == "int":
            if code.startswith(BOOL_PREFIX):
                lines.append("        System.out.println(b2i(%s) == %d);" % (code, val))
            else:
                lines.append("        System.out.println(%s == %d);" % (code, val))
        else:
            nm = "exp%d" % ai
            ai += 1
            helpers.append(hlp(nm, val))
            lines.append("        System.out.println(eq(%s, %s()));" % (code, nm))

    src = """import Math;
import Digits;
import Stats;
import Matrix;

class Main {
    static int b2i(boolean b) {
        if (b) { return 1; }
        return 0;
    }

    static int eq(int[] a, int[] b) {
        if (a.length != b.length) { return 0; }
        for (int i = 0; i < a.length; i++) {
            if (a[i] != b[i]) { return 0; }
        }
        return 1;
    }

    static int[] xs() {
        int[] v = new int[8];
        v[0] = 3; v[1] = 1; v[2] = 4; v[3] = 1;
        v[4] = 5; v[5] = 9; v[6] = 2; v[7] = 6;
        return v;
    }

    static int[] ma() {
        int[] v = new int[4];
        v[0] = 1; v[1] = 2; v[2] = 3; v[3] = 4;
        return v;
    }

    static int[] mb() {
        int[] v = new int[4];
        v[0] = 5; v[1] = 6; v[2] = 7; v[3] = 8;
        return v;
    }

    static int[] mc() {
        int[] v = new int[9];
        v[0] = 2; v[1] = 0; v[2] = 1;
        v[3] = 1; v[4] = 3; v[5] = 2;
        v[6] = 1; v[7] = 1; v[8] = 4;
        return v;
    }

    public static void main(String[] args) {
%s
    }

%s
}
""" % ("\n".join(lines), "\n".join(helpers))

    path = sys.argv[1] if len(sys.argv) > 1 else \
        "/data/data/com.termux/files/usr/tmp/kopi/mathvec.kop"
    open(path, "w").write(src)
    with open(path + ".names", "w") as nf:
        for k, (kind, code, val) in enumerate(KASUS):
            nf.write("%d\t%s\t%s\n" % (k, code, val))
    print("kasus: %d → %s" % (len(KASUS), path))


if __name__ == "__main__":
    main()
