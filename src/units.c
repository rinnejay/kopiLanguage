/* units.c — penggabungan source: import + multi-file CLI.
 *
 * Model: compile-time merge ala #include, bukan separate compilation
 * per-object. Semua file yang terlibat dilex → token disatukan → parser
 * melihat satu compilation unit besar. File yang sama (secara realpath)
 * hanya dimasukkan sekali — mencegah duplikat & siklus impor.
 *
 * Pencarian modul `import NamaClass;`:
 *   1) folder file pengimpor (Nama.kop lalu nama.kop) — lokal menang
 *   2) lib standar: <folder kopicc>/lib (atau $KOPPATH jika diset)
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lex.h"
#include "ast.h"

typedef struct {
    Tok *t;
    int n, cap;
} TokArr;

static void ta_push(TokArr *a, Tok tk) {
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 256;
        a->t = realloc(a->t, sizeof(Tok) * (size_t)a->cap);
        if (!a->t) die("out of memory");
    }
    a->t[a->n++] = tk;
}

typedef struct {
    char *paths[256];
    int n;
} Visited;

static int visited_add(Visited *v, const char *path) {
    char buf[PATH_MAX];
    if (!realpath(path, buf)) return 1;   /* file tak ada; dibiarkan error di lex */
    for (int i = 0; i < v->n; i++)
        if (!strcmp(v->paths[i], buf)) return 0;   /* sudah pernah dimuat */
    if (v->n >= 256) die("terlalu banyak file yang diimpor (maks 256)");
    v->paths[v->n++] = strdup(buf);
    return 0;
}

/* buang 'import' ... ';' lalu sisip token file yang diimpor di tempatnya */
static void splice_file(TokArr *out, const char *path, Visited *vis);

/* versi lowercase dari Nama: Keranjang → keranjang */
static void name_lower(const char *name, char *low, size_t sz) {
    size_t i = 0;
    for (; name[i] && i < sz - 1; i++)
        low[i] = (name[i] >= 'A' && name[i] <= 'Z') ? (char)(name[i] + 32) : name[i];
    low[i] = 0;
}

/* ditentukan sekali: $KOPPATH, atau <direktori kopicc>/lib.
 * /proc/self/exe tersedia di Linux/Termux — kopicc walau dipanggil
 * dari folder mana pun tetap menemukan lib/ di sampingnya. */
static const char *libdir(void) {
    static char buf[PATH_MAX];
    static int init = 0;
    if (init) return buf;
    init = 1;
    const char *env = getenv("KOPPATH");
    if (env && *env) { snprintf(buf, sizeof buf, "%s", env); return buf; }
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 6);
    if (n > 0) {
        buf[n] = 0;
        char *slash = strrchr(buf, '/');
        if (slash) *slash = 0;
        else buf[0] = 0;
        strncat(buf, "/lib", sizeof buf - strlen(buf) - 1);
        return buf;
    }
    buf[0] = 0;   /* fallback terakhir: "./lib" relatif cwd */
    return "";
}

/* cari file .kop untuk import Nama; urutan:
 * 1) folder file pengimpor (kasus asli & lowercase) — lokal menang
 * 2) folder lib standar (kasus asli & lowercase) */
static char *find_module(const char *dir, const char *name, int line) {
    char low[512];
    name_lower(name, low, sizeof low);
    const char *ld = libdir();
    const char *cand[4];
    char c0[PATH_MAX], c1[PATH_MAX], c2[PATH_MAX], c3[PATH_MAX];
    (void)snprintf(c0, sizeof c0, "%s%s.kop", dir, name);
    (void)snprintf(c1, sizeof c1, "%s%s.kop", dir, low);
    if (ld[0]) {
        (void)snprintf(c2, sizeof c2, "%s/%s.kop", ld, name);
        (void)snprintf(c3, sizeof c3, "%s/%s.kop", ld, low);
        cand[0] = c0; cand[1] = c1; cand[2] = c2; cand[3] = c3;
    } else {
        (void)snprintf(c2, sizeof c2, "./lib/%s.kop", name);
        (void)snprintf(c3, sizeof c3, "./lib/%s.kop", low);
        cand[0] = c0; cand[1] = c1; cand[2] = c2; cand[3] = c3;
    }
    for (int i = 0; i < 4; i++) {
        FILE *f = fopen(cand[i], "rb");
        if (f) { fclose(f); return strdup(cand[i]); }
    }
    die("baris %d: tidak menemukan modul untuk import '%s' "
        "(dicoba: %s, %s, %s, %s)",
        line, name, cand[0], cand[1], cand[2], cand[3]);
    return 0;
}

static char *dir_of(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) return strdup("");
    size_t n = (size_t)(slash - path) + 1;
    char *d = malloc(n + 1);
    memcpy(d, path, n);
    d[n] = 0;
    return d;
}

static void splice_file(TokArr *out, const char *path, Visited *vis) {
    int ntoks;
    Tok *toks = lex_file(path, &ntoks);
    const char *dir = dir_of(path);

    int i = 0;
    while (i < ntoks) {
        if (toks[i].type == T_EOF) break;          /* EOF per-file dibuang */
        if (toks[i].type != KW_IMPORT) { ta_push(out, toks[i]); i++; continue; }
        i++;
        char *modpath = 0;
        if (i < ntoks && toks[i].type == TK_STR) {
            size_t nl = strlen(dir) + strlen(toks[i].text) + 1;
            modpath = malloc(nl);
            snprintf(modpath, nl, "%s%s", dir, toks[i].text);
            i++;
        } else if (i < ntoks && toks[i].type == T_ID) {
            char name[256];
            snprintf(name, sizeof name, "%s", toks[i].text);
            int dotted = 0;
            while (i + 1 < ntoks && toks[i + 1].type == '.' && i + 2 < ntoks &&
                   toks[i + 2].type == T_ID) { dotted = 1; i += 2; }
            if (dotted)
                die("baris %d: import library eksternal belum didukung — gunakan import \"file.kop\"",
                    toks[i].line);
            modpath = find_module(dir, name, toks[i].line);
            i++;
        } else {
            die("baris %d: setelah 'import' harus ada \"namafile.kop\" atau NamaClass",
                i < ntoks ? toks[i].line : toks[ntoks - 1].line);
        }
        if (i >= ntoks || toks[i].type != ';')
            die("baris %d: ';' setelah pernyataan import", toks[i].line);
        i++;

        /* sisip token file yang diimpor DI POSISI eksak pernyataan import */
        if (visited_add(vis, modpath) == 0)
            splice_file(out, modpath, vis);   /* loop while: i sudah pas */
    }
}

Tok *units_load_many(const char **paths, int n, int *ntoks) {
    TokArr out = {0};
    Visited vis = {0};
    for (int i = 0; i < n; i++)
        if (visited_add(&vis, paths[i]) == 0)
            splice_file(&out, paths[i], &vis);
    ta_push(&out, (Tok){0});   /* sentinel T_EOF */
    *ntoks = out.n;
    return out.t;
}
