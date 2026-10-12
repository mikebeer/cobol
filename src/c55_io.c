/* c55_io.c -- file and sort statements, USE declaratives */

static int in_decl = 0;

static Fil *p_fil(void)
{
    char *n = get_name();
    Fil *f = find_file(n);
    if (!f) syn(cat2("unknown file name: ", n));
    if (!f->recs) syn(cat2("file has no FD entry: ", n));
    return f;
}

static char *fitem(const char *n)
{
    Item *it;
    if (!n) return "0";
    it = lookup(n, 0, 0);
    if (!it)
        perr(srcname, cur()->line, "unknown name in file entry", n);
    return fmt("&F%d", it->id);
}

static void emit_files(Prog *pr, Sb *o)
{
    Fil *f;
    int i;
    for (f = pr->files; f; f = f->next) {
        Item *rec = f->recs;
        if (!rec) continue;
        if (f->dependn && !lookup(f->dependn, 0, 0)) {
            f->assign = f->dependn;
            f->dependn = 0;
        }
        sb_s(o, fmt("static CobFile %s = { \"%s\", %d, %d, %d, %d, "
                    "%ld, %ld,\n", f->cname,
                    f->assign ? f->assign : f->name, f->org,
                    f->access, f->optional, f->rectype, f->minrec,
                    f->maxrec));
        sb_s(o, fmt("    R%d, %ld, %s, %s, %s, %s, %s,\n",
                    rec->master->id, rec->master->bufsize,
                    fitem(f->statusn), fitem(f->depn),
                    fitem(f->relkeyn), fitem(f->dependn),
                    fitem(f->keyn)));
        sb_s(o, "    { ");
        for (i = 0; i < 8; i++)
            sb_s(o, fmt("%s%s", i ? ", " : "",
                        i < f->nakeys ? fitem(f->akn[i]) : "0"));
        sb_s(o, " }, { ");
        for (i = 0; i < 8; i++)
            sb_s(o, fmt("%s%d", i ? ", " : "",
                        i < f->nakeys ? f->akdup[i] : 0));
        sb_s(o, fmt(" }, %d, 0 };\n", f->nakeys));
    }
}

/* what to do with an exception nobody handled */
static void em_unhandled(Fil *f)
{
    int i, any = 0;
    if (f->dp) {
        em(perform_call(f->dp, 0));
        return;
    }
    for (i = 0; i < 4; i++) {
        if (!P->dmode[i]) continue;
        em(fmt("%sif (%s.mode == %d) {\n", any ? "else " : "",
               f->cname, i));
        em(perform_call(P->dmode[i], 0));
        em("}\n");
        any = 1;
    }
    if (any) em("else ");
    em(fmt("cob_fnh(&%s, E);\n", f->cname));
}

/* kind: 0 none, 2 AT END, 3 INVALID KEY */
static void em_fres(Fil *f, Hnd *h, int kind, const char *okx)
{
    em("if (E >= 10) {\n");
    if (kind && h && h->has) {
        em(fmt("if (%s) {\n", kind == 2 ? "E == 10" : "E / 10 == 2"));
        if (h->err) em(h->err);
        em("} else {\n");
        em_unhandled(f);
        em("}\n");
    } else em_unhandled(f);
    em("} else {\n");
    if (okx) em(okx);
    if (h && h->ok) em(h->ok);
    em("}\n");
}

static char *recfld(Fil *f)
{
    return fmt("&F%d", f->recs->id);
}

/* ---- OPEN and CLOSE ---- */

static int file_list_more(void)
{
    Tok *t = cur();
    if (t->k != T_WORD) return 0;
    if (isw("INPUT") || isw("OUTPUT") || isw("I-O") || isw("EXTEND") ||
        isw("WITH") || isw("NO") || isw("REWIND") || isw("LOCK") ||
        isw("REEL") || isw("UNIT") || isw("FOR") || isw("REMOVAL"))
        return 1;
    return find_file(t->s) != 0;
}

static void p_open(void)
{
    int mode = 0;
    tp++;
    while (file_list_more()) {
        if (acc("INPUT")) mode = 0;
        else if (acc("OUTPUT")) mode = 1;
        else if (acc("I-O")) mode = 2;
        else if (acc("EXTEND")) mode = 3;
        else if (isw("WITH") || isw("NO") || isw("REWIND")) {
            tp++;
            continue;
        } else {
            Fil *f = p_fil();
            em("cob_clr_ex();\n");
            em(fmt("E = cob_open(&%s, %d);\n", f->cname, mode));
            em_fres(f, 0, 0, 0);
        }
    }
}

