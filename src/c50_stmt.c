/* c50_stmt.c -- procedure division: control flow and arithmetic */

static Sb *O;                   /* current output */
static Para *cur_sec = 0, *cur_para = 0;
static int loop_brk[64], loop_cyc[64], nloops = 0;
static Sb retcases;
static int ns_id = 0, ns_used = 0;
static int nctr = 0;            /* TIMES counters */

static void em(const char *s) { sb_s(O, s); }

static const char *verbs[] = {
    "ACCEPT", "ADD", "ALLOCATE", "ALTER", "CALL", "CANCEL", "CLOSE",
    "COMPUTE", "CONTINUE", "DELETE", "DISPLAY", "DIVIDE", "EVALUATE",
    "EXIT", "FREE", "GENERATE", "GOBACK", "GO", "IF", "INITIALIZE",
    "INITIATE", "INSPECT", "INVOKE", "MERGE", "MOVE", "MULTIPLY",
    "NEXT", "OPEN", "PERFORM", "READ", "RELEASE", "RETURN", "REWRITE",
    "SEARCH", "SET", "SORT", "START", "STOP", "STRING", "SUBTRACT",
    "TERMINATE", "UNSTRING", "WRITE", "UNLOCK", "RAISE", "RESUME",
    "JSON", "XML", "VALIDATE", "ENTRY", "COMMIT", "ROLLBACK", "USE",
    "SUPPRESS", 0
};

static const char *phrase_words[] = {
    "TO", "FROM", "BY", "GIVING", "INTO", "ON", "SIZE", "ERROR", "NOT",
    "AT", "END", "INVALID", "KEY", "OVERFLOW", "EXCEPTION", "ELSE",
    "WHEN", "THEN", "UNTIL", "VARYING", "AFTER", "TIMES", "THRU",
    "THROUGH", "USING", "RETURNING", "REMAINDER", "ROUNDED", "ALSO",
    "OTHER", "WITH", "TEST", "BEFORE", "INITIAL", "TALLYING",
    "REPLACING", "CONVERTING", "FOR", "LEADING", "FIRST", "CHARACTERS",
    "DELIMITED", "DELIMITER", "COUNT", "IN", "POINTER", "OF", "IS",
    "EQUAL", "GREATER", "LESS", "THAN", "OR", "AND", "TRUE", "FALSE",
    "UPON", "ADVANCING", "LINE", "LINES", "PAGE", "INPUT", "OUTPUT",
    "I-O", "EXTEND", "DEPENDING", "CORRESPONDING", "CORR", "SECTION",
    "DIVISION", "RUN", "PROGRAM", "REFERENCE", "CONTENT", "VALUE",
    "OMITTED", "STATUS", "NEXT", "PREVIOUS", "RECORD", "RECORDS",
    "SEQUENTIAL", "RANDOM", "DYNAMIC", "ASCENDING", "DESCENDING",
    "ADDRESS", "AS", "IS", "GIVING", "SENTENCE", "TALLY", "ALL",
    "PROCEDURE", "RELATIVE", "ONLY", "SHARING", "NO", "REWIND",
    "LOCK", "FORWARD", "BACKWARD", "REVERSED", "FILE", "OPTIONAL",
    "COLLATING", "SEQUENCE", "DUPLICATES", "THEN", 0
};

static int is_verb(const char *s)
{
    int i;
    for (i = 0; verbs[i]; i++)
        if (!strcmp(verbs[i], s)) return 1;
    return 0;
}

static int is_resv(const char *s)
{
    int i;
    if (is_verb(s)) return 1;
    if (!strncmp(s, "END-", 4)) return 1;
    for (i = 0; phrase_words[i]; i++)
        if (!strcmp(phrase_words[i], s)) return 1;
    return 0;
}

static int stmt_start(void)
{
    Tok *t = cur();
    if (t->k != T_WORD) return 0;
    if (!strcmp(t->s, "NEXT") && !isw_at(1, "SENTENCE")) return 0;
    if (!strcmp(t->s, "USE")) return 0;
    return is_verb(t->s);
}

static void p_statement(void);

/* parse statements while a statement starts here; return the code */
static char *capture_stmts(void)
{
    Sb *save = O;
    Sb tmp;
    memset(&tmp, 0, sizeof tmp);
    O = &tmp;
    while (stmt_start()) p_statement();
    O = save;
    return tmp.s ? tmp.s : "";
}

/* ---- paragraphs ---- */

static Para *new_para(const char *name, int issec)
{
    Para *p = (Para *)xalloc(sizeof(Para));
    p->name = xdup(name);
    p->id = ++idctr;
    p->endid = ++idctr;
    p->issec = issec;
    p->line = cur()->line;
    if (P->lastpara) P->lastpara->next = p;
    else P->paras = p;
    P->lastpara = p;
    return p;
}

