/* c28_par.c -- token cursor, global compiler state, symbol lookup */

static TV toks;
static long tp = 0;
static Prog *P = 0;             /* the program being compiled */
static Prog *progs = 0, *lastprog = 0;
static int idctr = 0;           /* unique numbers for C names */
static char *srcname = "";
static Sb out_lit;              /* file-level literal declarations */
static int opt_check = 0;       /* -check: subscript checking */
static int opt_binsize = 24;    /* 24: 2-4-8, 12: 1-2-4-8 */

static Tok *cur(void) { return &toks.a[tp]; }

static Tok *peek(int k)
{
    long i = tp + k;
    if (i >= toks.n) i = toks.n - 1;
    return &toks.a[i];
}

static void syn(const char *m)
{
    Tok *t = cur();
    perr(t->file, t->line, m, t->k == T_EOF ? "at end of file" :
         cat3("near '", t->s, "'"));
}

static int at_eof(void) { return cur()->k == T_EOF; }
static int at_period(void) { return cur()->k == T_PERIOD; }

static int isw(const char *w)
{
    Tok *t = cur();
    return t->k == T_WORD && !strcmp(t->s, w);
}

static int isw_at(int k, const char *w)
{
    Tok *t = peek(k);
    return t->k == T_WORD && !strcmp(t->s, w);
}

static int isop(const char *o)
{
    Tok *t = cur();
    return t->k == T_OP && !strcmp(t->s, o);
}

static int acc(const char *w)
{
    if (isw(w)) { tp++; return 1; }
    return 0;
}

static int accop(const char *o)
{
    if (isop(o)) { tp++; return 1; }
    return 0;
}

static void need(const char *w)
{
    if (!acc(w)) syn(cat3("expected ", w, ""));
}

static void needop(const char *o)
{
    if (!accop(o)) syn(cat3("expected ", o, ""));
}

static void acc_period(void)
{
    if (at_period()) tp++;
}

static void need_period(void)
{
    if (!at_period()) syn("expected '.'");
    tp++;
}

/* skip optional filler words IS, ARE, THE, ... */
static void skip_is(void)
{
    if (isw("IS") || isw("ARE")) tp++;
}

static char *get_name(void)
{
    Tok *t = cur();
    if (t->k != T_WORD && t->k != T_NUM && t->k != T_STR)
        syn("name expected");
    tp++;
    return t->s;
}

static long need_int(void)
{
    Tok *t = cur();
    long v;
    if (t->k != T_NUM) syn("integer expected");
    v = atol(t->s);
    tp++;
    return v;
}

/* skip tokens up to and including the next period */
static void skip_sentence(void)
{
    while (!at_eof() && !at_period()) tp++;
    acc_period();
}

/* --- symbol table --- */

#define HASHN 2048
static Item *hashtab[HASHN];

static unsigned hashs(const char *s)
{
    unsigned h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % HASHN;
}

static Item *new_item(void)
{
    Item *it = (Item *)xalloc(sizeof(Item));
    it->id = ++idctr;
    it->prog = P;
    it->line = cur()->line;
    return it;
}

static void item_link(Item *it)
{
    if (P->last) P->last->inext = it;
    else P->items = it;
    P->last = it;
    if (it->name && !it->filler) {
        unsigned h = hashs(it->name);
        it->hnext = hashtab[h];
        hashtab[h] = it;
    }
}

/* does it have the qualifier chain q[0] (innermost) ...? */
static int has_qual(Item *it, char **q, int nq)
{
    Item *p = it->parent;
    int i;
    for (i = 0; i < nq; i++) {
        while (p && (!p->name || strcmp(p->name, q[i]))) p = p->parent;
        if (!p) return 0;
        p = p->parent;
    }
    return 1;
}

static Item *lookup_in(Prog *pr, const char *name, char **q, int nq,
                       int *amb, int globalonly)
{
    Item *it, *found = 0;
    for (it = hashtab[hashs(name)]; it; it = it->hnext) {
        if (strcmp(it->name, name)) continue;
        if (it->prog != pr) continue;
        if (globalonly && !(it->global || (it->rec
            && it->rec->global)))
            continue;
        if (!has_qual(it, q, nq)) continue;
        if (found) { *amb = 1; }
        else found = it;
    }
    return found;
}

static Item *lookup(const char *name, char **q, int nq)
{
    Prog *pr;
    int amb = 0, go = 0;
    for (pr = P; pr; pr = pr->outer) {
        Item *it = lookup_in(pr, name, q, nq, &amb, go);
        if (it) {
            if (amb) perr(cur()->file, cur()->line,
                          "ambiguous reference to", name);
            return it;
        }
        go = 1;
    }
    return 0;
}

/* --- literal pool --- */

