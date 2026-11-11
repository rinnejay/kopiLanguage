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

Prog prog;

void die(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "kopicc: error: %s: ", g_linefile ? g_linefile : "");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
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
            printf("kopicc 1.17.1 (Kopi) — target a64 | x64 | vm\n");
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