static Para *find_para(const char *name, const char *secname,
    int create)
{
    Para *p, *found = 0;
    for (p = P->paras; p; p = p->next) {
        if (strcmp(p->name, name)) continue;
        if (secname) {
            if (p->sec && !strcmp(p->sec->name, secname)) return p;
            continue;
        }
        if (!found || (p->sec == cur_sec && cur_sec)) found = p;
    }
    if (found || !create) return found;
    return new_para(name, 0);
}

static void em_end(Para *p)
{
    em(fmt("/*E%d*/\n", p->endid));
}

static void close_para(void)
{
    if (cur_para) em_end(cur_para);
    cur_para = 0;
}

static void close_section(void)
{
    close_para();
    if (cur_sec) em_end(cur_sec);
    cur_sec = 0;
}

static void begin_para(const char *name, int issec)
{
    Para *p, *q;
    if (issec) {
        close_section();
        p = 0;
        for (q = P->paras; q; q = q->next)
            if (!strcmp(q->name, name) && (q->issec || (!q->defined
                && !q->sec))) { p = q; break; }
        if (p && p->defined)
            perr(srcname, cur()->line, "duplicate section", name);
        if (!p) p = new_para(name, 1);
        p->issec = 1;
        p->defined = 1;
        cur_sec = p;
        em(fmt("L%d: ;\n", p->id));
        return;
    }
    close_para();
    p = 0;
    for (q = P->paras; q; q = q->next) {
        if (strcmp(q->name, name)) continue;
        if (q->defined && q->sec == cur_sec && !q->issec)
            perr(srcname, cur()->line, "duplicate paragraph", name);
        if (!q->defined && !q->sec && !q->issec) { p = q; break; }
    }
    if (!p) p = new_para(name, 0);
    p->defined = 1;
    p->sec = cur_sec;
    cur_para = p;
    em(fmt("L%d: ;\n", p->id));
}

/* target of GO TO / PERFORM */
static Para *p_paraname(void)
{
    char *n = get_name();
    char *sec = 0;
    Para *p;
    if (isw("OF") || isw("IN")) {
        tp++;
        sec = get_name();
    }
    p = find_para(n, sec, 1);
    if (!p->defined && !p->sec && !sec) {
        /* it may be a section defined later: leave as is */
    }
    return p;
}

/* ---- sentence labels for NEXT SENTENCE ---- */

static void sentence_start(void)
{
    ns_id = ++idctr;
    ns_used = 0;
}

static void sentence_end(void)
{
    if (ns_used) em(fmt("NS%d: ;\n", ns_id));
    ns_used = 0;
}

/* ---- scope terminators and conditional phrases ---- */

static void end_scope(const char *w)
{
    acc(w);
}

typedef struct { char *err, *ok; int has; } Hnd;

/* kind: 1 SIZE ERROR, 2 AT END, 3 INVALID KEY, 4 OVERFLOW,
   5 EXCEPTION, 6 END-OF-PAGE */
static int phrase_at(int kind, int *not_)
{
    long save = tp;
    int n = 0;
    *not_ = 0;
    if (isw("NOT")) { n = 1; tp++; }
    switch (kind) {
    case 1:
        acc("ON");
        if (isw("SIZE")) { tp += 2; *not_ = n; return 1; }
        break;
    case 2:
        if (isw("AT") && isw_at(1, "END")) { tp += 2; *not_ = n;
            return 1; }
        if (isw("END")) { tp++; *not_ = n; return 1; }
        break;
    case 3:
        if (isw("INVALID")) { tp++; acc("KEY"); *not_ = n; return 1; }
        break;
    case 4:
        acc("ON");
        if (isw("OVERFLOW")) { tp++; *not_ = n; return 1; }
        break;
    case 5:
        acc("ON");
        if (isw("EXCEPTION")) { tp++; *not_ = n; return 1; }
        break;
    case 6:
        if (isw("AT") && isw_at(1, "END-OF-PAGE")) {
            tp += 2; *not_ = n; return 1;
        }
        if (isw("END-OF-PAGE")) { tp++; *not_ = n; return 1; }
        break;
    }
    tp = save;
    return 0;
}

static Hnd p_handlers(int kind)
{
    Hnd h;
    int i, nt;
    memset(&h, 0, sizeof h);
    for (i = 0; i < 2; i++) {
        if (phrase_at(kind, &nt)) {
            char *c = capture_stmts();
            h.has = 1;
            if (nt) h.ok = c;
            else h.err = c;
        }
    }
    return h;
}

/* emit if (flag) {err} else {ok} */
static void em_handlers(Hnd *h, const char *flag)
{
    if (!h->has) return;
    em(fmt("if (%s) {\n", flag));
    if (h->err) em(h->err);
    em("} else {\n");
    if (h->ok) em(h->ok);
    em("}\n");
}

/* ---- simple statements ---- */

