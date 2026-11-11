#ifndef AST_H
#define AST_H

typedef enum {
    T_INT, T_BOOL, T_VOID, T_STR, T_FN, T_ARI,
    /* tipe numerik tambahan (v1.6) */
    T_LONG, T_DOUBLE, T_FLOAT,
    T_UINT8, T_UINT16, T_UINT32, T_UINT64,
    T_BYTE, T_SHORT          /* 8/16-bit bertanda (akhir = nilai byte stabil) */
} Ty;

/* ── sifat tipe numerik ─────────────────────────────────────────── */

static inline int ty_is_float(Ty t) {
    return t == T_FLOAT || t == T_DOUBLE;
}

static inline int ty_is_uint(Ty t) {
    return t == T_UINT8 || t == T_UINT16 || t == T_UINT32 || t == T_UINT64;
}

/* semua integer bertanda/unsigned (bukan bool) */
static inline int ty_is_int(Ty t) {
    return t == T_INT || t == T_LONG || t == T_BYTE || t == T_SHORT || ty_is_uint(t);
}

static inline int ty_is_num(Ty t) {
    return ty_is_int(t) || ty_is_float(t);
}

/* lebar bit; 0 untuk non-numerik */
static inline int ty_width(Ty t) {
    switch (t) {
    case T_INT: case T_UINT32: return 32;
    case T_LONG: case T_UINT64: return 64;
    case T_UINT8: case T_BYTE: return 8;
    case T_UINT16: case T_SHORT: return 16;
    case T_FLOAT: return 32;
    case T_DOUBLE: return 64;
    default: return 0;
    }
}

/* peringkat promosi (makin besar makin lebar/umum).
 * Aturan Kopi: int + uint32 → int (signed menang pada lebar sama);
 * long + uint64 → long. Ini bukan aturan Java (Java tak punya unsigned),
 * didefinisikan eksplisit dan diuji. */
static inline int ty_rank(Ty t) {
    switch (t) {
    case T_UINT8: case T_BYTE: return 1;
    case T_UINT16: case T_SHORT: return 2;
    case T_UINT32: return 3;
    case T_INT:    return 4;
    case T_UINT64: return 5;
    case T_LONG:   return 6;
    case T_FLOAT:  return 7;
    case T_DOUBLE: return 8;
    default:       return 0;
    }
}

static inline Ty ty_promote(Ty a, Ty b) {
    if (ty_is_float(a) || ty_is_float(b))
        return ty_rank(a) >= ty_rank(b) ? a : b;
    if (!ty_is_int(a) || !ty_is_int(b)) return T_INT;
    int ua = ty_is_uint(a), ub = ty_is_uint(b);
    if (ua == ub)
        return ty_width(a) >= ty_width(b) ? a : b;
    /* campuran signed/unsigned: perlu tipe signed yang lebih lebar
     * (int+uint32 → long; uint64+long → long dengan reinterpretasi bit) */
    Ty sg = ua ? b : a;
    Ty un = ua ? a : b;
    if (ty_width(sg) > ty_width(un)) return sg;
    if (ty_width(un) < 32) return T_INT;
    return T_LONG;
}

/* boleh dikonversi implisit (pelebaran), selain itu butuh cast eksplisit */
static inline int ty_can_implicit(Ty dst, Ty src) {
    if (dst == src) return 1;
    /* String native dan int[] byte-array memakai representasi yang sama;
     * keduanya boleh saling diberikan (interop perpustakaan). */
    if ((dst == T_STR && src == T_ARI) || (dst == T_ARI && src == T_STR))
        return 1;
    if (!ty_is_num(dst) || !ty_is_num(src)) return 0;
    if (ty_is_float(dst)) {
        if (ty_is_float(src)) return ty_rank(dst) >= ty_rank(src);
        /* int apa pun → float/double: Java mengizinkan int→float/double,
         * long→float boleh (presisi hilang tetapi legal di Java) */
        return 1;
    }
    if (ty_is_float(src)) return 0;             /* butuh cast */
    /* dua-duanya integer */
    if (ty_is_uint(dst) == ty_is_uint(src))
        return ty_width(dst) >= ty_width(src);
    if (ty_is_uint(dst))                        /* signed → unsigned */
        return ty_width(dst) > ty_width(src) && src != T_LONG;
    /* unsigned → signed (lebar sama boleh: reinterpretasi bit) */
    return ty_width(dst) >= ty_width(src);
}

