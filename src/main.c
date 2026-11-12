/* kopicc — driver compiler: .kop -> (assembly a64 | assembly x64 | bytecode KOP1) */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "lex.h"

void parse_all(Tok *toks);
void gen_vm(const char *outpath);
void gen_a64(const char *outpath);
void gen_x64(const char *outpath);
Tok *units_load_many(const char **paths, int n, int *ntoks);

const char *g_linefile = 0;
int g_line = 0;
int g_col = 0;

Prog prog;

/* ambil nomor baris dari pesan ("baris N: ...") */
static int msg_line_no(const char *m) {
    const char *p = strstr(m, "baris ");
    if (!p) return 0;
    p += 6;
    if (*p < '0' || *p > '9') return 0;
    int n = 0;
    while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
    return n;
}

/* tampilkan baris sumber + penunjuk kolom (bila diketahui) */
static void show_source_line(int line) {
    if (!g_linefile || !*g_linefile || line <= 0) return;
    if (g_linefile[0] == '<') return;
    FILE *f = fopen(g_linefile, "r");
    if (!f) return;
    char buf[1200];
    int n = 0;
    while (fgets(buf, sizeof buf, f)) {
        if (++n != line) continue;
        size_t L = strlen(buf);
        while (L && (buf[L - 1] == '\n' || buf[L - 1] == '\r')) buf[--L] = 0;
        fprintf(stderr, "  %4d | %s\n", n, buf);
        if (g_line == line && g_col > 0 && g_col <= (int)L + 1) {
            fprintf(stderr, "       | ");
            for (int i = 0; i < g_col - 1; i++)
                fputc(buf[i] == '\t' ? '\t' : ' ', stderr);
            fprintf(stderr, "^\n");
        }
        break;
    }
    fclose(f);
}

/* rekomendasi perbaikan berdasarkan pola pesan error */
static const char *hint_for(const char *m) {
    static const struct { const char *pat, *hint; } tab[] = {
    { "hidup di luar malloc{}",
      "Pindahkan pemakaian ke dalam blok, atau salin nilainya (referensi tidak boleh keluar arena)." },
    { "return bertipe rujukan dari dalam malloc{}",
      "Hitung/salin hasil di dalam blok, lalu lakukan `return` setelah blok malloc{} selesai." },
    { "template butuh 'import Str;'",
      "Tambahkan `import Str;` di awal berkas (sebelum `class`)." },
    { "metode String",
      "Tambahkan `import Str;` di awal berkas agar metode String tersedia." },
    { "';' setelah pernyataan import",
      "Akhiri pernyataan import dengan `;`, mis. `import Str;`." },
    { "expected ';'",
      "Tambahkan `;` di akhir pernyataan." },
    { "expected ')'",
      "Periksa pasangan tanda kurung `(` dan `)`." },
    { "expected '('",
      "Tambahkan `(` setelah nama metode/`if`/`while`/`for`." },
    { "expected '}'",
      "Tutup blok dengan `}`; periksa pasangan `{` dan `}`." },
    { "expected '{'",
      "Tambahkan `{` untuk membuka blok." },
    { "butuh cast", "Tambahkan cast eksplisit, mis. `(int)x`, `(long)x`, atau `(double)x`." },
    { "perlu cast eksplisit", "Tambahkan cast eksplisit agar tipe kedua operand sama." },
    { "tidak cocok tipe", "Samakan tipenya atau tambahkan cast eksplisit, mis. `(int)x`." },
    { "tidak cocok dengan tipe", "Samakan tipe nilai dengan deklarasi variabel (atau tambah cast)." },
    { "kondisi", "Kondisi harus boolean; gunakan perbandingan seperti `i < n` atau `x != 0`." },
    { "boolean", "Gunakan ekspresi boolean (perbandingan/`true`/`false`), bukan angka." },
    { "variabel", "Deklarasikan variabel sebelum dipakai dan periksa ejaan namanya." },
    { "metode '%s' tidak ada", "Periksa ejaan nama metode, atau impor berkas yang mendefinisikannya." },
    { "class '%s' tidak ada", "Definisikan class sebelum `new`, atau impor berkasnya." },
    { "tidak menemukan modul untuk import",
      "Periksa nama/path modul. Lib standar: `import Str;`, `import Net;`." },
    { "indeks array", "Indeks array yang valid adalah 0 sampai `panjang - 1`." },
    { ".length hanya", "`.length` hanya berlaku untuk array dan String." },
    { "duplikat", "Hapus atau ganti nama deklarasi yang duplikat." },
    { "case", "`case` hanya menerima literal bilangan bulat; pastikan juga tidak ada nilai ganda." },
    { "switch tanpa", "Tambahkan minimal satu `case` atau `default` di dalam switch." },
    { "static field belum didukung",
      "Gunakan metode konstanta, mis. `static int W() { return 800; }`, lalu panggil `W()`." },
    { "malloc<...> butuh angka", "Contoh: `malloc<64mb>`. Biarkan `malloc { }` untuk ukuran otomatis." },
    { "ukuran malloc", "Gunakan angka positif dengan satuan b/kb/mb/gb, mis. `malloc<64mb>`." },
    { "satuan", "Satuan yang didukung: b, kb, mb, gb (tanpa satuan = MB)." },
    { "maksimum 4096 MB", "Pakai ukuran ≤ 4096 MB, atau `malloc { }` agar otomatis." },
    { "belum didukung", "Fitur ini belum tersedia; lihat docs/06-batas-roadmap.md untuk alternatif." },
    { "Net.", "Periksa signature builtin Net.* di docs/05-lib.md." },
    { "JSON", "Gunakan bentuk JSON yang didukung: null/true/false/angka/string/objek/array." },
    { "pembagian dengan nol", "Pastikan pembagi tidak nol sebelum operasi pembagian/modulo." },
    { "arena malloc habis", "Naikkan batas dengan `malloc<...>`, atau gunakan `malloc { }` otomatis." },
    { "tidak dikenal", "Periksa ejaan nama dan tipe operand." },
    { 0, 0 }
    };
    for (int i = 0; tab[i].pat; i++)
        if (strstr(m, tab[i].pat)) return tab[i].hint;
    return 0;
}