static const char *spec_target(const char *name)
{
    SN *s;
    for (s = specnames; s; s = s->next)
        if (s->kind == 1 && !strcmp(s->name, name)) return s->target;
    return 0;
}

static void p_display(void)
{
    char *ops[256];
    int n = 0, i, nl = 1, err = 0;
    tp++;
    while (!at_period() && !at_eof()) {
        Opd o;
        if (isw("UPON")) {
            const char *tg;
            tp++;
            tg = cur()->s;
            if (!strcmp(tg, "SYSERR") || !strcmp(tg, "STDERR"))
                err = 1;
            else {
                const char *m = spec_target(tg);
                if (m && (!strcmp(m, "SYSERR") || !strcmp(m,
                    "STDERR")))
                    err = 1;
            }
            tp++;
            continue;
        }
        if (isw("WITH") || isw("NO")) {
            acc("WITH");
            if (acc("NO")) { acc("ADVANCING"); nl = 0; }
            continue;
        }
        if (isw("AT") && 1) {
            tp++;
            while (cur()->k == T_NUM || isw("LINE") || isw("COLUMN") ||
                   isw("COL") || isw("NUMBER")) tp++;
            continue;
        }
        if (!starts_operand()) break;
        o = p_aexpr();
        if (n < 256) ops[n++] = opd_fld(&o);
    }
    if (err) em("cob_disp_upon(1);\n");
    for (i = 0; i < n; i++) em(fmt("cob_disp(%s);\n", ops[i]));
    if (nl) em("cob_disp_nl();\n");
    if (err) em("cob_disp_upon(0);\n");
}

static void p_accept(void)
{
    Opd t;
    tp++;
    t = p_ident();
    if (isw("FROM")) {
        tp++;
        if (isw("DATE")) {
            tp++;
            if (acc("YYYYMMDD"))
                em(fmt("cob_accept_from(%s, AC_DATE8);\n", t.c));
            else em(fmt("cob_accept_from(%s, AC_DATE);\n", t.c));
        } else if (isw("DAY")) {
            tp++;
            if (acc("YYYYDDD"))
                em(fmt("cob_accept_from(%s, AC_DAY7);\n", t.c));
            else em(fmt("cob_accept_from(%s, AC_DAY);\n", t.c));
        } else if (acc("DAY-OF-WEEK"))
            em(fmt("cob_accept_from(%s, AC_DOW);\n", t.c));
        else if (acc("TIME"))
            em(fmt("cob_accept_from(%s, AC_TIME);\n", t.c));
        else if (acc("COMMAND-LINE"))
            em(fmt("cob_accept_from(%s, AC_CMDLINE);\n", t.c));
        else if (acc("ARGUMENT-NUMBER"))
            em(fmt("cob_accept_from(%s, AC_ARGNUM);\n", t.c));
        else if (acc("ARGUMENT-VALUE"))
            em(fmt("cob_accept_from(%s, AC_ARGVAL);\n", t.c));
        else if (acc("ENVIRONMENT")) {
            Opd e = p_operand();
            em(fmt("cob_accept_env(%s, %s);\n", t.c, opd_fld(&e)));
        } else if (acc("ENVIRONMENT-NAME")
            || acc("ENVIRONMENT-VALUE")) {
            em(fmt("cob_accept(%s);\n", t.c));
        } else {
            tp++;                       /* mnemonic: console/sysin */
            em(fmt("cob_accept(%s);\n", t.c));
        }
    } else em(fmt("cob_accept(%s);\n", t.c));
    if (isw("AT")) {
        tp++;
        while (cur()->k == T_NUM || isw("LINE") || isw("COLUMN"))
            tp++;
    }
    {
        Hnd h = p_handlers(5);
        (void)h;
    }
    end_scope("END-ACCEPT");
}

/* corresponding leaf pairs between two groups */
typedef struct { Item *s, *d; } Pair;

static int find_pairs(Item *sg, Item *dg, Pair *out, int max)
{
    int n = 0;
    Item *a, *b;
    for (a = sg->child; a; a = a->next) {
        if (a->level == 88 || a->level == 66
            || a->level == 77) continue;
        if (a->filler || a->redef) continue;
        for (b = dg->child; b; b = b->next) {
            if (b->level == 88 || b->filler || b->redef) continue;
            if (!b->name || !a->name || strcmp(a->name, b->name))
                continue;
            if (a->child && b->child)
                n += find_pairs(a, b, out + n, max - n);
            else if (!a->child && !b->child && n < max) {
                out[n].s = a;
                out[n].d = b;
                n++;
            }
        }
    }
    return n;
}

