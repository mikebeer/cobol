/* c52_more.c -- STRING, UNSTRING, INSPECT, SET, SEARCH, INITIALIZE,
   CALL and the statement dispatcher */

static void p_invoke(void);
static int ncall = 0;           /* maximum CALL arguments */

/* ---- STRING ---- */

static void p_string(void)
{
    char *srcs[64], *dels[64];
    int modes[64], n = 0, i;
    Opd dst, ptr;
    int hasptr = 0;
    Hnd h;
    tp++;
    while (!isw("INTO") && !at_period() && !at_eof()) {
        int first = n;
        while (starts_operand() && !isw("DELIMITED")) {
            Opd o = p_operand();
            if (n < 64) { srcs[n] = opd_fld(&o); dels[n] = "0";
                          modes[n] = 0; n++; }
        }
        need("DELIMITED");
        acc("BY");
        if (acc("SIZE")) {
            for (i = first; i < n; i++) modes[i] = 0;
        } else {
            Opd d;
            acc("ALL");
            d = p_operand();
            for (i = first; i < n; i++) {
                dels[i] = opd_fld(&d);
                modes[i] = 1;
            }
        }
    }
    need("INTO");
    dst = p_ident();
    if (isw("WITH") || isw("POINTER")) {
        acc("WITH"); need("POINTER");
        ptr = p_ident();
        hasptr = 1;
    }
    em("cob_clr_ex();\n");
    em(fmt("cob_str_begin(%s, %s);\n", dst.c, hasptr ? ptr.c : "0"));
    for (i = 0; i < n; i++)
        em(fmt("cob_str_add(%s, %s, %d);\n", srcs[i], dels[i],
            modes[i]));
    em("E = cob_str_end();\n");
    h = p_handlers(4);
    em_handlers(&h, "E");
    end_scope("END-STRING");
}

/* ---- UNSTRING ---- */

static void p_unstring(void)
{
    Opd src, ptr, tally;
    int hasptr = 0, hastally = 0;
    char *delims[16];
    int dall[16], nd = 0, i;
    Sb into;
    Hnd h;
    memset(&into, 0, sizeof into);
    tp++;
    src = p_operand();
    if (acc("DELIMITED")) {
        acc("BY");
        for (;;) {
            Opd d;
            int all = acc("ALL");
            d = p_operand();
            if (nd < 16) { delims[nd] = opd_fld(&d); dall[nd++] = all;
                }
            if (acc("OR")) continue;
            break;
        }
    }
    need("INTO");
    while (cur()->k == T_WORD && !is_resv(cur()->s)) {
        Opd d = p_ident();
        char *di = "0", *ci = "0";
        for (;;) {
            if (acc("DELIMITER")) {
                acc("IN");
                di = p_ident().c;
            } else if (acc("COUNT")) {
                acc("IN");
                ci = p_ident().c;
            } else break;
        }
        sb_s(&into, fmt("cob_unstr_into(%s, %s, %s);\n", d.c, di, ci));
    }
    if (isw("WITH") || isw("POINTER")) {
        acc("WITH"); need("POINTER");
        ptr = p_ident();
        hasptr = 1;
    }
    if (acc("TALLYING")) {
        acc("IN");
        tally = p_ident();
        hastally = 1;
    }
    em("cob_clr_ex();\n");
    em(fmt("cob_unstr_begin(%s, %s);\n", opd_fld(&src),
           hasptr ? ptr.c : "0"));
    for (i = 0; i < nd; i++)
        em(fmt("cob_unstr_delim(%s, %d);\n", delims[i], dall[i]));
    em(sb_str(&into));
    em(fmt("E = cob_unstr_end(%s);\n", hastally ? tally.c : "0"));
    h = p_handlers(4);
    em_handlers(&h, "E");
    end_scope("END-UNSTRING");
}

/* ---- INSPECT ---- */

static void insp_ba(char **before, char **after)
{
    *before = "0";
    *after = "0";
    for (;;) {
        if (isw("BEFORE")) {
            Opd o;
            tp++; acc("INITIAL");
            o = p_operand();
            *before = opd_fld(&o);
        } else if (isw("AFTER")) {
            Opd o;
            tp++; acc("INITIAL");
            o = p_operand();
            *after = opd_fld(&o);
        } else break;
    }
}