static void p_close(void)
{
    tp++;
    while (file_list_more()) {
        if (isw("WITH") || isw("NO") || isw("REWIND") ||
            isw("LOCK") || isw("REEL") || isw("UNIT") ||
            isw("FOR") || isw("REMOVAL")) {
            tp++;
            continue;
        } else {
            Fil *f = p_fil();
            em(fmt("E = cob_close(&%s);\n", f->cname));
            em_fres(f, 0, 0, 0);
        }
    }
}

/* ---- READ ---- */

static int key_number(Fil *f, const char *n)
{
    int i;
    if (f->keyn && !strcmp(f->keyn, n)) return 0;
    for (i = 0; i < f->nakeys; i++)
        if (f->akn[i] && !strcmp(f->akn[i], n)) return i + 1;
    return -1;
}

static void p_read(void)
{
    Fil *f;
    int how, kn = 0, nxt = 0, prev = 0;
    char *into = 0;
    Hnd he, hi, h;
    tp++;
    f = p_fil();
    if (acc("NEXT")) nxt = 1;
    else if (acc("PREVIOUS")) prev = 1;
    acc("RECORD");
    if (acc("INTO")) {
        Opd t = p_ident();
        into = fmt("cob_move(%s, %s);\n", recfld(f), opd_fld(&t));
    }
    if (acc("KEY")) {
        char *n;
        skip_is();
        n = get_name();
        kn = key_number(f, n);
        if (kn < 0) syn(cat2("not a key of the file: ", n));
    }
    if (prev) how = 2;
    else if (nxt || f->access == 0) how = 0;
    else how = 1;
    if (f->org <= 1 && how == 1) how = 0;
    he = p_handlers(2);
    hi = p_handlers(3);
    memset(&h, 0, sizeof h);
    h.has = he.has || hi.has;
    h.err = he.has ? he.err : hi.err;
    h.ok = he.ok ? he.ok : hi.ok;
    end_scope("END-READ");
    em(fmt("E = cob_read(&%s, %d, %d);\n", f->cname, how, kn));
    em_fres(f, &h, he.has ? 2 : 3, into);
    if (he.has && hi.has) { /* both given: INVALID KEY too */ }
}

/* ---- WRITE and REWRITE ---- */

static Fil *record_file(Item *it)
{
    Item *r = it->rec ? it->rec : it;
    if (!r->file) syn("not a file record");
    return r->file;
}

static void p_write(int rewrite)
{
    Opd rec;
    Fil *f;
    char *recc;
    char *from = 0;
    int adv = 0;
    char *lines = "1";
    Hnd hk, he, h;
    tp++;
    if (isw("FILE")) {
        tp++;
        f = p_fil();
        recc = recfld(f);
    } else {
        rec = p_ident();
        if (!rec.it) syn("record name expected");
        f = record_file(rec.it);
        recc = opd_fld(&rec);
    }
    if (acc("FROM")) {
        Opd s = p_operand();
        from = fmt("cob_move(%s, %s);\n", opd_fld(&s), recc);
    }
    if (!rewrite) {
        acc("WITH");
        if (isw("BEFORE") || isw("AFTER")) {
            adv = isw("BEFORE") ? 1 : 2;
            tp++;
            acc("ADVANCING");
            if (acc("PAGE")) lines = "-1";
            else {
                Opd n = p_aexpr();
                lines = fmt("cob_toi(%s)", opd_fld(&n));
                acc("LINE"); acc("LINES");
            }
        }
    }
    he = p_handlers(6);
    hk = p_handlers(3);
    memset(&h, 0, sizeof h);
    h.has = hk.has;
    h.err = hk.err;
    h.ok = hk.ok ? hk.ok : he.ok;
    end_scope(rewrite ? "END-REWRITE" : "END-WRITE");
    if (from) em(from);
    if (rewrite) em(fmt("E = cob_rewrite(&%s);\n", f->cname));
    else em(fmt("E = cob_write(&%s, (%s)->size, %d, %s);\n",
                f->cname, recc, adv, lines));
    em_fres(f, &h, 3, 0);
}

static void p_delete(void)
{
    Fil *f;
    Hnd h;
    tp++;
    f = p_fil();
    acc("RECORD");
    h = p_handlers(3);
    end_scope("END-DELETE");
    em(fmt("E = cob_delete(&%s);\n", f->cname));
    em_fres(f, &h, 3, 0);
}