static void p_move(void)
{
    Opd s;
    char *dst[64];
    int nd = 0, i, corr = 0;
    tp++;
    if (acc("CORRESPONDING") || acc("CORR")) corr = 1;
    s = p_aexpr();
    need("TO");
    while (!at_period() && !at_eof() && !is_resv(cur()->s) &&
           cur()->k == T_WORD) {
        Opd d = p_ident();
        if (nd < 64) dst[nd++] = d.c;
        if (corr) {
            Pair pr[256];
            int np = find_pairs(s.it, d.it, pr, 256), k;
            for (k = 0; k < np; k++)
                em(fmt("cob_move_rel(%s, %ld, &F%d, %s, %ld, &F%d);\n",
                       s.c, pr[k].s->offset - s.it->offset,
                           pr[k].s->id,
                       d.c, pr[k].d->offset - d.it->offset,
                       pr[k].d->id));
            nd = 0;
        }
    }
    if (corr) return;
    for (i = 0; i < nd; i++) {
        if (s.k == K_FIG) em(fmt("cob_fig(%s, %d);\n", dst[i], s.fig));
        else if (s.k == K_ALL)
            em(fmt("cob_move_all(%s, %s);\n", dst[i], s.c));
        else if (s.k == K_NULL)
            em(fmt("cob_fig(%s, FG_LOW);\n", dst[i]));
        else em(fmt("cob_move(%s, %s);\n", opd_fld(&s), dst[i]));
    }
}

/* ---- arithmetic ---- */

typedef struct { char *c; int mode; Item *it; } Tgt;

static int p_rounded(void)
{
    int mode = CR_NONE;
    if (acc("ROUNDED")) {
        mode = CR_AWAY;
        if (acc("MODE")) {
            skip_is();
            if (acc("NEAREST-AWAY-FROM-ZERO")) mode = CR_AWAY;
            else if (acc("NEAREST-EVEN")) mode = CR_EVEN;
            else if (acc("NEAREST-TOWARD-ZERO")) mode = CR_NEARTOZERO;
            else if (acc("AWAY-FROM-ZERO")) mode = CR_UP;
            else if (acc("TOWARD-GREATER")) mode = CR_CEIL;
            else if (acc("TOWARD-LESSER")) mode = CR_FLOOR;
            else if (acc("TRUNCATION")) mode = CR_TRUNC;
            else if (acc("PROHIBITED")) mode = CR_PROHIB;
        }
    }
    return mode;
}

static int p_targets(Tgt *t, int max)
{
    int n = 0;
    while (cur()->k == T_WORD && !is_resv(cur()->s)) {
        Opd o = p_ident();
        if (n >= max) syn("too many targets");
        t[n].c = o.c;
        t[n].it = o.it;
        t[n].mode = p_rounded();
        n++;
    }
    return n;
}

static void arith_end(Hnd *h, const char *endw)
{
    if (h->has) em_handlers(h, "E");
    end_scope(endw);
}

static void p_add_sub(int sub)
{
    char *ops[64];
    int nops = 0, i;
    Tgt t[32];
    int nt = 0, giving = 0;
    Hnd h;
    char *sum;
    const char *fn = sub ? "cob_subfrom" : "cob_addto";
    tp++;
    if (isw("CORRESPONDING") || isw("CORR")) {
        Opd a, b;
        Pair pr[256];
        int np, k;
        tp++;
        a = p_ident();
        need(sub ? "FROM" : "TO");
        b = p_ident();
        p_rounded();
        np = find_pairs(a.it, b.it, pr, 256);
        em("cob_clr_ex(); E = 0;\n");
        for (k = 0; k < np; k++) {
            em(fmt("S1 = cdx_ld(&D1, cob_relf(&T1, %s, %ld, &F%d));\n",
                   a.c, pr[k].s->offset - a.it->offset, pr[k].s->id));
            em(fmt(
                "E |= %s(S1, cob_relf(&T2, %s, %ld, &F%d), 0, 1);\n",
                   fn, b.c, pr[k].d->offset - b.it->offset,
                   pr[k].d->id));
        }
        h = p_handlers(1);
        arith_end(&h, sub ? "END-SUBTRACT" : "END-ADD");
        return;
    }
    while (starts_operand() && !isw("TO") && !isw("FROM") &&
           !isw("GIVING")) {
        Opd o = p_operand();
        if (nops < 64) ops[nops++] = opd_dec(&o);
    }
    if (nops == 0) syn("operand expected");
    sum = ops[0];
    for (i = 1; i < nops; i++)
        sum = fmt("cdx_add(%s, %s, %s)", newD(), sum, ops[i]);
    if (isw("TO") || isw("FROM")) {
        tp++;
        if (nops && (cur()->k == T_WORD) && !is_resv(cur()->s)) {
            /* targets, or an operand when GIVING follows */
            long save = tp;
            Opd o;
            o = p_aexpr();
            if (isw("GIVING")) {
                /* ADD a TO b GIVING c: b is part of the sum */
                char *ob = opd_dec(&o);
                if (sub)
                    sum = fmt("cdx_sub(%s, %s, %s)", newD(), ob, sum);
                else sum = fmt("cdx_add(%s, %s, %s)", newD(), sum, ob);
                tp++;
                giving = 1;
                nt = p_targets(t, 32);
            } else {
                tp = save;
                nt = p_targets(t, 32);
            }
        } else if (starts_operand()) {
            Opd o = p_aexpr();
            char *ob = opd_dec(&o);
            need("GIVING");
            giving = 1;
            if (sub) sum = fmt("cdx_sub(%s, %s, %s)", newD(), ob, sum);
            else sum = fmt("cdx_add(%s, %s, %s)", newD(), sum, ob);
            nt = p_targets(t, 32);
        }
    } else if (acc("GIVING")) {
        giving = 1;
        nt = p_targets(t, 32);
    } else syn("TO or GIVING expected");
    em("cob_clr_ex(); E = 0;\n");
    em(fmt("S1 = %s;\n", sum));
    h = p_handlers(1);
    for (i = 0; i < nt; i++) {
        if (giving)
            em(fmt("E |= cob_stx(S1, %s, %d, %d);\n", t[i].c,
                   t[i].mode, h.has));
        else
            em(fmt("E |= %s(S1, %s, %d, %d);\n", fn, t[i].c, t[i].mode,
                   h.has));
    }
    arith_end(&h, sub ? "END-SUBTRACT" : "END-ADD");
}