static void p_inspect(void)
{
    Opd t;
    tp++;
    t = p_ident();
    em(fmt("cob_insp_begin(%s);\n", t.c));
    if (acc("TALLYING")) {
        while (cur()->k == T_WORD && !isw("REPLACING") &&
               !isw("CONVERTING") && !is_verb(cur()->s) &&
               !strncmp(cur()->s, "END-", 4) == 0 && 0) { }
        for (;;) {
            Opd cnt;
            if (isw("REPLACING") || isw("CONVERTING") ||
                stmt_start() || at_period()) break;
            cnt = p_ident();
            need("FOR");
            for (;;) {
                int op = -1;
                char *pat = "0", *bf, *af;
                if (acc("CHARACTERS")) op = 0;
                else if (acc("ALL")) op = 1;
                else if (acc("LEADING")) op = 2;
                else break;
                if (op != 0) {
                    Opd p = p_operand();
                    pat = opd_fld(&p);
                }
                insp_ba(&bf, &af);
                em(fmt("cob_insp_t(%d, %s, %s, %s, %s);\n", op, cnt.c,
                       pat, bf, af));
            }
        }
    }
    if (acc("REPLACING")) {
        for (;;) {
            int op = -1;
            char *pat = "0", *rep, *bf, *af;
            Opd r;
            if (acc("CHARACTERS")) op = 0;
            else if (acc("ALL")) op = 1;
            else if (acc("LEADING")) op = 2;
            else if (acc("FIRST")) op = 3;
            else break;
            if (op != 0) {
                Opd p = p_operand();
                pat = opd_fld(&p);
            }
            need("BY");
            r = p_operand();
            rep = opd_fld(&r);
            insp_ba(&bf, &af);
            em(fmt("cob_insp_r(%d, %s, %s, %s, %s);\n", op, pat, rep,
                bf,
                   af));
        }
    }
    if (acc("CONVERTING")) {
        Opd a, b;
        char *bf, *af;
        a = p_operand();
        need("TO");
        b = p_operand();
        insp_ba(&bf, &af);
        em(fmt("cob_insp_c(%s, %s, %s, %s);\n", opd_fld(&a),
               opd_fld(&b), bf, af));
    }
    em("cob_insp_run();\n");
    end_scope("END-INSPECT");
}

/* ---- SET ---- */

static void p_set(void)
{
    tp++;
    if (isw("ADDRESS") && isw_at(1, "OF")) {
        Opd x, p;
        tp += 2;
        x = p_ident();
        need("TO");
        p = p_operand();
        if (!x.it || x.it->rec != x.it)
            syn("SET ADDRESS OF needs a level 01 or 77 item");
        em(fmt("BIND%d(cob_ptr_get(%s));\n", x.it->id, opd_fld(&p)));
        return;
    }
    {
        Opd tg[16];
        int nt = 0, i;
        while (cur()->k == T_WORD && !isw("TO") && !isw("UP") &&
               !isw("DOWN") && !isw("ON") && !isw("OFF") && nt < 16)
            tg[nt++] = p_ident();
        if (nt == 0) syn("SET target expected");
        if (acc("TO")) {
            if (isw("TRUE") || isw("FALSE")) {
                int t = isw("TRUE");
                tp++;
                for (i = 0; i < nt; i++) {
                    Item *c = tg[i].cond;
                    Val *v;
                    if (!c) syn(
                            "SET ... TO TRUE needs a condition-name");
                    v = t ? c->vals : c->falsev;
                    if (!v) syn("no value for SET TO FALSE");
                    if (v->kind == 1)
                        em(fmt("cob_fig(%s, %d);\n", tg[i].parent,
                               v->fig));
                    else em(fmt("cob_move(&%s, %s);\n",
                                lit_name(&v->lo, v->neg),
                                tg[i].parent));
                }
                return;
            }
            if (isw("ADDRESS") && isw_at(1, "OF")) {
                Opd x;
                tp += 2;
                x = p_ident();
                for (i = 0; i < nt; i++)
                    em(fmt("cob_set_addr(%s, %s);\n", tg[i].c, x.c));
                return;
            }
            if (isw("NULL") || isw("NULLS")) {
                tp++;
                for (i = 0; i < nt; i++)
                    em(fmt("cob_fig(%s, FG_LOW);\n", tg[i].c));
                return;
            }
            if (isw("ON") || isw("OFF")) { tp++; return; }
            {
                Opd v = p_aexpr();
                for (i = 0; i < nt; i++) {
                    if (v.k == K_DEC)
                        em(fmt("cob_stx(%s, %s, 0, 0);\n", v.c,
                            tg[i].c));
                    else em(fmt("cob_move(%s, %s);\n", opd_fld(&v),
                                tg[i].c));
                }
            }
            return;
        }
        if (isw("UP") || isw("DOWN")) {
            int up = isw("UP");
            Opd v;
            tp++;
            need("BY");
            v = p_aexpr();
            for (i = 0; i < nt; i++) {
                if (tg[i].it && tg[i].it->ctype == CT_PTR)
                    em(fmt("cob_ptr_add(%s, %s%s);\n", tg[i].c,
                           up ? "" : "-", fmt("cob_dtoi(%s)",
                                              opd_dec(&v))));
                else
                    em(fmt("cob_clr_ex(); %s(%s, %s, 0, 0);\n",
                           up ? "cob_addto" : "cob_subfrom",
                           opd_dec(&v), tg[i].c));
            }
            return;
        }
        if (isw("ON") || isw("OFF")) { tp++; return; }
        syn("bad SET");
    }
}

