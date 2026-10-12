/* c58_proc.c -- programs, procedure division, C emission */

static Sb out_code;             /* decls and functions, in order */
static Prog *allprogs[256];
static int nallprogs = 0;
static int opt_nomain = 0;

static void parse_class(void);
static void skip_interface(void);
static Prog *allclasses[64];
static int nallclasses = 0;
static int cur_method_fac = 0;

static void skip_comment_paras(void)
{
    while (!at_eof()) {
        if (isw("ENVIRONMENT") || isw("DATA") || isw("PROCEDURE"))
            break;
        if (isw("END") && isw_at(1, "PROGRAM")) break;
        tp++;
    }
}

static char *prog_c_name(const char *n)
{
    return mangle(n);
}

static void emit_program(Prog *pr)
{
    Sb f;
    Item *r;
    int i;
    memset(&f, 0, sizeof f);
    /* body: resolve end markers */
    {
        char *b = pr->body.s ? pr->body.s : "";
        char *p = b;
        Sb nb;
        memset(&nb, 0, sizeof nb);
        while (*p) {
            if (p[0] == '/' && p[1] == '*' && p[2] == 'E' &&
                isdigit((unsigned char)p[3])) {
                int id = atoi(p + 3);
                Para *q;
                int perf = 0;
                while (*p && !(p[0] == '*' && p[1] == '/')) p++;
                p += 2;
                for (q = pr->paras; q; q = q->next)
                    if (q->endid == id) perf = q->perfend;
                sb_s(&nb, fmt("E%d: ;\n", id));
                if (perf)
                    sb_s(&nb, fmt(
                            "if (pfsp > 0 && pfe[pfsp - 1] == %d) "
                                  "{ pfsp--; pfr = pfv[pfsp]; "
                                  "goto PFRET; }\n", id));
                continue;
            }
            sb_c(&nb, *p++);
        }
        pr->body = nb;
    }
    sb_s(&out_code, sb_str(&pr->init));
    sb_s(&f, fmt("static int PGM%d(int cob_na, CobFld **cob_a, "
                 "CobFld *cob_ret)\n{\n", pr->id_));
    for (i = 1; i <= ntmp_max; i++) sb_s(&f, fmt("    CobFld T%d;\n",
        i));
    for (i = 1; i <= ndec_max; i++) sb_s(&f, fmt("    CobDec D%d;\n",
        i));
    sb_s(&f, "    CobDec *S1, *S2, *S3, *S4;\n    int E = 0;\n");
    for (i = 1; i <= nctr; i++) sb_s(&f, fmt("    long CT%d;\n", i));
    sb_s(&f, fmt("    CobFld *CA[%d];\n", ncall + 1));
    sb_s(&f, "    int pfe[128], pfv[128], pfsp = 0, pfr = 0;\n");
    sb_s(&f, "    static int cob_first = 1;\n");
    if (pr->ismethod) {
        Prog *cp = pr->outer;
        sb_s(&f, "    CobObj *self = cob_self;\n"
                 "    CobClass *fcls = cob_fcls;\n"
                 "    unsigned char SELFB[16];\n    CobFld SELFF;\n");
        for (r = cp->recs; r; r = r->rnext)
            if (r->master == r && r->storage == (pr->isfacm ? 5 : 4))
                sb_s(&f, fmt("    unsigned char *OSV%d = B%d;\n",
                             r->id, r->id));
    }
    sb_s(&f, "    (void)E; (void)S1; (void)S2; (void)S3; (void)S4;\n"
             "    (void)pfe; (void)pfv; (void)pfsp; (void)pfr;\n"
             "    (void)cob_na; (void)cob_a; (void)cob_ret;\n");
    if (ncall >= 0) sb_s(&f, "    (void)CA;\n");
    if (pr->ismethod) {
        Prog *cp = pr->outer;
        int any = 0;
        sb_s(&f, "    (void)self; (void)fcls;\n"
                 "    SELFF.size = 8; SELFF.data = SELFB;\n"
                 "    SELFF.attr = &cob_obj_attr;\n"
                 "    cob_set_ptr(&SELFF, self);\n");
        if (pr->isfacm) {
            for (r = cp->recs; r; r = r->rnext)
                if (r->master == r && r->storage == 5)
                    sb_s(&f, fmt("    BIND%d(CLS%d.fac + %ld);\n",
                                 r->id, cp->id_, r->objoff));
        } else {
            for (r = cp->recs; r; r = r->rnext)
                if (r->master == r && r->storage == 4) any = 1;
            if (any) {
                sb_s(&f, fmt("    {\n        int isnew_;\n"
                    "        unsigned char *ob_ = cob_objblk(self, "
                    "CLS%d.idx, OSZ%d, &isnew_);\n", cp->id_,
                             cp->id_));
                for (r = cp->recs; r; r = r->rnext)
                    if (r->master == r && r->storage == 4)
                        sb_s(&f, fmt("        BIND%d(ob_ + %ld);\n",
                                     r->id, r->objoff));
                sb_s(&f, fmt("        if (isnew_) cob_run_init_z("
                             "INO%d);\n    }\n", cp->ndec));
            }
        }
    }
    /* local storage */
    for (r = pr->recs; r; r = r->rnext)
        if (r->master == r && r->storage == 2 && !r->based)
            sb_s(&f, fmt(
                "    BIND%d((unsigned char *)cob_alloc(%ld, 1));\n",
                         r->id, r->bufsize + 8));
    if (pr->initial)
        sb_s(&f, fmt("    cob_run_init(INI%d, %d);\n", pr->ndec,
                     pr->nws));
    else
        sb_s(&f, fmt("    if (cob_first) { cob_first = 0; "
                     "cob_run_init(INI%d, %d); }\n", pr->ndec,
                         pr->nws));
    if (pr->nloc)
        sb_s(&f, fmt("    cob_run_init(INL%d, %d);\n", pr->ndec,
                     pr->nloc));
    for (i = 0; i < pr->nusing; i++)
        sb_s(&f, fmt(
                "    if (cob_na > %d && cob_a[%d]) BIND%d("
                     "cob_adapt(cob_a[%d], &F%d));\n", i, i,
                     pr->using_[i]->id, i, pr->using_[i]->id));
    if (pr->retitem) {
        Item *ri = lookup(pr->retitem, 0, 0);
        if (ri && ri->storage == 1 && pr->isfunc)
            sb_s(&f, fmt("    BIND%d((unsigned char *)cob_alloc"
                         "(%ld, 1));\n", ri->master->id,
                         ri->master->bufsize + 8));
        else if (ri && ri->storage == 1)
            sb_s(&f, fmt("    if (cob_ret) BIND%d(cob_ret->data);\n"
                         "    else BIND%d((unsigned char *)cob_alloc"
                         "(%ld, 1));\n", ri->master->id,
                         ri->master->id, ri->master->bufsize + 8));
    }
    sb_s(&f, pr->body.s ? pr->body.s : "");
    sb_s(&f, "cob_exit: ;\n");
    if (pr->ismethod) {
        Prog *cp = pr->outer;
        for (r = cp->recs; r; r = r->rnext)
            if (r->master == r && r->storage == (pr->isfacm ? 5 : 4))
                sb_s(&f, fmt("    BIND%d(OSV%d);\n", r->id, r->id));
    }
    if (pr->retitem) {
        Item *ri = lookup(pr->retitem, 0, 0);
        if (ri && pr->isfunc)
            sb_s(&f, fmt("    if (cob_ret) cob_fret(cob_ret, &F%d);\n",
                         ri->id));
        else if (ri && ri->storage != 1)
            sb_s(&f, fmt("    if (cob_ret) cob_move(&F%d, cob_ret);\n",
                         ri->id));
    }
    for (r = pr->recs; r; r = r->rnext)
        if (r->master == r && r->storage == 2 && !r->based)
            sb_s(&f, fmt("    cob_free(B%d);\n", r->id));
    sb_s(&f, "    return 0;\n");
    sb_s(&f, "PFRET: switch (pfr) {\n");
    sb_s(&f, sb_str(&retcases));
    sb_s(&f, "default: break;\n}\n    return 0;\n}\n\n");
    sb_s(&out_code, sb_str(&f));
}

