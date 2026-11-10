#!/usr/bin/env python3
"""bench.py — uji ketangguhan proyek besar Kopi.

- kompilasi a64 & VM secara PARALEL
- jalankan a64 & VM secara PARALEL, ukur wall time + puncak RSS (VmHWM)
- timeout per proses (deteksi hang)
- bandingkan output a64 vs VM byte-per-byte
- stress: N proses a64 paralel sekaligus
- cek sintaks x64 (rakit dengan cc -target)
"""
import concurrent.futures as cf
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
KOPICC = os.path.join(ROOT, "kopicc")
KOPIVM = os.path.join(ROOT, "kopivm")
SRC = os.path.join(HERE, "ProyekBesar.kop")
TMP = os.environ.get("TMPDIR", "/tmp")
OUT = os.path.join(TMP, "proyekbesar")
os.makedirs(OUT, exist_ok=True)


def sh(cmd, timeout=180):
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        o, e = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        o, e = p.communicate()
        return dict(ok=False, hang=True, rc=-1, wall=time.time() - t0,
                    out=o, err=e, rss=0)
    return dict(ok=p.returncode == 0, hang=False, rc=p.returncode,
                wall=time.time() - t0, out=o, err=e, rss=0)


def run_measured(cmd, out_path, timeout=120):
    """jalankan cmd, sampling /proc/<pid>/status VmHWM (puncak RSS)."""
    t0 = time.time()
    with open(out_path, "wb") as fo:
        p = subprocess.Popen(cmd, stdout=fo, stderr=subprocess.PIPE)
        peak = 0
        while p.poll() is None:
            try:
                with open("/proc/%d/status" % p.pid) as st:
                    for ln in st:
                        if ln.startswith("VmHWM:"):
                            peak = max(peak, int(ln.split()[1]))
                            break
            except OSError:
                pass
            if time.time() - t0 > timeout:
                p.kill()
                p.wait()
                return dict(ok=False, hang=True, rc=-1,
                            wall=time.time() - t0, rss=peak)
            time.sleep(0.005)
        err = p.stderr.read()
        p.stderr.close()
    return dict(ok=p.returncode == 0, hang=False, rc=p.returncode,
                wall=time.time() - t0, rss=peak, err=err)


def compile_all():
    jobs = {
        "a64": [KOPICC, SRC, "-t", "a64", "-o", os.path.join(OUT, "p.s")],
        "vm": [KOPICC, SRC, "-t", "vm", "-o", os.path.join(OUT, "p.kbc")],
        "x64": [KOPICC, SRC, "-t", "x64", "-o", os.path.join(OUT, "p64.s")],
    }
    with cf.ThreadPoolExecutor(max_workers=3) as ex:
        futs = {k: ex.submit(sh, v, 240) for k, v in jobs.items()}
        res = {k: f.result() for k, f in futs.items()}
    if res["a64"]["ok"]:
        cc = sh(["cc", os.path.join(OUT, "p.s"), "-o", os.path.join(OUT, "p64bin")], 120)
        res["link_a64"] = cc
    else:
        res["link_a64"] = dict(ok=False)
    if res["x64"]["ok"]:
        x = sh(["cc", "-target", "x86_64-linux-gnu", "-c",
                os.path.join(OUT, "p64.s"), "-o", os.path.join(OUT, "p64.o")], 120)
        res["x64_asm"] = x
    else:
        res["x64_asm"] = dict(ok=False)
    return res


def fmt(res):
    if res.get("hang"):
        return "HANG (timeout)"
    if not res.get("ok"):
        return "GAGAL rc=%s" % res.get("rc")
    return "OK"


def main():
    print("== kompilasi paralel (a64, vm, x64) ==")
    c = compile_all()
    for k in ("a64", "vm", "x64"):
        r = c[k]
        print("  compile %-4s : %s (%.2fs)" % (k, fmt(r), r.get("wall", 0)))
        if not r.get("ok") and r.get("err"):
            print("    stderr:", r["err"].decode(errors="replace")[:400])
    if not c["a64"]["ok"] or not c["vm"]["ok"]:
        sys.exit("kompilasi gagal")
    print("  link a64    : %s" % fmt(c["link_a64"]))
    print("  asm x64     : %s" % fmt(c["x64_asm"]))

    print("\n== eksekusi paralel a64 & VM (timeout 120s) ==")
    a_out = os.path.join(OUT, "a64.out")
    v_out = os.path.join(OUT, "vm.out")
    with cf.ThreadPoolExecutor(max_workers=2) as ex:
        fa = ex.submit(run_measured, [os.path.join(OUT, "p64bin")], a_out, 120)
        fv = ex.submit(run_measured, [KOPIVM, os.path.join(OUT, "p.kbc")], v_out, 120)
        ra, rv = fa.result(), fv.result()
    for name, r in (("a64", ra), ("vm", rv)):
        print("  %-3s : %-12s wall=%6.3fs peakRSS=%7.1f MB rc=%s" %
              (name, fmt(r), r.get("wall", 0), r.get("rss", 0) / 1024.0, r.get("rc")))
    if ra.get("hang") or rv.get("hang"):
        sys.exit("HANG terdeteksi")
    if not ra.get("ok") or not rv.get("ok"):
        sys.exit("eksekusi gagal")

    a = open(a_out, "rb").read()
    v = open(v_out, "rb").read()
    same = a == v
    print("\n  output a64 : %d byte / %d baris" % (len(a), a.count(b"\n")))
    print("  output vm  : %d byte / %d baris" % (len(v), v.count(b"\n")))
    print("  identik    : %s" % ("YA" if same else "TIDAK"))
    if not same:
        for i, (x, y) in enumerate(zip(a.split(b"\n"), v.split(b"\n"))):
            if x != y:
                print("  baris %d: a64=%r vm=%r" % (i + 1, x[:40], y[:40]))
                break
        sys.exit("output beda")

    print("\n== stress paralel: 4 proses a64 bersamaan ==")
    outs = [os.path.join(OUT, "par%d.out" % i) for i in range(4)]
    with cf.ThreadPoolExecutor(max_workers=4) as ex:
        futs = [ex.submit(run_measured, [os.path.join(OUT, "p64bin")], o, 120)
                for o in outs]
        rs = [f.result() for f in futs]
    total = 0.0
    for i, r in enumerate(rs):
        total += r.get("wall", 0)
        print("  par%d : %-12s wall=%6.3fs peakRSS=%7.1f MB" %
              (i, fmt(r), r.get("wall", 0), r.get("rss", 0) / 1024.0))
    allsame = all(open(o, "rb").read() == a for o in outs if os.path.exists(o))
    print("  4 output identik dengan a64: %s" % ("YA" if allsame else "TIDAK"))
    if not allsame:
        sys.exit("stress paralel menghasilkan output berbeda")

    print("\n== ringkasan ==")
    print("  proyek      : 54 file .kop (termasuk main)")
    print("  output      : %d baris, identik VM & a64" % a.count(b"\n"))
    print("  hang        : tidak (semua < timeout)")
    print("  wall a64/vm : %.3fs / %.3fs" % (ra["wall"], rv["wall"]))
    print("  RSS a64/vm  : %.1f MB / %.1f MB" % (ra["rss"] / 1024.0, rv["rss"] / 1024.0))


if __name__ == "__main__":
    main()