/* ---- SEARCH ---- */

static void p_search(void)
{
    int all, id = ++idctr;
    Opd tbl;
    char *idx = 0, *vary = 0, *atend = 0;
    char *limit;
    Sb whens;
    int nw = 0;
    memset(&whens, 0, sizeof whens);
    tp++;
    all = acc("ALL");
    tbl = p_ident();
    if (!tbl.it) syn("SEARCH needs a table");
    {
        Item *t = tbl.it;
        while (t && t->occmax <= 0) t = t->parent;
        if (!t) syn("SEARCH table has no OCCURS");
        if (t->idxn[0]) {
            Item *ix = lookup(t->idxn[0], 0, 0);
            idx = fmt("&F%d", ix->id);
        }
        limit = t->odo ? fmt("cob_toi(&F%d)", t->odo->id)
                       : fmt("%ldL", (long)t->occmax);
    }
    if (acc("VARYING")) {
        Opd v = p_ident();
        vary = v.c;
        if (!idx || (v.it && v.it->isindex)) { if (!idx) idx = v.c;
                                               vary = (idx == v.c) ? 0
                                                   : v.c; }
    }
    if (!idx) syn("SEARCH table needs INDEXED BY or VARYING");
    if (isw("AT") || isw("END")) {
        acc("AT"); need("END");
        atend = capture_stmts();
    }
    while (isw("WHEN")) {
        char *c;
        tp++;
        c = p_cond();
        sb_s(&whens, fmt("if (%s) {\n", c));
        sb_s(&whens, capture_stmts());
        sb_s(&whens, fmt("goto SE%d;\n}\n", id));
        nw++;
    }
    end_scope("END-SEARCH");
    em("{\n");
    if (all) em(fmt("cob_seti(%s, 1);\n", idx));
    em(fmt("LS%d: if (cob_toi(%s) > %s) {\n", id, idx, limit));
    if (atend) em(atend);
    em(fmt("goto SE%d;\n}\n", id));
    em(sb_str(&whens));
    em(fmt("cob_seti(%s, cob_toi(%s) + 1);\n", idx, idx));
    if (vary) em(fmt("cob_seti(%s, cob_toi(%s) + 1);\n", vary, vary));
    em(fmt("goto LS%d;\nSE%d: ;\n}\n", id, id));
}

/* ---- INITIALIZE ---- */

static int item_cat(Item *it)
{
    switch (it->ctype) {
    case CT_NUMDISP: case CT_BIN: case CT_BIN5: case CT_PACKED:
    case CT_FLT: case CT_DBL: return 1;         /* numeric */
    case CT_NUMEDIT: return 2;
    case CT_ALNEDIT: return 3;
    case CT_NAT: return 4;
    case CT_PTR: return 5;
    case CT_OBJ: return 6;
    case CT_ALNUM: return (it->flags & CF_ALPHA) ? 7 : 8;
    default: return 8;
    }
}

typedef struct { int cat; Opd v; int set; } Rpl;

