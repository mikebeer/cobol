/* c40_expr.c -- operands, references, arithmetic, conditions */

enum { K_FIELD, K_NLIT, K_SLIT, K_FIG, K_ALL, K_DEC, K_NULL };

typedef struct Opd Opd;
struct Opd {
    int k;
    char *c;            /* C expression: CobFld * or CobDec * */
    Item *it;           /* item, when a plain data reference */
    int fig;            /* FG_* for K_FIG and K_ALL of a figurative */
    Tok lit;            /* literal token */
    int neg;            /* negative numeric literal */
    int numeric;        /* known to be numeric */
    int whole;          /* an unsubscripted whole item */
    Item *cond;         /* a condition-name (level 88) */
    char *parent;       /* field expression of the conditioned item */
    char *uclass;       /* user class table name */
};

static int ntmp = 0, ndec = 0, ntmp_max = 0, ndec_max = 0;

static char *newT(void)
{
    ntmp++;
    if (ntmp > ntmp_max) ntmp_max = ntmp;
    return fmt("&T%d", ntmp);
}

static char *newD(void)
{
    ndec++;
    if (ndec > ndec_max) ndec_max = ndec;
    return fmt("&D%d", ndec);
}

static void tmp_reset(void)
{
    ntmp = 0;
    ndec = 0;
}

/* names of the intrinsic functions */
static const char *intrinsics[] = {
    "ABS", "ACOS", "ANNUITY", "ASIN", "ATAN", "BASECONVERT",
    "BOOLEAN-OF-INTEGER", "BYTE-LENGTH", "CHAR", "CONCATENATE",
    "COS", "CURRENT-DATE", "DATE-OF-INTEGER", "DATE-TO-YYYYMMDD",
    "DAY-OF-INTEGER", "DAY-TO-YYYYDDD", "E", "EXP", "EXP10",
    "FACTORIAL", "FORMATTED-CURRENT-DATE", "FORMATTED-DATE",
    "FORMATTED-DATETIME", "FORMATTED-TIME", "FRACTION-PART",
    "HEX-OF", "HIGHEST-ALGEBRAIC", "INTEGER", "INTEGER-OF-BOOLEAN",
    "INTEGER-OF-DATE", "INTEGER-OF-DAY", "INTEGER-OF-FORMATTED-DATE",
    "INTEGER-PART", "LENGTH", "LOG", "LOG10", "LOWER-CASE",
    "LOWEST-ALGEBRAIC", "MAX", "MEAN", "MEDIAN", "MIDRANGE", "MIN",
    "MOD", "NUMVAL", "NUMVAL-C", "NUMVAL-F", "ORD", "ORD-MAX",
    "ORD-MIN", "PI", "PRESENT-VALUE", "RANDOM", "RANGE", "REM",
    "REVERSE", "SECONDS-FROM-FORMATTED-TIME", "SECONDS-PAST-MIDNIGHT",
    "SIGN", "SIN", "SQRT", "STANDARD-DEVIATION", "STORED-CHAR-LENGTH",
    "SUBSTITUTE", "SUBSTITUTE-CASE", "SUM", "TAN",
    "TEST-DATE-YYYYMMDD", "TEST-DAY-YYYYDDD",
        "TEST-FORMATTED-DATETIME",
    "TEST-NUMVAL", "TEST-NUMVAL-C", "TEST-NUMVAL-F", "TRIM",
    "UPPER-CASE", "VARIANCE", "WHEN-COMPILED", "YEAR-TO-YYYY",
    "EXCEPTION-FILE", "EXCEPTION-LOCATION", "EXCEPTION-STATEMENT",
    "EXCEPTION-STATUS", "LOCALE-DATE", "LOCALE-TIME", "DISPLAY-OF",
    "NATIONAL-OF", "ULENGTH", "UPOS", "USUBSTR", "UUID4", "BIT-OF",
    "BIT-TO-CHAR", "CHAR-NATIONAL", "COMBINED-DATETIME",
    "TRIM-LEFT", "TRIM-RIGHT", "MODULE-NAME",
    "MODULE-ID", "NUMBER-OF-CALL-PARAMETERS", 0
};