/* handlers parse after targets: reorder by capturing the targets
   first.  The code above emits the statement before parsing the
   handlers' text, so the handler code lands after the stores. */

static void p_multiply(void)
{
    Opd a, b;
    Tgt t[32];
    int nt, i, giving = 0;
    Hnd h;
    char *da;
    tp++;
    a = p_operand();
    da = opd_dec(&a);
    need("BY");
    b = p_aexpr();
    if (isw("GIVING")) {
        char *db = opd_dec(&b);
        tp++;
        giving = 1;
        nt = p_targets(t, 32);
        em("cob_clr_ex(); E = 0;\n");
        em(fmt("S1 = cdx_mul(%s, %s, %s);\n", newD(), da, db));
    } else {
        /* the second operand is the target */
        Tgt tt;
        long save;
        (void)save;
        if (b.k != K_FIELD || !b.it) syn("MULTIPLY target expected");
        tt.c = b.c;
        tt.it = b.it;
        tt.mode = p_rounded();
        nt = 0;
        t[nt++] = tt;
        nt += p_targets(t + nt, 31);
        em("cob_clr_ex(); E = 0;\n");
        em(fmt("S1 = %s;\n", da));
    }
    h = p_handlers(1);
    for (i = 0; i < nt; i++) {
        if (giving)
            em(fmt("E |= cob_stx(S1, %s, %d, %d);\n", t[i].c,
                   t[i].mode, h.has));
        else
            em(fmt("E |= cob_mulby(S1, %s, %d, %d);\n", t[i].c,
                   t[i].mode, h.has));
    }
    arith_end(&h, "END-MULTIPLY");
}

static void p_divide(void)
{
    Opd a, b;
    Tgt t[32];
    int nt = 0, i, giving = 0, into;
    Hnd h;
    char *da, *db = 0;
    Opd rem;
    int hasrem = 0;
    Tgt rt;
    tp++;
    a = p_operand();
    da = opd_dec(&a);
    into = isw("INTO");
    if (!into && !isw("BY")) syn("INTO or BY expected");
    tp++;
    b = p_aexpr();
    if (isw("GIVING")) {
        char *dbb = opd_dec(&b);
        tp++;
        giving = 1;
        nt = p_targets(t, 32);
        em("cob_clr_ex(); E = 0;\n");
        if (into) {
            db = fmt("cdx_div(%s, %s, %s)", newD(), dbb, da);
        } else {
            db = fmt("cdx_div(%s, %s, %s)", newD(), da, dbb);
        }
        em(fmt("S1 = %s;\n", db));
        if (isw("REMAINDER")) {
            tp++;
            rem = p_ident();
            rt.c = rem.c;
            hasrem = 1;
            /* S2 = dividend, S3 = divisor */
            if (into) {
                em(fmt("S2 = %s; S3 = %s;\n", dbb, da));
            } else {
                em(fmt("S2 = %s; S3 = %s;\n", da, dbb));
            }
        }
    } else {
        Tgt tt;
        if (b.k != K_FIELD || !b.it) syn("DIVIDE target expected");
        if (!into) syn("DIVIDE BY needs GIVING");
        tt.c = b.c;
        tt.it = b.it;
        tt.mode = p_rounded();
        nt = 0;
        t[nt++] = tt;
        nt += p_targets(t + nt, 31);
        em("cob_clr_ex(); E = 0;\n");
        em(fmt("S1 = %s;\n", da));
    }
    h = p_handlers(1);
    for (i = 0; i < nt; i++) {
        if (giving)
            em(fmt("E |= cob_stx(S1, %s, %d, %d);\n", t[i].c,
                   t[i].mode, h.has));
        else
            em(fmt("E |= cob_divby(S1, %s, %d, %d);\n", t[i].c,
                   t[i].mode, h.has));
    }
    if (hasrem) {
        /* remainder = dividend - stored quotient * divisor */
        em(fmt("cob_remainder(S2, S3, %s, %s);\n", t[0].c, rt.c));
        (void)rt;
    }
    arith_end(&h, "END-DIVIDE");
}