static void parse_procedure(void)
{
    int k;
    need("PROCEDURE"); need("DIVISION");
    P->nusing = 0;
    if (acc("USING")) {
        for (;;) {
            Item *it;
            if (acc("BY")) { acc("REFERENCE"); acc("VALUE"); continue;
                }
            if (acc("REFERENCE") || acc("VALUE")) continue;
            if (cur()->k != T_WORD || isw("RETURNING")) break;
            it = lookup(cur()->s, 0, 0);
            if (!it) syn("unknown USING item");
            tp++;
            if (P->nusing < 32) P->using_[P->nusing++] = it;
        }
    }
    if (acc("RETURNING")) P->retitem = get_name();
    if (P->isfunc && P->retitem && nufn_num < 64) {
        Item *ri = lookup(P->retitem, 0, 0);
        if (ri && (ri->ctype == CT_NUMDISP || ri->ctype == CT_BIN ||
                   ri->ctype == CT_BIN5 || ri->ctype == CT_PACKED ||
                   ri->ctype == CT_FLT || ri->ctype == CT_DBL))
            ufn_num[nufn_num++] = xdup(P->name + 3);
    }
    need_period();
    O = &P->body;
    memset(&retcases, 0, sizeof retcases);
    ntmp_max = ndec_max = 0;
    nctr = 0;
    ncall = 0;
    cur_sec = 0;
    cur_para = 0;
    nloops = 0;
    if (isw("DECLARATIVES")) {
        tp++;
        need_period();
        in_decl = 1;
        em(fmt("goto LDE%d;\n", P->id_));
    }
    sentence_start();
    for (;;) {
        Tok *t = cur();
        if (at_eof()) break;
        if (isw("END") && (isw_at(1, "PROGRAM") ||
                           isw_at(1, "FUNCTION") ||
                           isw_at(1, "METHOD"))) break;
        if ((isw("IDENTIFICATION") || isw("ID")) &&
            isw_at(1, "DIVISION")) break;
        if (isw("PROGRAM-ID") || isw("CLASS-ID") ||
            isw("FUNCTION-ID")) break;
        if (t->k == T_WORD && isw_at(1, "SECTION")) {
            char *n = t->s;
            tp += 2;
            if (cur()->k == T_NUM) tp++;
            need_period();
            begin_para(n, 1);
            sentence_start();
            continue;
        }
        if ((t->k == T_WORD || t->k == T_NUM) &&
            peek(1)->k == T_PERIOD && !(t->k == T_WORD &&
                                         is_verb(t->s))) {
            char *n = t->s;
            tp += 2;
            begin_para(n, 0);
            sentence_start();
            continue;
        }
        if (at_period()) { tp++; continue; }
        if (in_decl && isw("END") && isw_at(1, "DECLARATIVES")) {
            tp += 2;
            need_period();
            close_section();
            em(fmt("LDE%d: ;\n", P->id_));
            in_decl = 0;
            sentence_start();
            continue;
        }
        if (in_decl && isw("USE")) {
            p_use();
            need_period();
            continue;
        }
        if (!stmt_start()) syn("statement expected");
        while (stmt_start()) p_statement();
        if (at_period()) {
            tp++;
            sentence_end();
            sentence_start();
        } else if (!(at_eof() || (isw("END") &&
                  (isw_at(1, "PROGRAM") || isw_at(1, "FUNCTION") ||
                   isw_at(1, "METHOD")))))
            syn("unexpected word in statement");
    }
    close_section();
    em("goto cob_exit;\n");
    /* undefined paragraphs */
    {
        Para *q;
        for (q = P->paras; q; q = q->next)
            if (!q->defined)
                perr(srcname, q->line,
                    "undefined paragraph or section",
                     q->name);
    }
    (void)k;
}

