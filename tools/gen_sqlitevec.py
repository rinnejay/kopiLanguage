#!/usr/bin/env python3
"""gen_sqlitevec.py — vektor regresi Sqlite.kop (pembaca file .db, v1.38).

Pemakaian: tools/gen_sqlitevec.py <dir-keluaran>

Membuat (deterministik, tanpa acak):
  <dir>/sqlitevec.db   — db fixture: page_size 512, 3 tabel
                         (Barang 14 baris: NULL/int64/REAL/blob/overflow,
                          Log 200 baris: b-tree multipage + interior,
                          Pasangan 5 baris: tanpa INTEGER PRIMARY KEY)
  <dir>/sqlitevec.kop  — program uji (path db dibenamkan)
  <dir>/sqlitevec.ref  — oracle: python sqlite3 membaca db yang sama
                         (format REAL memakai replika persis
                          Str.fromDouble: 6 digit pecahan, nol ekor dibuang)
"""
import os
import sqlite3
import sys


def kopi_double(d):
    """replika persis Str.fromDouble (lib/core/Str.kop)."""
    if d != d:
        return "nan"
    neg = d < 0
    x = -d if neg else d
    if x > 9000000000000000000.0:
        return ("-inf" if neg else "inf")
    ip = int(x)
    frac = x - ip
    s = str(ip)
    if frac > 0.0000005:
        digs = []
        for _ in range(6):
            frac *= 10
            dig = int(frac)
            digs.append(str(dig))
            frac -= dig
        while digs and digs[-1] == "0":
            digs.pop()
        if digs:
            s += "." + "".join(digs)
    return ("-" if neg else "") + s


BARANG = [
    (1, "kopi", 1.5, b"\x00\xff\x10", -42),
    (2, None, -0.75, b"", 0),
    (3, "A" * 2000, 100.125, bytes(i % 256 for i in range(300)), 7),
    (4, "teh", 12345.6789, None, 9007199254740993),
    (5, "", 3.0, b"\x01\x02", None),
    (6, "gula", 2.25, b"\xde\xad\xbe\xef", 123456),
    (7, "garam" * 60, 0.5, b"\x00", -999999),
    (8, "beras", 1.0, b"\xff" * 500, 42),
    (9, None, None, None, None),
    (10, "susu", 99.5, b"\x01", 1),
    (11, "madu", 0.125, b"\x02\x03\x04", 2),
    (12, "minyak", -1.25, b"", 3),
    (13, "cuka", 4.75, b"\x05" * 800, -3),
    (14, "tepung", 10.0, b"\x06", 10000000000),
]

LOG = [(i, "log-%03d-%05d" % (i, i * 7)) for i in range(1, 201)]

PASANGAN = [(("k%d" % i), ("v%d" % i)) for i in range(1, 6)]


def build_db(path):
    if os.path.exists(path):
        os.remove(path)
    con = sqlite3.connect(path)
    cur = con.cursor()
    cur.execute("PRAGMA page_size=512")
    cur.execute("CREATE TABLE Barang("
                "id INTEGER PRIMARY KEY, nama TEXT, harga REAL, "
                "tag BLOB, jumlah INTEGER)")
    cur.execute("CREATE TABLE Log(id INTEGER PRIMARY KEY, pesan TEXT)")
    cur.execute("CREATE TABLE Pasangan(k TEXT, v TEXT)")
    cur.executemany(
        "INSERT INTO Barang VALUES (?,?,?,?,?)",
        [(i, n, h, t, j) for (i, n, h, t, j) in BARANG])
    cur.executemany("INSERT INTO Log VALUES (?,?)", LOG)
    cur.executemany("INSERT INTO Pasangan VALUES (?,?)", PASANGAN)
    con.commit()
    con.close()