typedef struct Lit Lit;
struct Lit {
    int kind;                   /* 0 alnum, 1 numeric, 2 national,
                                   3 boolean */
    int fl;
    char *data;
    long len;
    int digits, scale, neg;
    int id;
    Lit *next;
};
static Lit *lits = 0;

static void c_str(Sb *b, const char *s, long n)
{
    long i;
    sb_c(b, '"');
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') sb_s(b, "\\\"");
        else if (c == '\\') sb_s(b, "\\\\");
        else if (c == '?') sb_s(b, "\\?");
        else if (c >= 32 && c < 127) sb_c(b, c);
        else {
            char t[8];
            sprintf(t, "\\%03o", c);
            sb_s(b, t);
        }
    }
    sb_c(b, '"');
}


static char *c_lit_str(const char *s)
{
    Sb b;
    memset(&b, 0, sizeof b);
    c_str(&b, s, (long)strlen(s));
    return sb_str(&b);
}

/* literal field name for a token (string or number); negative allows
   a numeric literal with a sign */
static char *lit_name(Tok *t, int neg)
{
    Lit *l;
    int kind, fl = t->fl, digits = 0, scale = 0;
    char *data;
    long len;
    char *dot;
    Sb b;
    if (t->k == T_NUM) {
        kind = 1;
        dot = strchr(t->s, '.');
        if (strchr(t->s, 'E') || strchr(t->s, 'e')) {
            /* floating literal: convert to a plain decimal */
            char buf[80];
            double v = atof(t->s);
            long i;
            sprintf(buf, "%.15f", v);
            for (i = (long)strlen(buf) - 1; i > 0 && buf[i] == '0';
                i--)
                buf[i] = 0;
            data = xdup(buf);
            dot = strchr(data, '.');
        } else data = t->s;
        memset(&b, 0, sizeof b);
        {
            const char *p;
            for (p = data; *p; p++) {
                if (*p == '.') continue;
                sb_c(&b, *p);
            }
            if (dot) scale = (int)strlen(dot + 1);
            if (dot && !dot[1]) scale = 0;
        }
        digits = (int)b.n;
        data = b.s;
        len = b.n;
        fl = 0;
    } else {
        kind = (t->fl & LF_NAT) ? 2 : (t->fl & LF_BOOL) ? 3 : 0;
        data = t->s;
        len = t->len;
    }
    for (l = lits; l; l = l->next)
        if (l->kind == kind && l->len == len && l->fl == fl &&
            l->neg == neg && l->scale == scale &&
            !memcmp(l->data, data, (size_t)len)) return
            cat2("K", itoa_(l->id));
    l = (Lit *)xalloc(sizeof(Lit));
    l->kind = kind;
    l->fl = fl;
    l->data = data;
    l->len = len;
    l->digits = digits;
    l->scale = scale;
    l->neg = neg;
    l->id = ++idctr;
    l->next = lits;
    lits = l;
    {
        char *n = itoa_(l->id);
        Sb *o = &out_lit;
        if (kind == 1) {
            sb_s(o, "static unsigned char KD");
            sb_s(o, n);
            sb_s(o, "[] = ");
            if (neg) {
                Sb s2;
                memset(&s2, 0, sizeof s2);
                sb_c(&s2, '-');
                sb_m(&s2, data, len);
                c_str(o, s2.s, s2.n);
                len++;
            } else c_str(o, data, len);
            sb_s(o, ";\nstatic const CobAttr KA");
            sb_s(o, n);
            sb_s(o, fmt(" = { CT_NUMDISP, %d, %d, %d, 0 };\n",
                        digits, scale,
                        neg ? (CF_SIGNED | CF_SEP | CF_LEAD | CF_LIT)
                            : CF_LIT));
        } else {
            int ty = kind == 2 ? CT_NAT : kind == 3 ? CT_BOOL
                                                    : CT_ALNUM;
            long dl = len;
            sb_s(o, "static unsigned char KD");
            sb_s(o, n);
            sb_s(o, "[] = ");
            if (kind == 2) {
                Sb w;
                long i;
                memset(&w, 0, sizeof w);
                if (fl & LF_HEX) {
                    for (i = 0; i + 1 < len; i += 2) {
                        sb_c(&w, data[i]);
                        sb_c(&w, data[i + 1]);
                    }
                } else {
                    for (i = 0; i < len; i++) {
                        sb_c(&w, 0);
                        sb_c(&w, data[i]);
                    }
                }
                c_str(o, w.s, w.n);
                dl = w.n;
            } else c_str(o, data, len);
            sb_s(o, ";\nstatic const CobAttr KA");
            sb_s(o, n);
            sb_s(o, fmt(" = { %d, 0, 0, %d, 0 };\n", ty, CF_LIT));
            len = dl;
        }
        sb_s(o, "static CobFld K");
        sb_s(o, n);
        sb_s(o, fmt(" = { %ld, KD%s, &KA%s };\n", len, n, n));
    }
    return cat2("K", itoa_(l->id));
}