static int is_intrinsic(const char *s)
{
    int i;
    for (i = 0; intrinsics[i]; i++)
        if (!strcmp(intrinsics[i], s)) return 1;
    return 0;
}

char *opd_fld(Opd *o);
static Opd p_aexpr(void);
static char *ie_expr(void);
static char *p_cond(void);

static int is_resv(const char *s);

/* find a user class by name */
static UCls *find_ucls(const char *n)
{
    UCls *c;
    for (c = uclasses; c; c = c->next)
        if (!strcmp(c->name, n)) return c;
    return 0;
}

/* integer expression as a C long expression */
static char *ie_primary(void)
{
    Tok *t = cur();
    if (t->k == T_NUM) {
        tp++;
        if (strchr(t->s, '.')) syn("integer expected");
        return fmt("%sL", t->s);
    }
    if (isop("(")) {
        char *e;
        tp++;
        e = ie_expr();
        needop(")");
        return fmt("(%s)", e);
    }
    if (isop("-")) {
        tp++;
        return fmt("(-%s)", ie_primary());
    }
    if (isop("+")) {
        tp++;
        return ie_primary();
    }
    {
        Opd o = p_aexpr();
        if (o.k == K_DEC) return fmt("cob_dtoi(%s)", o.c);
        if (o.k == K_NLIT) return fmt("%sL", o.lit.s);
        if (o.k == K_FIG) return "0L";
        return fmt("cob_toi(%s)", o.c);
    }
}

static char *ie_term(void)
{
    char *a = ie_primary();
    for (;;) {
        if (isop("*")) { tp++; a = fmt("(%s * %s)", a, ie_primary()); }
        else if (isop("/")) {
            tp++;
            a = fmt("cob_idiv(%s, %s)", a, ie_primary());
        } else break;
    }
    return a;
}

static char *ie_expr(void)
{
    char *a = ie_term();
    for (;;) {
        if (isop("+")) { tp++; a = fmt("(%s + %s)", a, ie_term()); }
        else if (isop("-")) { tp++; a = fmt("(%s - %s)", a,
            ie_term()); }
        else break;
    }
    return a;
}

/* ---- references ---- */

static Opd opd_lit(Tok *t, int neg)
{
    Opd o;
    memset(&o, 0, sizeof o);
    o.lit = *t;
    o.neg = neg;
    o.k = t->k == T_NUM ? K_NLIT : K_SLIT;
    o.numeric = t->k == T_NUM;
    o.c = cat2("&", lit_name(t, neg));
    return o;
}

static char *fig_lit(int fig)
{
    Tok t;
    char b[2];
    memset(&t, 0, sizeof t);
    t.k = T_STR;
    b[0] = fig == FG_ZERO ? '0' : fig == FG_SPACE ? ' ' :
           fig == FG_HIGH ? (char)0xFF : fig == FG_LOW ? 0 : '"';
    b[1] = 0;
    t.s = xdupn(b, 1);
    t.len = 1;
    return cat2("&", lit_name(&t, 0));
}

static Opd opd_fig(int fig, int all)
{
    Opd o;
    memset(&o, 0, sizeof o);
    o.k = all ? K_ALL : K_FIG;
    o.fig = fig;
    o.numeric = fig == FG_ZERO;
    o.c = fig_lit(fig);
    return o;
}

/* build the field expression of an item with subscripts */
static char *ref_expr(Item *it, char **subs, int nsubs)
{
    Item *d[16];
    int nd = item_dims(it, d), i;
    Sb off;
    char *t, *e;
    if (nd == 0) return fmt("&F%d", it->id);
    if (nsubs == 0) return fmt("&F%d", it->id);
    if (nsubs != nd)
        perr(cur()->file, cur()->line,
             "wrong number of subscripts for", it->name);
    memset(&off, 0, sizeof off);
    for (i = 0; i < nd; i++) {
        if (i) sb_s(&off, " + ");
        sb_s(&off, fmt("cob_ix(%s, %ld) * %ld", subs[i],
                       (long)d[i]->occmax, d[i]->size));
    }
    t = newT();
    e = fmt("cob_el(%s, &F%d, %s)", t, it->id, off.s);
    return e;
}