def oracle(path):
    """oracle python: baca db fixture yang sama, hasil = .ref"""
    con = sqlite3.connect(path)
    cur = con.cursor()
    names = [r[0] for r in cur.execute(
        "SELECT name FROM sqlite_master WHERE type='table' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY rootpage")]
    out = ["== tabel", " ".join(names)]
    for t in names:
        cols = [r[1] for r in cur.execute("PRAGMA table_info(%s)" % t)]
        out.append("== kolom " + t)
        out.append(" ".join(cols))
        out.append("== baris " + t)
        last = None
        for row in cur.execute(
                "SELECT %s FROM %s" % (",".join(cols), t)):
            rid = row[cols.index("id")] if "id" in cols else None
            if rid is not None:
                assert last is None or rid > last, "urutan rowid"
                last = rid
            vals = []
            for c, v in zip(cols, row):
                s = c + "="
                if v is None:
                    s += "NULL"
                elif isinstance(v, float):
                    s += kopi_double(v)
                elif isinstance(v, bytes):
                    s += v.hex()
                else:
                    s += str(v)
                vals.append(s)
            out.append(" ".join(vals))
    out.append("== selesai")
    con.close()
    return "\n".join(out) + "\n"


KOP = '''// sqlitevec.kop — HASIL GENERATOR (tools/gen_sqlitevec.py); jangan edit.
import Str;
import Bytes;
import Sqlite;

class Main {
    static void main() {
        malloc {
            String path = "DBPATH";
            int[] ctx = Sqlite.open(path);
            if (ctx[0] < 0) { System.out.println("gagal buka"); return; }
            String[] t = Sqlite.tables(ctx);
            System.out.println("== tabel");
            for (int i = 0; i < t.length; i++) {
                if (i > 0) System.out.print(" ");
                System.out.print(t[i]);
            }
            System.out.println("");
            String[] names = new String[3];
            names[0] = "Barang"; names[1] = "Log"; names[2] = "Pasangan";
            for (int i = 0; i < 3; i++) {
                String nm = names[i];
                String[] c = Sqlite.columns(ctx, nm);
                System.out.println("== kolom " + nm);
                for (int j = 0; j < c.length; j++) {
                    if (j > 0) System.out.print(" ");
                    System.out.print(c[j]);
                }
                System.out.println("");
                int k = Sqlite.query(ctx, nm);
                if (k < 0) {
                    System.out.println("query gagal err=" + Str.fromInt(Sqlite.errOf(ctx)));
                    continue;
                }
                System.out.println("== baris " + nm);
                while (Sqlite.next(ctx)) {
                    String line = "";
                    for (int j = 0; j < k; j++) {
                        if (j > 0) line = line + " ";
                        line = line + c[j] + "=";
                        if (Sqlite.isNull(ctx, j)) {
                            line = line + "NULL";
                        } else {
                            int ty = Sqlite.typeOf(ctx, j);
                            if (ty == 7) {
                                line = line + Str.fromDouble(Sqlite.realOf(ctx, j));
                            } else if (ty >= 13 && (ty % 2) != 0) {
                                line = line + Sqlite.strOf(ctx, j);
                            } else if (ty >= 12) {
                                line = line + Bytes.text(Bytes.hex(Sqlite.blobOf(ctx, j)));
                            } else {
                                line = line + Str.fromLong(Sqlite.intOf(ctx, j));
                            }
                        }
                    }
                    System.out.println(line);
                }
                if (Sqlite.errOf(ctx) != 0) {
                    System.out.println("scan err=" + Str.fromInt(Sqlite.errOf(ctx)));
                }
            }
            System.out.println("== selesai");
            Sqlite.close(ctx);
        }
    }
}
'''


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    dbp = os.path.join(outdir, "sqlitevec.db")
    build_db(dbp)
    ref = oracle(dbp)
    with open(os.path.join(outdir, "sqlitevec.kop"), "w") as f:
        f.write(KOP.replace("DBPATH", dbp))
    with open(os.path.join(outdir, "sqlitevec.ref"), "w") as f:
        f.write(ref)
    print("gen_sqlitevec: %s (3 tabel, %d baris) + kop + ref"
          % (dbp, len(BARANG) + len(LOG) + len(PASANGAN)))


if __name__ == "__main__":
    main()