static void parse_program(Prog *outer)
{
    Prog *pr;
    char *name;
    int idx;
    if (isw("CLASS-ID") || ((isw("IDENTIFICATION") || isw("ID")) &&
                            isw_at(3, "CLASS-ID"))) {
        parse_class();
        return;
    }
    if (isw("INTERFACE-ID") || ((isw("IDENTIFICATION") || isw("ID")) &&
                                isw_at(3, "INTERFACE-ID"))) {
        skip_interface();
        return;
    }
    pr = (Prog *)xalloc(sizeof(Prog));
    pr->outer = outer;
    pr->id_ = ++idctr;
    pr->ndec = ++idctr;
    P = pr;
    if (acc("IDENTIFICATION") || acc("ID")) {
        need("DIVISION");
        need_period();
    }
    if (acc("FUNCTION-ID")) pr->isfunc = 1;
    else if (acc("METHOD-ID")) {
        pr->ismethod = 1;
        pr->isfacm = cur_method_fac;
    } else need("PROGRAM-ID");
    acc_period();
    name = get_name();
    pr->name = upcase(name);
    if (pr->isfunc) pr->name = cat2("FN:", pr->name);
    pr->cname = prog_c_name(pr->name);
    if (acc("AS")) {
        if (cur()->k == T_STR) {
            pr->name = upcase(cur()->s);
            if (pr->isfunc) pr->name = cat2("FN:", pr->name);
            tp++;
        }
    }
    if (pr->ismethod && outer && outer->ncm < 96) {
        outer->cm[outer->ncm].name = pr->name;
        outer->cm[outer->ncm].id = pr->id_;
        outer->cm[outer->ncm++].fac = pr->isfacm;
    }
    for (;;) {
        skip_is();
        if (acc("OVERRIDE") || acc("FINAL")) { }
        else if (acc("COMMON")) pr->common = 1;
        else if (acc("INITIAL")) pr->initial = 1;
        else if (acc("RECURSIVE")) pr->recursive = 1;
        else if (acc("PROGRAM")) { }
        else break;
    }
    need_period();
    idx = nallprogs;
    if (nallprogs < 256 && !pr->ismethod) allprogs[nallprogs++] = pr;
    pr->next = 0;
    if (lastprog) lastprog->next = pr;
    else progs = pr;
    lastprog = pr;
    skip_comment_paras();
    parse_environment();
    memset(hashtab, 0, sizeof hashtab);
    /* the hash table is per program: re-add the global items of the
       enclosing programs */
    if (outer) {
        Prog *o;
        for (o = outer; o; o = o->outer) {
            Item *it;
            for (it = o->items; it; it = it->inext) {
                if (it->name && !it->filler) {
                    unsigned h = hashs(it->name);
                    it->hnext = hashtab[h];
                    hashtab[h] = it;
                }
            }
        }
    }
    synth77("TALLY", "S9(9)", U_COMP5);
    synth77("SORT-RETURN", "S9(9)", U_COMP5);
    synth77("LINAGE-COUNTER", "9(9)", U_COMP5);
    parse_data();
    close_record();
    {
        Sb d;
        memset(&d, 0, sizeof d);
        emit_data(pr, &d);
        emit_files(pr, &d);
        sb_s(&out_code, sb_str(&d));
    }
    parse_procedure();
    emit_program(pr);
    /* nested programs */
    while (!pr->ismethod && (isw("IDENTIFICATION") || isw("ID") ||
           isw("PROGRAM-ID") || isw("FUNCTION-ID")) && !(isw("END"))) {
        Prog *saved = pr;
        parse_program(pr);
        P = saved;
    }
    if (isw("END") && (isw_at(1, "PROGRAM") ||
                       isw_at(1, "FUNCTION") || isw_at(1, "METHOD"))) {
        tp += 2;
        get_name();
        acc_period();
    }
    (void)idx;
}