/* if the cursor is at a left parenthesis: subscripts or ref-mod */
static int paren_has_colon(void)
{
    long i = tp;
    int depth = 0;
    if (!isop("(")) return 0;
    for (; i < toks.n; i++) {
        Tok *t = &toks.a[i];
        if (t->k == T_OP && !strcmp(t->s, "(")) depth++;
        else if (t->k == T_OP && !strcmp(t->s, ")")) {
            depth--;
            if (!depth) return 0;
        } else if (t->k == T_OP && !strcmp(t->s, ":") && depth == 1)
            return 1;
        else if (t->k == T_PERIOD || t->k == T_EOF) return 0;
    }
    return 0;
}

static char *p_refmod(char *base)
{
    char *a, *b = "-1L", *t;
    needop("(");
    a = ie_expr();
    needop(":");
    if (!isop(")")) b = ie_expr();
    needop(")");
    t = newT();
    return fmt("cob_rm(%s, %s, %s, %s)", t, base, a, b);
}

static int is_space_sep(void)
{
    return 1;
}

static Opd special_register(const char *n)
{
    Opd o;
    memset(&o, 0, sizeof o);
    if (!strcmp(n, "RETURN-CODE")) {
        o.k = K_FIELD;
        o.c = "&cob_return_code";
        o.numeric = 1;
        return o;
    }
    o.k = -1;
    return o;
}

/* identifier (data reference) */
static Opd p_ident(void)
{
    Tok *t = cur();
    char *quals[16];
    int nq = 0;
    char *subs[16];
    int ns = 0;
    Item *it;
    Opd o;
    char *c;
    memset(&o, 0, sizeof o);
    if (t->k != T_WORD) syn("data name expected");
    {
        Opd sp = special_register(t->s);
        if (sp.k != -1) {
            tp++;
            return sp;
        }
    }
    tp++;
    while ((isw("OF") || isw("IN")) && nq < 16) {
        tp++;
        quals[nq++] = get_name();
    }
    it = lookup(t->s, quals, nq);
    if (!it) perr(t->file, t->line, "unknown data name", t->s);
    (void)is_space_sep;
    if (it->level == 78) {
        Val *v = it->vals;
        if (v->kind == 1) return opd_fig(v->fig, 0);
        return opd_lit(&v->lo, v->neg);
    }
    /* subscripts */
    while (isop("(") && !paren_has_colon()) {
        tp++;
        for (;;) {
            if (ns >= 16) syn("too many subscripts");
            subs[ns++] = ie_expr();
            if (isop(")")) break;
            /* relative subscripts are handled by ie_expr */
        }
        needop(")");
        if (isw("OF") || isw("IN")) break;
    }
    if (it->level == 88) {
        Item *par = it->parent;
        o.k = K_FIELD;
        o.cond = it;
        o.it = par;
        o.parent = ref_expr(par, subs, ns);
        o.c = o.parent;
        return o;
    }
    c = ref_expr(it, subs, ns);
    o.it = it;
    o.whole = ns == 0;
    o.k = K_FIELD;
    o.numeric = it->ctype >= CT_NUMDISP && it->ctype <= CT_DBL;
    if (it->ctype == CT_NUMEDIT) o.numeric = 1;
    if (isop("(") && paren_has_colon()) {
        c = p_refmod(c);
        o.numeric = 0;
        o.whole = 0;
    }
    o.c = c;
    return o;
}

static void dump_fn_args(Sb *a, int *n)
{
    *n = 0;
    if (!isop("(") || paren_has_colon()) return;
    tp++;
    while (!isop(")")) {
        Opd o = p_aexpr();
        sb_s(a, ", ");
        sb_s(a, opd_fld(&o));
        (*n)++;
        if (at_eof()) syn("unterminated argument list");
    }
    tp++;
}

char *opd_fld(Opd *o)
{
    switch (o->k) {
    case K_DEC: return fmt("cob_dtof(%s)", o->c);
    case K_NULL: return "&cob_null";
    default: return o->c;
    }
}

