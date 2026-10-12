/* c00_defs.h -- types of the COBOL translator */

typedef struct Item Item;
typedef struct Val Val;
typedef struct Prog Prog;
typedef struct Fil Fil;

/* tokens */
enum { T_EOF, T_WORD, T_NUM, T_STR, T_OP, T_PERIOD, T_PSEUDO };
#define LF_HEX 1
#define LF_NAT 2
#define LF_BOOL 4
#define LF_ZSTR 8

typedef struct {
    int k;
    char *s;            /* WORD: upper case; others: text */
    char *raw;          /* WORD: as written */
    long len;           /* STR: length (may contain NUL) */
    int line;
    char *file;
    int sp;             /* preceded by white space */
    int fl;             /* literal flags */
    int rep;            /* produced by REPLACING */
} Tok;

typedef struct { Tok *a; long n, cap; } TV;

/* string builder */
typedef struct { char *s; long n, cap; } Sb;

/* usages */
enum { U_DISPLAY, U_COMP, U_COMP1, U_COMP2, U_COMP3, U_COMP5,
       U_INDEX, U_POINTER, U_OBJECT, U_NATIONAL, U_PROCPTR,
       U_FUNCPTR };

/* values of VALUE clauses and level 88 */
struct Val {
    int kind;           /* 0 literal, 1 figurative, 2 ALL literal */
    Tok lo, hi;         /* literal(s); hi.k set for THRU */
    int hasthru;
    int fig, neg;
    Val *next;
};

struct Item {
    char *name;
    int level, line, id;
    Item *parent, *child, *next, *lastchild;
    Item *redef;
    Item *rfrom, *rthru;        /* RENAMES */
    char *pic;                  /* expanded picture */
    int usage;
    int ctype, digits, scale, flags;
    long size;                  /* bytes of one occurrence */
    long offset;                /* from the start of the 01 record */
    int occmin, occmax;
    Item *odo;                  /* OCCURS DEPENDING ON */
    Item **idx;                 /* INDEXED BY names */
    int nidx;
    Val *vals;
    Item *conds;                /* level 88 children */
    Item *cnext;                /* next 88 of the same parent */
    int filler, external, global, based, isgroup, hasvalue;
    int sync, justified, blankzero, signsep, signlead, signed_;
    int storage;                /* 0 ws, 1 linkage, 2 local, 3 file,
                                   4 object, 5 factory */
    Item *rec;                  /* the 01 or 77 item */
    Fil *file;
    Prog *prog;
    char *cname;                /* C name of the field */
    char *objtype;              /* OBJECT REFERENCE class */
    long objoff;                /* offset in object/factory block */
    int emitted;
    int dims;                   /* number of enclosing OCCURS */
    Item *hnext;                /* hash chain */
    Item *rnext;                /* next record */
    Item *inext;                /* next item of the program */
    Item *master;               /* record owning the storage */
    long bufsize;               /* master: storage size */
    int isindex, usage_set, inredef, redefd;
    int cneg;                   /* level 78 / CONSTANT sign */
    char *pictxt;               /* raw picture text */
    int blen;                   /* bytes of one occurrence incl. slack
        */
    int dynocc, resolved;
    Val *falsev;
    char *keyn[8];              /* OCCURS KEY names */
    int keydir[8], nkeys;
    char *oddn;                 /* DEPENDING ON name */
    char *idxn[16];             /* INDEXED BY names */
};

struct Fil {
    char *name;
    char *assign;
    int org;                    /* 0 seq, 1 line seq, 2 relative,
                                   3 indexed */
    int access;                 /* 0 seq, 1 random, 2 dynamic */
    int optional, isort, id;
    Item *key, *relkey, *status;
    Item **akeys;
    int *adups;
    int nakeys, dupkey;
    Item *recs;                 /* first record description */
    Item *lastrec;
    long maxrec, minrec;
    Item *depends;
    Fil *next;
    char *cname;
    char *lockmode;
    int linage;
    Item *declarative;
    char *keyn, *relkeyn, *statusn, *akn[8];
    int akdup[8];
    char *dependn;
    int sel_line;
    int rectype;                /* 0 fixed, 1 variable */
    char *depn;                 /* DEPENDING ON name */
    struct Para *dp;            /* USE procedure for the file */
};

typedef struct Para Para;
struct Para {
    char *name;
    int id;
    int issec;
    Para *sec;                  /* section it belongs to */
    Para *next;
    int endid;
    int perfend;                /* used as the end of a PERFORM */
    int defined, line;
};

typedef struct Cls Cls;

struct Prog {
    char *name;
    char *cname;
    int id_;
    int initial, common, recursive;
    Prog *outer, *next;
    Item *items;                /* all named items in order */
    Item *last;
    Item *recs;                 /* the 01/77 records in order */
    Item *lastrec;
    Fil *files, *lastfile;
    Para *paras, *lastpara;
    Sb body, decl, bind, init;
    int ntmp, ndec, nlab, nloop, nperf;
    int nws, nloc;
    int hasusing;
    Item *using_[32];
    int nusing;
    char *retitem;
    int decimal_comma;
    char currency;
    Cls *cls;
    int ismethod;
    int isfunc;                 /* FUNCTION-ID */
    int iscls;                  /* CLASS-ID */
    int isfacm;                 /* a FACTORY method */
    char *base;                 /* parent class */
    char *clsname;
    long objsz, facsz;
    int nobj, nfac;
    struct { char *name; int id; int fac; } cm[96];
    int ncm;
    struct Para *dmode[4];      /* USE ... INPUT/OUTPUT/I-O/EXTEND */
};
