#ifndef AST_H
#define AST_H

typedef enum { T_INT, T_BOOL, T_VOID, T_STR, T_FN, T_ARI } Ty;

/* Node kinds */
enum {
    /* expressions */
    EX_INT, EX_BOOL, EX_VAR, EX_CALL, EX_UN, EX_BIN,
    EX_ARROW,      /* lambda: c=params, a=expr body / d=stmt body, cp=Method* */
    EX_FCALL,      /* pemanggilan lewat closure: a=args, b=callee expr    */
    EX_ENVLOAD,    /* baca capture ke-j dari env: ival=j, dty=tipe capture */
    EX_NEWARR,     /* new int[n]: a=size expr, dty=T_ARI                  */
    EX_IDX,        /* a[i]  : a=array expr, b=index expr                  */
    EX_LEN,        /* a.length : a=array expr                             */
    EX_NET,        /* builtin Net.* : ival=idx builtin, a=args            */
    EX_STRLIT,     /* string literal dalam ekspresi (khusus Net.bytes)    */
    /* statements */
    ST_VAR, ST_ASSIGN, ST_IF, ST_WHILE, ST_FOR,
    ST_BRK, ST_CONT, ST_RETURN, ST_PRINT, ST_EXPR,
    ST_IDXASG,     /* a[i] = v : a=array expr, b=index expr, c=value expr */
    ST_BLOCK,      /* blok { ... } : d=statement list (scope sendiri)      */
};

typedef struct Node {
    int kind;
    const char *str;   /* nama / operator / string literal */
    long ival;         /* nilai literal; slot ter resolusi / scratch codegen */
    long cp;           /* index cpool (ST_PRINT string atau grep codegen) */
    int dty;           /* tipe deklarasi ST_VAR */
    int line;
    const char *file;  /* file sumber AST — untuk laporan error */
    struct Node *a, *b, *c, *d;   /* anak generik */
    struct Node *next;        /* penghubung list statement */
} Node;

typedef struct Param {
    const char *name;
    int ty;            /* tipe deklarasi (T_INT default; T_FN untuk fn) */
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
    struct Method *next;
} Method;

typedef struct Class {
    const char *name;
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