static int user_function_known(const char *n)
{
    (void)n;
    return 0;
}

static Opd p_function(void)
{
    Tok *t = cur();
    Sb a;
    int n;
    Opd o;
    char *name;
    memset(&a, 0, sizeof a);
    memset(&o, 0, sizeof o);
    name = get_name();
    if (!is_intrinsic(name)) {
        /* user-defined FUNCTION-ID in the repository */
        if (!user_function_known(name))
            perr(t->file, t->line, "unknown function", name);
        dump_fn_args(&a, &n);
        o.k = K_FIELD;
        o.c = fmt("cob_ufn(%s, %d%s)", c_lit_str(name), n,
                  sb_str(&a));
        return o;
    }
    dump_fn_args(&a, &n);
    o.k = K_FIELD;
    o.c = fmt("cob_fn(FN_%s, %d%s)", mangle(name), n, sb_str(&a));
    if (isop("(") && paren_has_colon()) o.c = p_refmod(o.c);
    {
        const char *numf[] = { "ABS", "ACOS", "ANNUITY", "ASIN",
            "ATAN", "BYTE-LENGTH", "COS", "DATE-OF-INTEGER", "E",
            "EXP", "EXP10", "FACTORIAL", "FRACTION-PART", "INTEGER",
            "INTEGER-OF-DATE", "INTEGER-OF-DAY", "INTEGER-PART",
            "LENGTH", "LOG", "LOG10", "MAX", "MEAN", "MEDIAN",
            "MIDRANGE", "MIN", "MOD", "NUMVAL", "NUMVAL-C",
            "NUMVAL-F", "ORD", "ORD-MAX", "ORD-MIN", "PI",
            "PRESENT-VALUE", "RANDOM", "RANGE", "REM", "SIGN", "SIN",
            "SQRT", "STANDARD-DEVIATION", "SUM", "TAN", "VARIANCE",
            "YEAR-TO-YYYY", "DATE-TO-YYYYMMDD", "DAY-TO-YYYYDDD",
            "DAY-OF-INTEGER", "SECONDS-PAST-MIDNIGHT",
            "STORED-CHAR-LENGTH", "TEST-DATE-YYYYMMDD",
            "TEST-DAY-YYYYDDD", "TEST-NUMVAL",
            "INTEGER-OF-FORMATTED-DATE", "ULENGTH", "UPOS", 0 };
        int i;
        for (i = 0; numf[i]; i++)
            if (!strcmp(numf[i], name)) o.numeric = 1;
    }
    return o;
}

/* operand: literal, figurative, identifier, function, ... */
static Opd p_operand(void)
{
    Tok *t = cur();
    Opd o;
    memset(&o, 0, sizeof o);
    if (t->k == T_NUM) { tp++; return opd_lit(t, 0); }
    if (t->k == T_STR) { tp++; return opd_lit(t, 0); }
    if (t->k == T_OP && (!strcmp(t->s, "-") || !strcmp(t->s, "+")) &&
        peek(1)->k == T_NUM && !peek(1)->sp) {
        int neg = t->s[0] == '-';
        tp++;
        t = cur();
        tp++;
        return opd_lit(t, neg);
    }
    if (t->k == T_WORD) {
        int f = fig_code(t->s);
        if (!strcmp(t->s, "NULL") || !strcmp(t->s, "NULLS")) {
            tp++;
            o.k = K_NULL;
            o.c = "&cob_null";
            return o;
        }
        if (f) { tp++; return opd_fig(f, 0); }
        if (!strcmp(t->s, "ALL")) {
            tp++;
            t = cur();
            if (t->k == T_STR || t->k == T_NUM) {
                tp++;
                o = opd_lit(t, 0);
                o.k = K_ALL;
                return o;
            }
            if (t->k == T_WORD && fig_code(t->s)) {
                tp++;
                return opd_fig(fig_code(t->s), 1);
            }
            syn("literal expected after ALL");
        }
        if (!strcmp(t->s, "FUNCTION")) {
            tp++;
            return p_function();
        }
        if (!strcmp(t->s, "ADDRESS") && isw_at(1, "OF")) {
            Opd x;
            tp += 2;
            x = p_ident();
            o.k = K_FIELD;
            o.c = fmt("cob_addr(%s, %s)", newT(), x.c);
            return o;
        }
        if (!strcmp(t->s, "LENGTH") && isw_at(1, "OF")) {
            Opd x;
            tp += 2;
            x = p_ident();
            o.k = K_DEC;
            o.numeric = 1;
            o.c = fmt("cdx_int(%s, (%s)->size)", newD(), x.c);
            return o;
        }
        if (!strcmp(t->s, "NUMBER-OF-CALL-PARAMETERS")) {
            tp++;
            o.k = K_DEC;
            o.numeric = 1;
            o.c = fmt("cdx_int(%s, (long)cob_na)", newD());
            return o;
        }
        if (!strcmp(t->s, "SELF") || !strcmp(t->s, "SUPER")) {
            tp++;
            o.k = K_FIELD;
            o.c = !strcmp(t->s, "SELF") ? "&cob_self" : "&cob_super";
            return o;
        }
        if (is_intrinsic(t->s) && peek(1)->k == T_OP &&
            !strcmp(peek(1)->s, "(") && !lookup(t->s, 0, 0))
            return p_function();
        return p_ident();
    }
    syn("operand expected");
    return o;
}