static void p_compute(void)
{
    Tgt t[32];
    int nt, i;
    Hnd h;
    char *e;
    Opd x;
    tp++;
    nt = p_targets(t, 32);
    if (!accop("=") && !acc("EQUAL")) syn("'=' expected");
    x = p_aexpr();
    e = opd_dec(&x);
    em("cob_clr_ex(); E = 0;\n");
    em(fmt("S1 = %s;\n", e));
    h = p_handlers(1);
    for (i = 0; i < nt; i++)
        em(fmt("E |= cob_stx(S1, %s, %d, %d);\n", t[i].c, t[i].mode,
               h.has));
    arith_end(&h, "END-COMPUTE");
}

/* ---- IF ---- */

static void p_if(void)
{
    char *c;
    tp++;
    c = p_cond();
    acc("THEN");
    em(fmt("if (%s) {\n", c));
    if (isw("NEXT") && isw_at(1, "SENTENCE")) {
        tp += 2;
        ns_used = 1;
        em(fmt("goto NS%d;\n", ns_id));
    } else if (isw("CONTINUE") && 0) {
    } else em(capture_stmts());
    if (isw("ELSE")) {
        tp++;
        em("} else {\n");
        if (isw("NEXT") && isw_at(1, "SENTENCE")) {
            tp += 2;
            ns_used = 1;
            em(fmt("goto NS%d;\n", ns_id));
        } else em(capture_stmts());
    }
    em("}\n");
    end_scope("END-IF");
}

/* ---- GO TO, STOP, EXIT ---- */

static void p_goto(void)
{
    tp++;
    acc("TO");
    if (at_period()) {
        em("cob_fatal(\"GO TO without a target\");\n");
        return;
    }
    {
        Para *ps[64];
        int n = 0, i;
        while (cur()->k == T_WORD && !isw("DEPENDING") &&
               !is_resv(cur()->s) && n < 64)
            ps[n++] = p_paraname();
        if (acc("DEPENDING")) {
            Opd o;
            acc("ON");
            o = p_aexpr();
            em(fmt("switch (cob_toi(%s)) {\n", opd_fld(&o)));
            for (i = 0; i < n; i++)
                em(fmt("case %d: goto L%d;\n", i + 1, ps[i]->id));
            em("default: break;\n}\n");
        } else if (n >= 1) em(fmt("goto L%d;\n", ps[0]->id));
        else syn("paragraph name expected");
    }
}

static void p_stop(void)
{
    tp++;
    if (acc("RUN")) {
        if (acc("WITH")) {
            if (acc("ERROR")) {
                acc("STATUS"); skip_is();
                if (cur()->k == T_NUM || cur()->k == T_WORD) {
                    Opd o = p_aexpr();
                    em(fmt("cob_move(%s, &cob_return_code);\n",
                           opd_fld(&o)));
                } else em("cob_seti(&cob_return_code, 1);\n");
            } else if (acc("NORMAL")) {
                acc("STATUS");
                if (cur()->k == T_NUM || cur()->k == T_WORD) {
                    Opd o = p_aexpr();
                    em(fmt("cob_move(%s, &cob_return_code);\n",
                           opd_fld(&o)));
                }
            }
        } else if (cur()->k == T_NUM) {
            Opd o = p_aexpr();
            em(fmt("cob_move(%s, &cob_return_code);\n", opd_fld(&o)));
        }
        em("cob_stop((int)cob_getrc());\n");
    } else {
        /* STOP literal */
        if (cur()->k == T_STR || cur()->k == T_NUM) tp++;
        em("cob_stop((int)cob_getrc());\n");
    }
}

static void p_exit(void)
{
    tp++;
    if (acc("PROGRAM") || acc("FUNCTION") || acc("METHOD")) {
        em("goto cob_exit;\n");
    } else if (acc("PERFORM")) {
        int cyc = acc("CYCLE");
        if (nloops == 0) syn("EXIT PERFORM outside an inline PERFORM");
        em(fmt("goto %c%d;\n", cyc ? 'C' : 'B', cyc ?
               loop_cyc[nloops - 1] : loop_brk[nloops - 1]));
    } else if (acc("PARAGRAPH")) {
        if (!cur_para) syn("EXIT PARAGRAPH outside a paragraph");
        em(fmt("goto E%d;\n", cur_para->endid));
    } else if (acc("SECTION")) {
        if (!cur_sec) syn("EXIT SECTION outside a section");
        em(fmt("goto E%d;\n", cur_sec->endid));
    }
    /* plain EXIT does nothing */
}