static void init_gen(Sb *tab, int *n, Item *it, Item *grp,
    int use_value,
                     Rpl *rp, int nrp, int filler, int inred)
{
    Item *c;
    if (it->level == 88 || it->level == 66) return;
    if (it->redef) inred = 1;
    if (it->child) {
        for (c = it->child; c; c = c->next)
            init_gen(tab, n, c, grp, use_value, rp, nrp, filler,
                inred);
        return;
    }
    if (inred && it != grp) return;
    if (it->filler && !filler && it != grp) return;
    {
        Item *d[16];
        int nd = item_dims(it, d), gd = item_dims(grp, d), k;
        int own;
        Sb cs, ss;
        int kind = 0, fig = 0, r;
        char *lit = 0;
        long rel;
        Item *dd[16];
        (void)gd;
        nd = item_dims(it, dd);
        /* dimensions inside the group only */
        {
            Item *gdim[16];
            int ng = item_dims(grp, gdim);
            own = nd - ng;
            if (grp->occmax > 0 && ng > 0) own = nd - ng;
            (void)gdim;
        }
        if (own > 3) syn("too many OCCURS levels in INITIALIZE");
        memset(&cs, 0, sizeof cs);
        memset(&ss, 0, sizeof ss);
        for (k = 0; k < 3; k++) {
            int di = nd - own + k;
            if (k) { sb_c(&cs, ','); sb_c(&ss, ','); }
            if (k < own) {
                sb_s(&cs, fmt("%ld", (long)dd[di]->occmax));
                sb_s(&ss, fmt("%ld", dd[di]->size));
            } else { sb_c(&cs, '0'); sb_c(&ss, '0'); }
        }
        rel = it->offset - grp->offset;
        /* choose the value */
        if (use_value && it->vals) {
            kind = -1;
            lit = val_lit(it->vals, it, &kind, &fig);
        } else {
            for (r = 0; r < nrp; r++) {
                if (rp[r].cat == item_cat(it) && rp[r].set) {
                    Opd *v = &rp[r].v;
                    if (v->k == K_FIG) { kind = 2; fig = v->fig; }
                    else if (v->k == K_ALL) { kind = 3; lit = v->c; }
                    else { kind = 1; lit = v->c; }
                    break;
                }
            }
        }
        sb_s(tab, fmt(
                "    { &F%d, %d, %s, %d, %d, { %s }, { %s }, %ld },\n",
                      it->id, kind, lit ? lit : "0", fig, own, cs.s,
                      ss.s, rel));
        (*n)++;
    }
}

static void p_initialize(void)
{
    Opd t[16];
    int nt = 0, i;
    Rpl rp[16];
    int nrp = 0, use_value = 0, filler = 0;
    tp++;
    while (cur()->k == T_WORD && !is_resv(cur()->s) && nt < 16)
        t[nt++] = p_ident();
    for (;;) {
        if (acc("WITH")) { acc("FILLER"); filler = 1; continue; }
        if (acc("ALL")) { acc("TO"); acc("VALUE"); use_value = 1;
                          continue; }
        if (acc("THEN")) continue;
        if (acc("TO")) { need("VALUE"); use_value = 1; continue; }
        if (acc("DEFAULT")) continue;
        if (acc("REPLACING")) {
            for (;;) {
                int cat = 0;
                Opd v;
                if (acc("NUMERIC")) cat = 1;
                else if (acc("NUMERIC-EDITED")) cat = 2;
                else if (acc("ALPHANUMERIC-EDITED")) cat = 3;
                else if (acc("NATIONAL")) cat = 4;
                else if (acc("ALPHABETIC")) cat = 7;
                else if (acc("ALPHANUMERIC")) cat = 8;
                else if (acc("DATA") && 0) cat = 5;
                else break;
                acc("DATA"); need("BY");
                v = p_operand();
                if (nrp < 16) {
                    rp[nrp].cat = cat;
                    rp[nrp].v = v;
                    rp[nrp].set = 1;
                    nrp++;
                }
            }
            continue;
        }
        break;
    }
    for (i = 0; i < nt; i++) {
        Item *it = t[i].it;
        Sb tab;
        int n = 0, id = ++idctr;
        memset(&tab, 0, sizeof tab);
        if (!it) syn("INITIALIZE target");
        init_gen(&tab, &n, it, it, use_value, rp, nrp, filler, 0);
        if (n == 0) continue;
        sb_s(&P->init, fmt("static CobInit INI_%d[] = {\n", id));
        sb_s(&P->init, tab.s);
        sb_s(&P->init, "};\n");
        em(fmt("cob_run_init_at(%s, INI_%d, %d);\n", t[i].c, id, n));
    }
}