/* ---- arithmetic ---- */

static char *opd_dec(Opd *o)
{
    switch (o->k) {
    case K_DEC: return o->c;
    case K_NLIT: {
        char *s = o->lit.s;
        char *d = newD();
        char *n = o->neg ? cat2("-", s) : s;
        if (strchr(s, 'E') || strchr(s, 'e'))
            return fmt("cdx_ld(%s, %s)", d, o->c);
        return fmt("cdx_lit(%s, \"%s\")", d, n);
    }
    case K_FIG:
        if (o->fig == FG_ZERO) return fmt("cdx_int(%s, 0)", newD());
        return fmt("cdx_ld(%s, %s)", newD(), o->c);
    default:
        return fmt("cdx_ld(%s, %s)", newD(), opd_fld(o));
    }
}

static Opd p_apow(void);

static Opd p_aprimary(void)
{
    if (isop("(")) {
        Opd o;
        tp++;
        o = p_aexpr();
        needop(")");
        return o;
    }
    return p_operand();
}

static Opd mkdec(char *c)
{
    Opd o;
    memset(&o, 0, sizeof o);
    o.k = K_DEC;
    o.numeric = 1;
    o.c = c;
    return o;
}

static Opd p_aunary(void)
{
    if (isop("-") && !(peek(1)->k == T_NUM && !peek(1)->sp)) {
        Opd a;
        tp++;
        a = p_apow();
        return mkdec(fmt("cdx_neg(%s, %s)", newD(), opd_dec(&a)));
    }
    if (isop("+") && !(peek(1)->k == T_NUM && !peek(1)->sp)) {
        tp++;
        return p_apow();
    }
    return p_aprimary();
}

static Opd p_apow(void)
{
    Opd a = p_aunary();
    if (isop("**")) {
        Opd b;
        char *da;
        tp++;
        da = opd_dec(&a);
        b = p_apow();
        return mkdec(fmt("cdx_pow(%s, %s, %s)", newD(), da,
                         opd_dec(&b)));
    }
    return a;
}

static Opd p_aterm(void)
{
    Opd a = p_apow();
    while (isop("*") || isop("/")) {
        int mul = isop("*");
        Opd b;
        char *da;
        tp++;
        da = opd_dec(&a);
        b = p_apow();
        a = mkdec(fmt(mul ? "cdx_mul(%s, %s, %s)" :
                      "cdx_div(%s, %s, %s)", newD(), da, opd_dec(&b)));
    }
    return a;
}

