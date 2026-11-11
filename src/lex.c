/* Kopi lexer: mengubah source .kop menjadi array token. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lex.h"
#include "ast.h"

static void *xmalloc(size_t n) {
    void *p = calloc(1, n);
    if (!p) die("out of memory");
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    return p;
}

static const char *g_srcfile;   /* file sedang di-lex */

static struct { const char *kw; int type; } kws[] = {
    {"class", KW_CLASS}, {"public", KW_PUBLIC}, {"private", KW_PRIVATE},
    {"static", KW_STATIC}, {"final", KW_FINAL}, {"int", KW_INT},
    {"boolean", KW_BOOL}, {"void", KW_VOID}, {"String", KW_STRING},
    {"long", KW_LONG}, {"double", KW_DOUBLE}, {"float", KW_FLOAT},
    {"uint8", KW_UINT8}, {"uint16", KW_UINT16},
    {"uint32", KW_UINT32}, {"uint64", KW_UINT64},
    {"byte", KW_BYTE}, {"short", KW_SHORT},
    {"int8", KW_INT8}, {"int16", KW_INT16},
    {"if", KW_IF}, {"else", KW_ELSE}, {"while", KW_WHILE}, {"do", KW_DO}, {"for", KW_FOR},
    {"return", KW_RETURN}, {"true", KW_TRUE}, {"false", KW_FALSE},
    {"break", KW_BREAK}, {"continue", KW_CONTINUE}, {"import", KW_IMPORT},
    {"fn", KW_FN}, {"json", KW_JSON},
    {"switch", KW_SWITCH}, {"case", KW_CASE}, {"default", KW_DEFAULT},   /* v1.19 */
    {0, 0},
};

static char *unescape(const char *s, size_t n, char endq) {
    char *out = xmalloc(n + 1);
    size_t i = 0, j = 0;
    while (i < n && s[i] != endq) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[i + 1];
            i += 2;
            switch (c) {
            case 'n': out[j++] = '\n'; break;
            case 't': out[j++] = '\t'; break;
            case 'r': out[j++] = '\r'; break;
            case '0': die("literal string: escape \\0 tidak didukung"); break;
            default: out[j++] = c; break;   /* \" \\ dll */
            }
        } else {
            out[j++] = s[i++];
        }
    }
    return out;
}

/* lewati satu template `...{...}...` (mendukung nested template di dalam
 * interpolasi). *pp masuk di posisi backtick, keluar setelah backtick tutup. */