/* ---- CALL ---- */

static int p_call_args(char **argv_)
{
    int n = 0;
    int mode = 0;               /* 0 ref, 1 content, 2 value */
    for (;;) {
        if (acc("BY")) {
            if (acc("REFERENCE")) mode = 0;
            else if (acc("CONTENT")) mode = 1;
            else if (acc("VALUE")) mode = 2;
            continue;
        }
        if (acc("REFERENCE")) { mode = 0; continue; }
        if (acc("CONTENT")) { mode = 1; continue; }
        if (acc("VALUE")) { mode = 2; continue; }
        if (isw("OMITTED")) {
            tp++;
            if (n < 64) argv_[n++] = "0";
            continue;
        }
        if (!starts_operand()) break;
        {
            Opd o = p_aexpr();
            char *a;
            if (mode == 0) a = opd_fld(&o);
            else if (mode == 1) {
                int id = ++idctr;
                long sz = 64;
                if (o.it) sz = o.it->size;
                else if (o.k == K_NLIT || o.k == K_SLIT)
                    sz = o.lit.len ? o.lit.len : 32;
                if (sz < 64) sz = 64;
                sb_s(&P->init,
                    fmt("static unsigned char CC%d[%ld];\n"
                    "static CobFld CF%d = { %ld, CC%d, 0 };\n",
                    id, sz, id, sz, id));
                em(fmt("cob_content(&CF%d, %s, %ld);\n", id,
                       opd_fld(&o), sz));
                a = fmt("&CF%d", id);
            } else {
                int id = ++idctr;
                sb_s(&P->init,
                    fmt("static unsigned char CC%d[16];\n"
                    "static CobFld CF%d = { 4, CC%d, 0 };\n",
                    id, id, id));
                em(fmt("cob_byvalue(&CF%d, %s);\n", id,
                       opd_dec(&o)));
                a = fmt("&CF%d", id);
            }
            if (n < 64) argv_[n++] = a;
        }
    }
    return n;
}

static void p_call(void)
{
    char *argv_[64];
    int n = 0, i;
    char *name = 0, *namefld = 0, *ret = "0";
    Hnd h;
    Opd tn;
    int prefs = 0;
    tp++;
    acc("STATIC");
    tn = p_operand();
    if (tn.k == K_SLIT) {
        name = c_lit_str(tn.lit.s);
    } else namefld = opd_fld(&tn);
    if (acc("USING")) n = p_call_args(argv_);
    (void)prefs;
    if (acc("RETURNING") || acc("GIVING")) {
        Opd r = p_ident();
        ret = r.c;
    }
    if (n > ncall) ncall = n;
    for (i = 0; i < n; i++) em(fmt("CA[%d] = %s;\n", i, argv_[i]));
    h = p_handlers(5);
    em("cob_clr_ex();\n");
    if (name)
        em(fmt("E = cob_call(%s, %d, CA, %s);\n", name, n, ret));
    else
        em(fmt("E = cob_call_f(%s, %d, CA, %s);\n", namefld, n, ret));
    if (h.has) em_handlers(&h, "E");
    else em("if (E) cob_fatal(\"CALL: program not found\");\n");
    end_scope("END-CALL");
}

static void p_cancel(void)
{
    tp++;
    while (starts_operand()) {
        Opd o = p_operand();
        if (o.k == K_SLIT)
            em(fmt("cob_cancel(%s);\n", c_lit_str(o.lit.s)));
        else em(fmt("cob_cancel_f(%s);\n", opd_fld(&o)));
    }
}

/* ---- ALLOCATE / FREE ---- */