static void p_start(void)
{
    Fil *f;
    Hnd h;
    int op = 2, kn = 0;
    tp++;
    f = p_fil();
    if (acc("KEY")) {
        int neg;
        char *n;
        skip_is();
        neg = acc("NOT");
        if (acc("EQUAL") || isop("=")) {
            if (isop("=")) tp++;
            acc("TO");
            op = 0;
        } else if (acc("GREATER") || isop(">") || isop(">=")) {
            op = 1;
            if (isop(">")) tp++;
            else if (isop(">=")) { tp++; op = 2; }
            acc("THAN");
            if (acc("OR")) { acc("EQUAL"); acc("TO"); op = 2; }
        } else if (acc("LESS") || isop("<") || isop("<=")) {
            op = 3;
            if (isop("<")) tp++;
            else if (isop("<=")) { tp++; op = 4; }
            acc("THAN");
            if (acc("OR")) { acc("EQUAL"); acc("TO"); op = 4; }
        } else syn("bad START relation");
        if (neg) {
            if (op == 3) op = 2;
            else if (op == 1) op = 4;
            else syn("unsupported START relation");
        }
        n = get_name();
        kn = key_number(f, n);
        if (kn < 0 && f->org == 3)
            syn(cat2("not a key of the file: ", n));
        if (kn < 0) kn = 0;
    } else op = 0;
    if (acc("WITH")) { acc("LENGTH"); (void)p_aexpr(); }
    h = p_handlers(3);
    end_scope("END-START");
    em(fmt("E = cob_start(&%s, %d, %d);\n", f->cname, op, kn));
    em_fres(f, &h, 3, 0);
}

static void p_file_stmt(const char *v)
{
    if (!strcmp(v, "OPEN")) p_open();
    else if (!strcmp(v, "CLOSE")) p_close();
    else if (!strcmp(v, "READ")) p_read();
    else if (!strcmp(v, "WRITE")) p_write(0);
    else if (!strcmp(v, "REWRITE")) p_write(1);
    else if (!strcmp(v, "DELETE")) p_delete();
    else if (!strcmp(v, "START")) p_start();
    else { /* UNLOCK */
        tp++;
        (void)p_fil();
    }
}

/* ---- USE (declaratives) ---- */

static void p_use(void)
{
    Para *sec = cur_sec;
    tp++;                       /* USE */
    acc("GLOBAL");
    if (acc("BEFORE")) {
        while (!at_period() && !at_eof()) tp++;
        return;
    }
    acc("AFTER");
    acc("STANDARD");
    if (!(acc("EXCEPTION") || acc("ERROR"))) syn("bad USE statement");
    acc("PROCEDURE");
    acc("ON");
    if (!sec) syn("USE outside a section");
    while (!at_period() && !at_eof()) {
        if (acc("INPUT")) P->dmode[0] = sec;
        else if (acc("OUTPUT")) P->dmode[1] = sec;
        else if (acc("I-O")) P->dmode[2] = sec;
        else if (acc("EXTEND")) P->dmode[3] = sec;
        else {
            Fil *f = find_file(get_name());
            if (!f) syn("unknown file in USE");
            f->dp = sec;
        }
    }
}

/* ---- SORT, MERGE, RELEASE, RETURN ---- */

static void p_sort_proc(char **code)
{
    Para *a, *b = 0;
    acc("IS");
    a = p_paraname();
    if (acc("THRU") || acc("THROUGH")) b = p_paraname();
    *code = perform_call(a, b);
}

static void p_sort_table(void)
{
    Opd t = p_ident();
    Item *it = t.it;
    int nk = 0, i;
    char *keyc[16];
    int keyd[16];
    char *cnt;
    if (!it) syn("SORT needs a table or a file");
    while (it && it->occmax <= 0) it = it->parent;
    if (!it) syn("SORT table has no OCCURS");
    while (isw("ON") || isw("ASCENDING") || isw("DESCENDING")) {
        int desc;
        acc("ON");
        desc = isw("DESCENDING");
        tp++;
        acc("KEY");
        while (cur()->k == T_WORD && !isw("ON") && !isw("ASCENDING")
               && !isw("DESCENDING") && !isw("WITH") &&
               !isw("DUPLICATES")) {
            Opd k = p_ident();
            if (nk < 16) {
                keyc[nk] = opd_fld(&k);
                keyd[nk++] = desc;
            }
        }
        if (nk == 0) {
            keyc[0] = fmt("&F%d", it->id);
            keyd[nk++] = desc;
        }
    }
    if (acc("WITH")) acc("DUPLICATES");
    else acc("DUPLICATES");
    acc("IN"); acc("ORDER");
    cnt = it->odo ? fmt("cob_toi(&F%d)", it->odo->id)
                  : fmt("%ldL", (long)it->occmax);
    em("cob_sort_begin(0);\n");
    for (i = 0; i < nk; i++)
        em(fmt("cob_sort_key(%s, %d);\n", keyc[i], keyd[i]));
    em(fmt("cob_sort_table(&F%d, %s, %ldL);\n", it->id, cnt,
           (long)it->size));
}