static Opd p_aexpr(void)
{
    Opd a = p_aterm();
    while ((isop("+") || isop("-")) && peek(1)->sp + 0 >= 0) {
        int add = isop("+");
        Opd b;
        char *da;
        tp++;
        da = opd_dec(&a);
        b = p_aterm();
        a = mkdec(fmt(add ? "cdx_add(%s, %s, %s)" :
                      "cdx_sub(%s, %s, %s)", newD(), da, opd_dec(&b)));
    }
    return a;
}

/* ---- conditions ---- */

static const char *relop_c(int op)
{
    static const char *t[] = { "==", "!=", "<", ">", "<=", ">=" };
    return t[op];
}

/* returns -1 when no relational operator is at the cursor.  Handles
   IS [NOT] EQUAL TO, GREATER THAN, ... and symbols.  Sets *neg. */
static int p_relop(int *consumed_not)
{
    long save = tp;
    int neg = 0, op = -1;
    *consumed_not = 0;
    if (isw("IS")) tp++;
    if (isw("NOT")) { neg = 1; tp++; }
    if (isop("=")) { op = 0; tp++; }
    else if (isop("<>")) { op = 1; tp++; }
    else if (isop("<=")) { op = 4; tp++; }
    else if (isop(">=")) { op = 5; tp++; }
    else if (isop("<")) { op = 2; tp++; }
    else if (isop(">")) { op = 3; tp++; }
    else if (isw("EQUAL")) {
        tp++; acc("TO"); op = 0;
    } else if (isw("GREATER")) {
        tp++; acc("THAN");
        if (acc("OR")) { need("EQUAL"); acc("TO"); op = 5; }
        else op = 3;
    } else if (isw("LESS")) {
        tp++; acc("THAN");
        if (acc("OR")) { need("EQUAL"); acc("TO"); op = 4; }
        else op = 2;
    } else if (isw("EQUALS")) { tp++; op = 0; }
    else if (isw("GREATER")) { tp++; op = 3; }
    if (op < 0) { tp = save; return -1; }
    if (neg) {
        static const int inv[] = { 1, 0, 5, 4, 3, 2 };
        op = inv[op];
        *consumed_not = 1;
    }
    return op;
}

static int cmp_flip(int op)
{
    static const int f[] = { 0, 1, 3, 2, 5, 4 };
    return f[op];
}

static char *gen_cmp(Opd *a, int op, Opd *b)
{
    const char *o = relop_c(op);
    if (a->k == K_DEC || b->k == K_DEC)
        return fmt("(cob_cmpd(%s, %s) %s 0)", opd_dec(a), opd_dec(b),
            o);
    if (a->k == K_FIG && b->k != K_FIG && b->k != K_ALL)
        return fmt("(cob_cmp_fig(%s, %d) %s 0)", opd_fld(b), a->fig,
                   relop_c(cmp_flip(op)));
    if (b->k == K_FIG)
        return fmt("(cob_cmp_fig(%s, %d) %s 0)", opd_fld(a), b->fig,
            o);
    if (b->k == K_ALL)
        return fmt("(cob_cmp_all(%s, %s) %s 0)", opd_fld(a), b->c, o);
    if (a->k == K_ALL)
        return fmt("(cob_cmp_all(%s, %s) %s 0)", opd_fld(b), a->c,
                   relop_c(cmp_flip(op)));
    return fmt("(cob_cmp(%s, %s) %s 0)", opd_fld(a), opd_fld(b), o);
}

/* condition-name test */
static char *gen_cond88(Opd *o)
{
    Item *c = o->cond;
    Val *v;
    Sb b;
    memset(&b, 0, sizeof b);
    sb_c(&b, '(');
    for (v = c->vals; v; v = v->next) {
        char *part;
        if (v->kind == 1)
            part = fmt("cob_cmp_fig(%s, %d) == 0", o->parent, v->fig);
        else if (v->kind == 2)
            part = fmt("cob_cmp_all(%s, &%s) == 0", o->parent,
                       lit_name(&v->lo, 0));
        else if (v->hasthru) {
            part = fmt(
                    "(cob_cmp(%s, &%s) >= 0 && cob_cmp(%s, &%s) <= 0)",
                       o->parent, lit_name(&v->lo, v->neg), o->parent,
                       lit_name(&v->hi, 0));
        } else
            part = fmt("cob_cmp(%s, &%s) == 0", o->parent,
                       lit_name(&v->lo, v->neg));
        if (v != c->vals) sb_s(&b, " || ");
        sb_s(&b, part);
    }
    if (!c->vals) sb_s(&b, "0");
    sb_c(&b, ')');
    return sb_str(&b);
}