/* Node kinds */
enum {
    /* expressions */
    EX_INT, EX_BOOL, EX_VAR, EX_CALL, EX_UN, EX_BIN,
    EX_TERN,       /* cond ? a : b : a=cond, b=then, c=else */
    EX_ARROW,      /* lambda: c=params, a=expr body / d=stmt body, cp=Method* */
    EX_FCALL,      /* pemanggilan lewat closure: a=args, b=callee expr    */
    EX_ENVLOAD,    /* baca capture ke-j dari env: ival=j, dty=tipe capture */
    EX_NEWARR,     /* new int[n]: a=size expr, dty=T_ARI                  */
    EX_IDX,        /* a[i]  : a=array expr, b=index expr                  */
    EX_LEN,        /* a.length : a=array expr                             */
    EX_NET,        /* builtin Net.* : ival=idx builtin, a=args            */
    EX_STRLIT,     /* string literal dalam ekspresi (khusus Net.bytes)    */
    EX_CAST,       /* (tipe) expr : a=operand, dty=tipe tujuan            */
    EX_TSTR,       /* template `...{expr}...` : a=daftar bagian (byte/ekspresi) */
    EX_NEWOBJ,     /* new Kelas() : str=nama class → didesugar EX_NEWARR  */
    EX_FLD,        /* obj.field : a=objek, str=nama field → EX_IDX        */
    /* statements */
    ST_VAR, ST_ASSIGN, ST_IF, ST_WHILE, ST_FOR, ST_DO,
    ST_BRK, ST_CONT, ST_RETURN, ST_PRINT, ST_EXPR,
    ST_IDXASG,     /* a[i] = v : a=array expr, b=index expr, c=value expr */
    ST_FLDASG,     /* obj.field = v : a=objek, str=field, c=value expr    */
    ST_BLOCK,      /* blok { ... } : d=statement list (scope sendiri)      */
};

typedef struct Node {
    int kind;
    const char *str;   /* nama / operator / string literal */
    long ival;         /* nilai literal; slot ter resolusi / scratch codegen */
    double fval;       /* literal float/double atau nilai cast compile-time */
    long cp;           /* index cpool (ST_PRINT string atau grep codegen) */
    int dty;           /* tipe deklarasi / tipe hasil ekspresi */
    int typed;         /* sudah melalui type_expr (cache resolusi) */
    int line;
    const char *file;  /* file sumber AST — untuk laporan error */
    const char *cls;   /* ST_VAR/param: nama class bila deklarasi objek  */
    struct Method *meth; /* EX_CALL: metode target (untuk konversi argumen) */
    struct Node *a, *b, *c, *d;   /* anak generik */
    struct Node *next;        /* penghubung list statement */
} Node;

typedef struct Param {
    const char *name;
    int ty;            /* tipe deklarasi (T_INT default; T_FN untuk fn) */
    const char *cls;   /* tipe class (param `P p` → T_ARI + cls=P)      */
    struct Param *next;
} Param;

typedef struct Method {
    const char *cls, *name;
    Ty ret;
    Param *params;
    int nparams;
    Node *body;        /* statement list */
    int index;         /* method table index */
    int nlocals;       /* slots: params first, then locals */
    /* capture lambda (metode tersembunyi Lam_*): per capture j:
       captslot = slot frame UEE MENCIPTAKAN lambda (dibaca saat NEWCLO),
       capcopy  = slot salinan di frame metode ini (diisi ENVLOAD j),    */
    int ncapt;
    long captslot[32];
    int capcopy[32];
    Ty captty[32];
    int hidden;        /* metode lambda tersembunyi — jangan di-resolve ulang */
    int is_inst;       /* metode instance (param pertama = this)          */
    const char *retcls; /* tipe kembalian class (ret = T_ARI + retcls)    */
    struct Method *next;
} Method;

typedef struct Field {
    const char *name;
    int idx;           /* slot dalam tape instance */
    int ty;            /* 32-bit: T_INT/BOOL/BYTE/SHORT/UINT8/16; 64-bit: T_LONG/T_STR/T_ARI(class) */
    const char *cls;   /* tipe class (ty = T_ARI) */
    struct Field *next;
} Field;

typedef struct Class {
    const char *name;
    Field *fields;
    int nfields;
    struct Class *next;
} Class;

typedef struct {
    Class *classes;
    Method *methods;
    int nmethods;
    int mainidx;       /* index of entry method, -1 if none */
    const char **cpool;
    int ncp;
    const char *curcls; /* class being parsed */
} Prog;

extern Prog prog;
extern const char *g_linefile;   /* file token/AST yang sedang ditinjau — dipakai die() */

Node *newnode(int kind, int line);
void die(const char *fmt, ...);

#endif