/* ---- PERFORM ---- */

static char *perform_call(Para *a, Para *b)
{
    int endid = b ? b->endid : a->endid;
    int ret = ++idctr;
    (b ? b : a)->perfend = 1;
    sb_s(&retcases, fmt("case %d: goto R%d;\n", ret, ret));
    return fmt(
        "if (pfsp >= 128) cob_fatal(\"PERFORM nested too deeply\");"
               " pfe[pfsp] = %d; pfv[pfsp] = %d; pfsp++; goto L%d;"
               " R%d: ;\n", endid, ret, a->id, ret);
}

static void mark_perfend(Para *a, Para *b)
{
    (void)a; (void)b;
}

/* parse a "UNTIL cond" clause and return the C condition */
static char *p_until(void)
{
    need("UNTIL");
    if (isw("EXIT")) { tp++; return "0"; }
    return p_cond();
}

static void p_perform(void)
{
    Para *pa = 0, *pb = 0;
    char *body = 0;
    int test_after = 0;
    char *times = 0;
    char *until = 0;
    int varying = 0, forever = 0;
    char *vname[8], *vfrom[8], *vby[8], *vcond[8];
    int nv = 0, i;
    int id = ++idctr;
    int inline_ = 0;
    tp++;
    if (isw("WITH") || isw("TEST")) {
        acc("WITH");
        need("TEST");
        if (acc("AFTER")) test_after = 1;
        else need("BEFORE");
    }
    if (isw("FOREVER")) { tp++; forever = 1; inline_ = 1; }
    else if ((cur()->k == T_WORD || (cur()->k == T_NUM &&
                                     !isw_at(1, "TIMES"))) &&
             !stmt_start() && !isw("UNTIL") && !isw("VARYING") &&
             !isw("WITH") && !isw("TEST")) {
        /* paragraph name or a counter before TIMES */
        if (isw_at(1, "TIMES")) {
            Opd o = p_aexpr();
            times = fmt("cob_toi(%s)", opd_fld(&o));
        } else {
            pa = p_paraname();
            if (acc("THRU") || acc("THROUGH")) pb = p_paraname();
            if (isw_at(1, "TIMES") &&
                (cur()->k == T_NUM || cur()->k == T_WORD)) {
                Opd o = p_aexpr();
                times = fmt("cob_toi(%s)", opd_fld(&o));
            }
        }
    } else if ((cur()->k == T_NUM && isw_at(1, "TIMES")) ||
               (cur()->k == T_OP && !strcmp(cur()->s, "("))) {
        Opd o = p_aexpr();
        times = fmt("cob_toi(%s)", opd_fld(&o));
        if (o.k == K_DEC) times = fmt("cob_dtoi(%s)", o.c);
    } else inline_ = 1;
    if (times) need("TIMES");
    if (!inline_ && !pa) inline_ = 1;
    if (isw("WITH") || isw("TEST")) {
        acc("WITH");
        need("TEST");
        if (acc("AFTER")) test_after = 1;
        else need("BEFORE");
    }
    if (isw("UNTIL")) {
        until = p_until();
    } else if (isw("VARYING")) {
        varying = 1;
        tp++;
        for (;;) {
            Opd v, f, b;
            char *cnd;
            v = p_ident();
            need("FROM");
            f = p_aexpr();
            need("BY");
            b = p_aexpr();
            cnd = p_until();
            if (nv < 8) {
                vname[nv] = v.c;
                vfrom[nv] = fmt("cob_move(%s, %s);\n", opd_fld(&f),
                    v.c);
                if (f.k == K_DEC)
                    vfrom[nv] = fmt("cob_stx(%s, %s, 0, 0);\n", f.c,
                        v.c);
                vby[nv] = fmt(
                        "cob_clr_ex(); cob_addto(%s, %s, 0, 0);\n",
                              opd_dec(&b), v.c);
                vcond[nv] = cnd;
                nv++;
            }
            if (acc("AFTER")) continue;
            break;
        }
    }
    (void)varying;
    /* body */
    loop_brk[nloops] = id;
    loop_cyc[nloops] = id;
    if (!inline_) {
        body = perform_call(pa, pb);
        mark_perfend(pa, pb);
    } else {
        nloops++;
        body = capture_stmts();
        nloops--;
        if (!acc("END-PERFORM")) {
            if (!at_period()) syn("END-PERFORM expected");
        }
    }
    /* assemble */
    if (nv == 0 && !times && !until && !forever) {
        /* plain: once */
        if (inline_) {
            em("{\n");
            em(body);
            em(fmt("C%d: ;\nB%d: ;\n}\n", id, id));
        } else em(body);
        return;
    }
    if (forever) {
        em(fmt("L%dT: ;\n", id));
        em(body);
        em(fmt("C%d: ; goto L%dT;\nB%d: ;\n", id, id, id));
        return;
    }
    if (times) {
        int c = ++nctr;
        em(fmt("CT%d = %s;\nL%dT: if (CT%d <= 0) goto B%d;\n", c,
            times,
               id, c, id));
        em(body);
        em(fmt("C%d: ; CT%d--; goto L%dT;\nB%d: ;\n", id, c, id, id));
        return;
    }
    if (until) {
        if (!test_after) {
            em(fmt("L%dT: if (%s) goto B%d;\n", id, until, id));
            em(body);
            em(fmt("C%d: ; goto L%dT;\nB%d: ;\n", id, id, id));
        } else {
            em(fmt("L%dT: ;\n", id));
            em(body);
            em(fmt("C%d: ; if (%s) goto B%d; goto L%dT;\nB%d: ;\n", id,
                   until, id, id, id));
        }
        return;
    }
    /* VARYING with nested AFTER levels */
    {
        int base = id;
        for (i = 0; i < nv; i++) {
            em(vfrom[i]);
            if (!test_after || i < nv - 1)
                em(fmt("L%dV%d: if (%s) goto B%dV%d;\n", base, i,
                       vcond[i], base, i));
            else em(fmt("L%dV%d: ;\n", base, i));
        }
        em(body);
        em(fmt("C%d: ;\n", id));
        for (i = nv - 1; i >= 0; i--) {
            if (test_after && i == nv - 1)
                em(fmt("if (%s) goto B%dV%d;\n", vcond[i], base, i));
            em(vby[i]);
            em(fmt("goto L%dV%d;\nB%dV%d: ;\n", base, i, base, i));
        }
        em(fmt("B%d: ;\n", id));
    }
}