/* does a class or sign keyword follow? */
static char *p_class_or_sign(Opd *a)
{
    long save = tp;
    int neg = 0;
    char *r = 0;
    if (isw("IS")) tp++;
    if (isw("NOT")) { neg = 1; tp++; }
    if (isw("NUMERIC")) {
        tp++;
        r = fmt("cob_is_numeric(%s)", opd_fld(a));
    } else if (isw("ALPHABETIC")) {
        tp++;
        r = fmt("cob_is_alpha(%s)", opd_fld(a));
    } else if (isw("ALPHABETIC-LOWER")) {
        tp++;
        r = fmt("cob_is_lower(%s)", opd_fld(a));
    } else if (isw("ALPHABETIC-UPPER")) {
        tp++;
        r = fmt("cob_is_upper(%s)", opd_fld(a));
    } else if (isw("POSITIVE")) {
        tp++;
        r = a->k == K_DEC ? fmt("(cob_dsign(%s) > 0)", a->c) :
                            fmt("(cob_sign(%s) > 0)", opd_fld(a));
    } else if (isw("NEGATIVE")) {
        tp++;
        r = a->k == K_DEC ? fmt("(cob_dsign(%s) < 0)", a->c) :
                            fmt("(cob_sign(%s) < 0)", opd_fld(a));
    } else if (isw("ZERO") && 0) {
        tp++;
    } else if (isw("OMITTED")) {
        tp++;
        r = fmt("(%s == 0)", opd_fld(a));
    } else if (isw("BOOLEAN")) {
        tp++;
        r = fmt("cob_is_numeric(%s)", opd_fld(a));
    } else if (cur()->k == T_WORD && find_ucls(cur()->s)) {
        UCls *c = find_ucls(cur()->s);
        tp++;
        r = fmt("cob_inclass(%s, UCL%d)", opd_fld(a), c->id);
    }
    if (!r) { tp = save; return 0; }
    return neg ? fmt("(!%s)", r) : r;
}

static int starts_operand(void)
{
    Tok *t = cur();
    if (t->k == T_NUM || t->k == T_STR) return 1;
    if (t->k == T_OP) return !strcmp(t->s, "(") || !strcmp(t->s, "-")
        ||
                             !strcmp(t->s, "+");
    if (t->k == T_WORD) {
        if (!strcmp(t->s, "NOT") || !strcmp(t->s, "AND") ||
            !strcmp(t->s, "OR")) return 0;
        return !is_resv(t->s) || fig_code(t->s) ||
               !strcmp(t->s, "ALL") || !strcmp(t->s, "FUNCTION") ||
               !strcmp(t->s, "LENGTH") || !strcmp(t->s, "ADDRESS") ||
               !strcmp(t->s, "NULL") || !strcmp(t->s, "SELF");
    }
    return 0;
}

/* is the '(' at the cursor the start of a parenthesized condition? */
static int paren_is_cond(void)
{
    long i = tp;
    int depth = 0;
    for (; i < toks.n; i++) {
        Tok *t = &toks.a[i];
        if (t->k == T_OP && !strcmp(t->s, "(")) depth++;
        else if (t->k == T_OP && !strcmp(t->s, ")")) {
            depth--;
            if (!depth) return 0;
        } else if (depth == 1) {
            if (t->k == T_OP && (!strcmp(t->s, "=") ||
                !strcmp(t->s, "<") || !strcmp(t->s, ">") ||
                !strcmp(t->s, "<=") || !strcmp(t->s, ">=") ||
                !strcmp(t->s, "<>"))) return 1;
            if (t->k == T_WORD && (!strcmp(t->s, "AND") ||
                !strcmp(t->s, "OR") || !strcmp(t->s, "NOT") ||
                !strcmp(t->s, "NUMERIC") || !strcmp(t->s, "IS") ||
                !strcmp(t->s, "EQUAL") || !strcmp(t->s, "GREATER") ||
                !strcmp(t->s, "LESS") || !strcmp(t->s, "POSITIVE") ||
                !strcmp(t->s, "NEGATIVE") ||
                !strcmp(t->s, "ALPHABETIC"))) return 1;
        }
        if (t->k == T_PERIOD || t->k == T_EOF) return 0;
    }
    return 0;
}