void die(const char *fmt, ...) {
    char msg[768];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    fprintf(stderr, "kopicc: error: %s: %s\n",
            g_linefile ? g_linefile : "", msg);
    int ln = msg_line_no(msg);
    show_source_line(ln > 0 ? ln : g_line);
    const char *hint = hint_for(msg);
    if (hint) fprintf(stderr, "  saran: %s\n", hint);
    exit(1);
}

static const char *basename_ext(const char *path, const char *ext) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    char *out = malloc(strlen(base) + strlen(ext) + 2);
    strcpy(out, base);
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
    strcat(out, ext);
    return out;
}

int main(int argc, char **argv) {
    const char *srcs[64];
    int nsrc = 0;
    const char *out = 0;
    const char *target =
#if defined(__aarch64__) || defined(__arm64__)
        "a64";
#else
        "x64";
#endif

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) { out = argv[++i]; continue; }
        if (!strcmp(argv[i], "-t") && i + 1 < argc) { target = argv[++i]; continue; }
        if (!strcmp(argv[i], "a64") || !strcmp(argv[i], "x64") || !strcmp(argv[i], "vm"))
            { target = argv[i]; continue; }
        if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) {
            printf("kopicc 1.23.0 (Kopi) — target a64 | x64 | vm\n");
            return 0;
        }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("kopicc — compiler bahasa Kopi\n"
                   "  usage: kopicc <source.kop> [more.kop ...] [-o output] [-t a64|x64|vm]\n"
                   "         kopicc --version | --help\n\n"
                   "  multi-file: seberapa banyak .kop pun digabung jadi satu unit;\n"
                   "  di dalam file juga boleh dipakai:\n"
                   "      import \"util.kop\";      (path relatif terhadap file asal)\n"
                   "      import Util;            (dicari Util.kop / util.kop)\n\n"
                   "  target:\n"
                   "    a64  assembly aarch64 (rangkai: cc out.s -o app)  — jalan di HP/ARM\n"
                   "    x64  assembly x86-64 (rangkai: gcc out.s -o app)  — jalan di PC\n"
                   "    vm   bytecode KOP1 (jalan: kopivm out.kbc)        — portabel\n");
            return 0;
        }
        if (nsrc < 64) srcs[nsrc++] = argv[i];
        else die("terlalu banyak file source (maks 64)");
    }
    if (!nsrc) die("tidak ada source. ketik 'kopicc --help'");

    if (strcmp(target, "a64") && strcmp(target, "x64") && strcmp(target, "vm"))
        die("target tidak dikenal: %s (pilihan: a64 | x64 | vm)", target);

    int ntoks;
    Tok *toks = units_load_many(srcs, nsrc, &ntoks);
    parse_all(toks);

    if (!strcmp(target, "vm")) {
        const char *opath = out ? out : basename_ext(srcs[0], ".kbc");
        gen_vm(opath);
    } else {
        const char *opath = out ? out : basename_ext(srcs[0], ".s");
        if (!strcmp(target, "a64")) gen_a64(opath);
        else gen_x64(opath);
    }
    return 0;
}