static void p_sort(int merge)
{
    Fil *sd;
    int nk = 0, i;
    char *keyc[16];
    int keyd[16];
    char *usings[16], *givings[16];
    int nu = 0, ng = 0;
    char *inproc = 0, *outproc = 0;
    tp++;
    if (cur()->k == T_WORD && !find_file(cur()->s)) {
        p_sort_table();
        return;
    }
    sd = p_fil();
    while (isw("ON") || isw("ASCENDING") || isw("DESCENDING")) {
        int desc;
        acc("ON");
        desc = isw("DESCENDING");
        tp++;
        acc("KEY");
        while (cur()->k == T_WORD && !isw("ON") && !isw("ASCENDING")
               && !isw("DESCENDING") && !isw("WITH") &&
               !isw("COLLATING") && !isw("USING") && !isw("INPUT") &&
               !isw("OUTPUT") && !isw("GIVING") &&
               !isw("DUPLICATES")) {
            Opd k = p_ident();
            if (nk < 16) {
                keyc[nk] = opd_fld(&k);
                keyd[nk++] = desc;
            }
        }
    }
    if (acc("WITH")) acc("DUPLICATES");
    else acc("DUPLICATES");
    acc("IN"); acc("ORDER");
    if (acc("COLLATING")) { acc("SEQUENCE"); skip_is(); tp++; }
    if (acc("INPUT")) {
        acc("PROCEDURE");
        p_sort_proc(&inproc);
    } else if (acc("USING")) {
        while (cur()->k == T_WORD && find_file(cur()->s)) {
            Fil *u = p_fil();
            if (nu < 16) usings[nu++] = u->cname;
        }
    }
    if (acc("OUTPUT")) {
        acc("PROCEDURE");
        p_sort_proc(&outproc);
    } else if (acc("GIVING")) {
        while (cur()->k == T_WORD && find_file(cur()->s)) {
            Fil *u = p_fil();
            if (ng < 16) givings[ng++] = u->cname;
        }
    }
    (void)merge;
    em(fmt("cob_sort_begin(&%s);\n", sd->cname));
    for (i = 0; i < nk; i++)
        em(fmt("cob_sort_key(%s, %d);\n", keyc[i], keyd[i]));
    for (i = 0; i < nu; i++)
        em(fmt("E = cob_sort_using(&%s, &%s);\nif (E >= 10) "
               "cob_fnh(&%s, E);\n", sd->cname, usings[i], usings[i]));
    if (inproc) em(inproc);
    em("cob_sort_run();\n");
    if (outproc) em(outproc);
    for (i = 0; i < ng; i++)
        em(fmt("E = cob_sort_giving(&%s, &%s);\nif (E >= 10) "
               "cob_fnh(&%s, E);\n", sd->cname, givings[i],
               givings[i]));
    em("cob_sort_end();\n");
}

static void p_release(void)
{
    Opd rec;
    Fil *f;
    tp++;
    rec = p_ident();
    if (!rec.it) syn("record name expected");
    f = record_file(rec.it);
    if (acc("FROM")) {
        Opd s = p_operand();
        em(fmt("cob_move(%s, %s);\n", opd_fld(&s), opd_fld(&rec)));
    }
    em(fmt("cob_release(&%s);\n", f->cname));
}

static void p_return(void)
{
    Fil *f;
    char *into = 0;
    Hnd h;
    tp++;
    f = p_fil();
    acc("RECORD");
    if (acc("INTO")) {
        Opd t = p_ident();
        into = fmt("cob_move(%s, %s);\n", recfld(f), opd_fld(&t));
    }
    h = p_handlers(2);
    end_scope("END-RETURN");
    em(fmt("E = cob_return(&%s);\n", f->cname));
    em_fres(f, &h, 2, into);
}

static void p_sort_stmt(const char *v)
{
    if (!strcmp(v, "SORT")) p_sort(0);
    else if (!strcmp(v, "MERGE")) p_sort(1);
    else if (!strcmp(v, "RELEASE")) p_release();
    else p_return();
}