/* ---- CONTINUE, NEXT SENTENCE ---- */

static void p_continue(void)
{
    tp++;
}

/* ---- EVALUATE ---- */

typedef struct {
    Opd o;
    int isbool;         /* 1 TRUE, 2 FALSE */
} Subj;

static char *eval_one(Subj *s)
{
    char *c;
    int neg = 0;
    if (isw("ANY")) { tp++; return "1"; }
    if (isw("NOT")) { neg = 1; tp++; }
    if (s->isbool) {
        /* WHEN TRUE/FALSE or a condition */
        if (isw("TRUE")) { tp++; c = "1"; }
        else if (isw("FALSE")) { tp++; c = "0"; }
        else c = p_cond();
        if (s->isbool == 2) c = fmt("(!%s)", c);
    } else {
        Opd lo, hi;
        char *ge;
        if (isw("TRUE") || isw("FALSE")) {
            int t = isw("TRUE");
            tp++;
            c = fmt("(%s)", t ? "1" : "0");
        } else {
            lo = p_aexpr();
            if (isw("THRU") || isw("THROUGH")) {
                tp++;
                hi = p_aexpr();
                ge = gen_cmp(&s->o, 5, &lo);
                c = fmt("(%s && %s)", ge, gen_cmp(&s->o, 4, &hi));
            } else c = gen_cmp(&s->o, 0, &lo);
        }
    }
    return neg ? fmt("(!%s)", c) : c;
}

static void p_evaluate(void)
{
    Subj subj[8];
    int ns = 0, i;
    int id = ++idctr;
    int base_t, base_d;
    int others = 0;
    tp++;
    for (;;) {
        Subj *s = &subj[ns];
        memset(s, 0, sizeof *s);
        if (isw("TRUE")) { tp++; s->isbool = 1; }
        else if (isw("FALSE")) { tp++; s->isbool = 2; }
        else s->o = p_aexpr();
        ns++;
        if (acc("ALSO")) continue;
        break;
    }
    base_t = ntmp;
    base_d = ndec;
    em("{\n");
    while (isw("WHEN")) {
        char *cond = 0;
        if (isw_at(1, "OTHER")) {
            tp += 2;
            others = 1;
            em(capture_stmts());
            break;
        }
        while (acc("WHEN")) {
            char *wc = 0;
            ntmp = base_t;
            ndec = base_d;
            for (i = 0; i < ns; i++) {
                char *o = eval_one(&subj[i]);
                wc = wc ? fmt("%s && %s", wc, o) : o;
                if (i + 1 < ns) need("ALSO");
            }
            cond = cond ? fmt("%s || %s", cond, wc) : wc;
        }
        {
            char *body = capture_stmts();
            em(fmt("if (%s) {\n", cond));
            em(body);
            em(fmt("goto EV%d;\n}\n", id));
        }
    }
    (void)others;
    em(fmt("EV%d: ;\n}\n", id));
    end_scope("END-EVALUATE");
}