static void scan_tpl(const char **pp, int *line) {
    const char *p = *pp;
    p++;                                   /* lewati ` pembuka */
    int depth = 0;
    while (*p) {
        if (*p == '\\' && p[1]) {
            if (p[1] == '\n') (*line)++;
            p += 2;
            continue;
        }
        if (*p == '`') {
            if (depth == 0) { p++; *pp = p; return; }
            scan_tpl(&p, line);            /* template bersarang */
            continue;
        }
        if (depth > 0 && (*p == '"' || *p == '\'')) {
            char q = *p;
            p++;
            while (*p && *p != q) {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            if (*p) p++;
            continue;
        }
        if (*p == '{') depth++;
        else if (*p == '}') { if (depth > 0) depth--; }
        if (*p == '\n') (*line)++;
        p++;
    }
    die("template ` tidak ditutup");
}

static Tok *lex_scan(char *src, const char *fname, int startline, int *ntoks) {
    g_srcfile = fname;
    int cap = 256, n = 0, line = startline;
    Tok *toks = xmalloc(sizeof(Tok) * (size_t)cap);
    char *p = src;

#define PUSH(tt, ttx, tnm) do { \
        if (n == cap) { cap *= 2; toks = realloc(toks, sizeof(Tok) * (size_t)cap); } \
        toks[n].type = (tt); toks[n].text = (ttx); toks[n].num = (tnm); toks[n].line = line; toks[n].file = g_srcfile; n++; \
    } while (0)

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') line++;
                p++;
            }
            if (!*p) die("baris %d: komentar /* tidak ditutup", line);
            p += 2;
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            char *start = p;
            int base = 10, isflt = 0, suf = 0;
            char *digend;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                base = 16; p += 2;
                while (isxdigit((unsigned char)*p)) p++;
            } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
                base = 2; p += 2;
                while (*p == '0' || *p == '1') p++;
            } else {
                while (isdigit((unsigned char)*p)) p++;
                if (*p == '.') {
                    isflt = 1; p++;
                    while (isdigit((unsigned char)*p)) p++;
                }
                if (*p == 'e' || *p == 'E') {
                    isflt = 1; p++;
                    if (*p == '+' || *p == '-') p++;
                    while (isdigit((unsigned char)*p)) p++;
                }
            }
            digend = p;
            if (*p == 'l' || *p == 'L') { suf = 'L'; p++; }
            else if (*p == 'f' || *p == 'F') { suf = 'F'; p++; }
            else if (*p == 'd' || *p == 'D') { suf = 'D'; p++; }
            else if (*p == 'u' || *p == 'U') {
                suf = 'U'; p++;
                if (*p == 'l' || *p == 'L') {
                    suf = 'V'; p++;
                    if (*p == 'l' || *p == 'L') p++;
                }
            }
            if (isflt || suf == 'F' || suf == 'D') {
                char tmp[512];
                size_t ln = (size_t)(digend - start);
                if (ln >= sizeof tmp) die("baris %d: angka terlalu panjang", line);
                memcpy(tmp, start, ln); tmp[ln] = 0;
                double dv = strtod(tmp, 0);
                PUSH(T_NUM, 0, 0);
                toks[n - 1].fnum = dv;
                toks[n - 1].numty = (suf == 'F') ? (int)T_FLOAT : (int)T_DOUBLE;
                toks[n - 1].text = xstrndup(start, (size_t)(digend - start));
            } else {
                unsigned long long uv;
                int ty;
                if (base == 2) {
                    uv = 0;
                    char *q = start + 2;
                    while (q < digend) { uv = uv * 2 + (unsigned)(*q - '0'); q++; }
                } else if (base == 16) {
                    char tmp[512];
                    size_t ln = (size_t)(digend - start);
                    if (ln >= sizeof tmp) die("baris %d: angka terlalu panjang", line);
                    memcpy(tmp, start, ln); tmp[ln] = 0;
                    uv = strtoull(tmp, 0, 0);
                } else {
                    char tmp[512];
                    size_t ln = (size_t)(digend - start);
                    if (ln >= sizeof tmp) die("baris %d: angka terlalu panjang", line);
                    memcpy(tmp, start, ln); tmp[ln] = 0;
                    uv = strtoull(tmp, 0, 10);
                }
                if (suf == 'L') {
                    if (uv > 9223372036854775807ULL)
                        die("baris %d: literal long terlalu besar", line);
                    ty = (int)T_LONG;
                } else if (suf == 'U') {
                    if (uv > 4294967295ULL)
                        die("baris %d: literal uint32 terlalu besar", line);
                    ty = (int)T_UINT32;
                } else if (suf == 'V') {
                    ty = (int)T_UINT64;
                } else if (uv <= 2147483647ULL) {
                    ty = (int)T_INT;
                } else if (uv <= 9223372036854775807ULL) {
                    ty = (int)T_LONG;
                } else {
                    die("baris %d: literal melebihi long (pakai suffix ul untuk uint64)", line);
                    ty = (int)T_LONG;
                }
                PUSH(T_NUM, 0, (long)uv);
                toks[n - 1].numty = ty;
                toks[n - 1].fnum = 0.0;
                toks[n - 1].text = xstrndup(start, (size_t)(digend - start));
            }
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t len = (size_t)(p - start);
            int type = T_ID;
            for (int i = 0; kws[i].kw; i++)
                if (strlen(kws[i].kw) == len && !strncmp(kws[i].kw, start, len))
                    { type = kws[i].type; break; }
            PUSH(type, type == T_ID ? xstrndup(start, len) : 0, 0);
            continue;
        }
        if (*p == '`') {
            char *start = p + 1;
            scan_tpl(&p, &line);
            char *end = p - 1;                 /* sebelum backtick tutup */
            PUSH(TK_TSTR, xstrndup(start, (size_t)(end - start)), 0);
            continue;
        }
        if (*p == '"' || *p == '\'') {
            char q = *p;
            p++;
            char *start = p;
            while (*p && *p != q) {
                if (*p == '\n') die("baris %d: string tidak ditutup", line);
                if (*p == '\\') p++;
                p++;
            }
            if (!*p) die("baris %d: string tidak ditutup", line);
            PUSH(TK_STR, unescape(start, (size_t)(p - start), q), 0);
            p++;
            continue;
        }
        /* operator 2-char */
        if (p[0] == '&' && p[1] == '&') { PUSH(T_AND, 0, 0); p += 2; continue; }
        if (p[0] == '|' && p[1] == '|') { PUSH(T_OR, 0, 0); p += 2; continue; }
        if (p[0] == '=' && p[1] == '=') { PUSH(T_EQ, 0, 0); p += 2; continue; }
        if (p[0] == '!' && p[1] == '=') { PUSH(T_NE, 0, 0); p += 2; continue; }
        if (p[0] == '<' && p[1] == '=') { PUSH(T_LE, 0, 0); p += 2; continue; }
        if (p[0] == '>' && p[1] == '=') { PUSH(T_GE, 0, 0); p += 2; continue; }
        /* ++ -- += -= *= /= %= => */
        if (p[0] == '+' && p[1] == '+') { PUSH(T_PP, 0, 0);    p += 2; continue; }
        if (p[0] == '-' && p[1] == '-') { PUSH(T_MM, 0, 0);    p += 2; continue; }
        if (p[0] == '+' && p[1] == '=') { PUSH(T_ADDEQ, 0, 0); p += 2; continue; }
        if (p[0] == '-' && p[1] == '=') { PUSH(T_SUBEQ, 0, 0); p += 2; continue; }
        if (p[0] == '*' && p[1] == '=') { PUSH(T_MULEQ, 0, 0); p += 2; continue; }
        if (p[0] == '/' && p[1] == '=') { PUSH(T_DIVEQ, 0, 0); p += 2; continue; }
        if (p[0] == '%' && p[1] == '=') { PUSH(T_MODEQ, 0, 0); p += 2; continue; }
        if (p[0] == '=' && p[1] == '>') { PUSH(T_ARROW, 0, 0); p += 2; continue; }
        /* bitwise: shift & compound; single-char & | ^ ~ jatuh ke strchr */
        if (p[0] == '&' && p[1] == '=') { PUSH(T_ANDEQ, 0, 0);  p += 2; continue; }
        if (p[0] == '|' && p[1] == '=') { PUSH(T_OREQ, 0, 0);   p += 2; continue; }
        if (p[0] == '^' && p[1] == '=') { PUSH(T_XOREQ, 0, 0);  p += 2; continue; }
        if (p[0] == '<' && p[1] == '<' && p[2] == '=') { PUSH(T_SHLEQ, 0, 0);  p += 3; continue; }
        if (p[0] == '<' && p[1] == '<') { PUSH(T_SHL, 0, 0);     p += 2; continue; }
        if (p[0] == '>' && p[1] == '>' && p[2] == '>' && p[3] == '=') { PUSH(T_USHREQ, 0, 0); p += 4; continue; }
        if (p[0] == '>' && p[1] == '>' && p[2] == '>') { PUSH(T_USHR, 0, 0);  p += 3; continue; }
        if (p[0] == '>' && p[1] == '>' && p[2] == '=') { PUSH(T_SHREQ, 0, 0);  p += 3; continue; }
        if (p[0] == '>' && p[1] == '>') { PUSH(T_SHR, 0, 0);     p += 2; continue; }
        if (strchr("+-*/%(){};:,.=!<>[]&|^~?", *p) && *p) {
            PUSH((unsigned char)*p, 0, 0);
            p++;
            continue;
        }
        die("baris %d: karakter tidak dikenal '%c'", line, *p);
    }
    PUSH(T_EOF, 0, 0);
    (void)src;
    *ntoks = n;
    return toks;
}

Tok *lex_file(const char *path, int *ntoks) {
    g_srcfile = path;
    FILE *f = fopen(path, "rb");
    if (!f) die("tidak bisa membuka file: %s", path);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = xmalloc((size_t)size + 1);
    if (fread(src, 1, (size_t)size, f) != (size_t)size) die("gagal membaca file: %s", path);
    fclose(f);
    src[size] = 0;

    return lex_scan(src, path, 1, ntoks);
}

Tok *lex_text(const char *src, const char *fname, int startline, int *ntoks) {
    return lex_scan((char *)src, fname, startline, ntoks);
}