static void p_allocate(void)
{
    tp++;
    if (cur()->k == T_WORD && !isw("RETURNING")
        && !isw("CHARACTERS")) {
        Opd x = p_ident();
        Item *it = x.it;
        char *ret = 0;
        int initz = 0;
        if (acc("INITIALIZED")) initz = 1;
        if (acc("RETURNING")) {
            Opd r = p_ident();
            ret = r.c;
        }
        if (!it || it->rec != it)
            syn("ALLOCATE needs a BASED item at level 01 or 77");
        em(fmt("BIND%d(cob_alloc(%ld, %d));\n", it->id, it->size,
               initz));
        if (ret)
            em(fmt("cob_set_addr(%s, %s);\n", ret, x.c));
    } else {
        Opd n;
        char *ret = 0;
        long dummy = 0;
        (void)dummy;
        n = p_aexpr();
        acc("CHARACTERS");
        if (acc("INITIALIZED")) { }
        need("RETURNING");
        ret = p_ident().c;
        em(fmt("cob_set_ptr(%s, cob_alloc(cob_dtoi(%s), 1));\n", ret,
               opd_dec(&n)));
    }
}

static void p_free(void)
{
    tp++;
    while (cur()->k == T_WORD && !is_resv(cur()->s)) {
        Opd x = p_ident();
        if (x.it && x.it->rec == x.it && x.it->based)
            em(fmt("cob_free(B%d); BIND%d(0);\n", x.it->master->id,
                   x.it->id));
        else if (x.it && x.it->ctype == CT_PTR)
            em(fmt("cob_free_ptr(%s);\n", x.c));
        else syn("FREE needs a BASED item or a pointer");
    }
}

/* ---- the dispatcher ---- */

static void p_file_stmt(const char *v);
static void p_sort_stmt(const char *v);

static void p_statement(void)
{
    Tok *t = cur();
    const char *v = t->s;
    tmp_reset();
    if (!strcmp(v, "DISPLAY")) p_display();
    else if (!strcmp(v, "MOVE")) p_move();
    else if (!strcmp(v, "ADD")) p_add_sub(0);
    else if (!strcmp(v, "SUBTRACT")) p_add_sub(1);
    else if (!strcmp(v, "MULTIPLY")) p_multiply();
    else if (!strcmp(v, "DIVIDE")) p_divide();
    else if (!strcmp(v, "COMPUTE")) p_compute();
    else if (!strcmp(v, "IF")) p_if();
    else if (!strcmp(v, "PERFORM")) p_perform();
    else if (!strcmp(v, "EVALUATE")) p_evaluate();
    else if (!strcmp(v, "GO")) p_goto();
    else if (!strcmp(v, "STOP")) p_stop();
    else if (!strcmp(v, "EXIT")) p_exit();
    else if (!strcmp(v, "GOBACK")) { tp++; em("goto cob_exit;\n"); }
    else if (!strcmp(v, "CONTINUE")) p_continue();
    else if (!strcmp(v, "NEXT")) {
        tp += 2;
        ns_used = 1;
        em(fmt("goto NS%d;\n", ns_id));
    }
    else if (!strcmp(v, "ACCEPT")) p_accept();
    else if (!strcmp(v, "STRING")) p_string();
    else if (!strcmp(v, "UNSTRING")) p_unstring();
    else if (!strcmp(v, "INSPECT")) p_inspect();
    else if (!strcmp(v, "SET")) p_set();
    else if (!strcmp(v, "SEARCH")) p_search();
    else if (!strcmp(v, "INITIALIZE")) p_initialize();
    else if (!strcmp(v, "CALL")) p_call();
    else if (!strcmp(v, "CANCEL")) p_cancel();
    else if (!strcmp(v, "ALLOCATE")) p_allocate();
    else if (!strcmp(v, "FREE")) p_free();
    else if (!strcmp(v, "OPEN") || !strcmp(v, "CLOSE") ||
             !strcmp(v, "READ") || !strcmp(v, "WRITE") ||
             !strcmp(v, "REWRITE") || !strcmp(v, "DELETE") ||
             !strcmp(v, "START") || !strcmp(v, "UNLOCK"))
        p_file_stmt(v);
    else if (!strcmp(v, "SORT") || !strcmp(v, "MERGE") ||
             !strcmp(v, "RELEASE") || !strcmp(v, "RETURN"))
        p_sort_stmt(v);
    else if (!strcmp(v, "INVOKE")) p_invoke();
    else perr(t->file, t->line, "unsupported statement", t->s);
}