typedef struct {
    Opd subj;
    int op;
    int have;
} Rel;


/* a relation: subject op object, with abbreviations */
static char *p_relation(Rel *rel)
{
    Opd a, b;
    int op, nt;
    char *r;
    if (isop("(") && paren_is_cond()) {
        char *c;
        tp++;
        c = p_cond();
        needop(")");
        return fmt("(%s)", c);
    }
    a = p_aexpr();
    if (a.cond) {
        rel->have = 0;
        return gen_cond88(&a);
    }
    r = p_class_or_sign(&a);
    if (r) {
        rel->have = 0;
        return r;
    }
    op = p_relop(&nt);
    if (op < 0) {
        /* a bare data-name that is a switch or a boolean */
        {
            Opd z = a;
            if (z.k == K_FIELD) {
                rel->have = 0;
                return fmt("cob_is_true(%s)", opd_fld(&z));
            }
        }
        syn("relational operator expected");
    }
    b = p_aexpr();
    rel->subj = a;
    rel->op = op;
    rel->have = 1;
    return gen_cmp(&a, op, &b);
}

static char *p_not_cond(Rel *rel)
{
    if (isw("NOT")) {
        char *c;
        tp++;
        c = p_not_cond(rel);
        return fmt("(!%s)", c);
    }
    return p_relation(rel);
}

static char *p_and_cond(Rel *rel)
{
    char *a = p_not_cond(rel);
    while (isw("AND")) {
        long save = tp;
        char *b;
        tp++;
        /* abbreviated relation? */
        if (rel->have) {
            int nt, op;
            long s2 = tp;
            op = p_relop(&nt);
            if (op >= 0) {
                Opd o = p_aexpr();
                b = gen_cmp(&rel->subj, op, &o);
                a = fmt("(%s && %s)", a, b);
                continue;
            }
            tp = s2;
            if (starts_operand() && !isw("NOT")) {
                Opd o;
                char *c;
                long s3 = tp;
                int nt2;
                o = p_aexpr();
                if (p_relop(&nt2) < 0 && !p_class_or_sign(&o) &&
                    !o.cond) {
                    b = gen_cmp(&rel->subj, rel->op, &o);
                    a = fmt("(%s && %s)", a, b);
                    continue;
                }
                tp = s3;
                (void)c;
            }
        }
        (void)save;
        b = p_not_cond(rel);
        a = fmt("(%s && %s)", a, b);
    }
    return a;
}

static char *p_cond(void)
{
    Rel rel;
    char *a;
    memset(&rel, 0, sizeof rel);
    a = p_and_cond(&rel);
    while (isw("OR")) {
        char *b;
        tp++;
        if (rel.have) {
            int nt, op;
            long s2 = tp;
            op = p_relop(&nt);
            if (op >= 0) {
                Opd o = p_aexpr();
                b = gen_cmp(&rel.subj, op, &o);
                a = fmt("(%s || %s)", a, b);
                continue;
            }
            tp = s2;
            if (starts_operand() && !isw("NOT")) {
                Opd o;
                long s3 = tp;
                int nt2;
                o = p_aexpr();
                if (p_relop(&nt2) < 0 && !p_class_or_sign(&o) &&
                    !o.cond) {
                    b = gen_cmp(&rel.subj, rel.op, &o);
                    a = fmt("(%s || %s)", a, b);
                    continue;
                }
                tp = s3;
            }
        }
        b = p_and_cond(&rel);
        a = fmt("(%s || %s)", a, b);
    }
    return a;
}
