#ifndef LEX_H
#define LEX_H

/* Token types. Single-char operators use their ASCII code directly. */
enum {
    T_EOF = 0, T_NUM, TK_STR, T_ID,
    KW_CLASS, KW_PUBLIC, KW_PRIVATE, KW_STATIC, KW_FINAL,
    KW_INT, KW_BOOL, KW_VOID, KW_STRING,
    KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_RETURN,
    KW_TRUE, KW_FALSE, KW_BREAK, KW_CONTINUE, KW_IMPORT, KW_FN,
    T_AND, T_OR, T_EQ, T_NE, T_LE, T_GE,
    /* token multi-char; >= 256 agar tidak bertabrakan ASCII operator 1-char */
    T_PP = 256, T_MM, T_ADDEQ, T_SUBEQ, T_MULEQ, T_DIVEQ, T_MODEQ,
    T_ARROW,       /* => */
    /* operator bitwise (v1.5): single-char & | ^ ~ memakai kode ASCII-nya
     * langsung; multi-char di bawah >= 256 */
    T_SHL, T_SHR, T_USHR,               /* << >> >>> */
    T_ANDEQ, T_OREQ, T_XOREQ,           /* &= |= ^= */
    T_SHLEQ, T_SHREQ, T_USHREQ,         /* <<= >>= >>>= */
};

typedef struct {
    int type;
    char *text;   /* identifier text or unescaped string literal */
    long num;     /* T_NUM value */
    int line;
    const char *file;   /* file asal token — untuk laporan error */
} Tok;

Tok *lex_file(const char *path, int *ntoks);

#endif
