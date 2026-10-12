/* c60_oo.c -- classes, methods and INVOKE */

static void skip_interface(void)
{
    while (!at_eof()) {
        if (isw("END") && isw_at(1, "INTERFACE")) {
            tp += 2;
            if (cur()->k == T_WORD) tp++;
            acc_period();
            return;
        }
        tp++;
    }
}

static void parse_class(void)
{
    Prog *cp;
    Sb saved, meths, d;
    int i;
    char *name;
    cp = (Prog *)xalloc(sizeof(Prog));
    cp->id_ = ++idctr;
    cp->ndec = ++idctr;
    cp->iscls = 1;
    P = cp;
    if (acc("IDENTIFICATION") || acc("ID")) {
        need("DIVISION");
        need_period();
    }
    need("CLASS-ID");
    acc_period();
    name = get_name();
    cp->clsname = upcase(name);
    cp->name = cp->clsname;
    cp->cname = mangle(cp->clsname);
    if (acc("AS") && cur()->k == T_STR) {
        cp->clsname = upcase(cur()->s);
        tp++;
    }
    while (!at_period() && !at_eof()) {
        if (acc("INHERITS")) {
            acc("FROM");
            cp->base = upcase(get_name());
        } else tp++;
    }
    need_period();
    if (nallclasses < 64) allclasses[nallclasses++] = cp;
    memset(hashtab, 0, sizeof hashtab);
    saved = out_code;
    memset(&out_code, 0, sizeof out_code);
    synth77("TALLY", "S9(9)", U_COMP5);
    parse_environment();
    for (;;) {
        int fac;
        if (isw("FACTORY") || isw("OBJECT")) {
            fac = isw("FACTORY");
            tp++;
            need_period();
            if (acc("IMPLEMENTS")) {
                while (!at_period() && !at_eof()) tp++;
                need_period();
            }
            ws_storage = fac ? 5 : 4;
            parse_data();
            close_record();
            ws_storage = 0;
            if (isw("PROCEDURE")) {
                tp++;
                need("DIVISION");
                need_period();
            }
            cur_method_fac = fac;
            while (isw("METHOD-ID") || ((isw("IDENTIFICATION") ||
                                         isw("ID")) &&
                                        isw_at(3, "METHOD-ID"))) {
                parse_program(cp);
                P = cp;
            }
            need("END");
            if (!acc("FACTORY")) need("OBJECT");
            acc_period();
        } else break;
    }
    if (isw("END") && isw_at(1, "CLASS")) {
        tp += 2;
        get_name();
        acc_period();
    }
    meths = out_code;
    out_code = saved;
    memset(&d, 0, sizeof d);
    sb_s(&out_code, fmt("static CobClass CLS%d;\n", cp->id_));
    emit_data(cp, &d);
    sb_s(&out_code, sb_str(&d));
    sb_s(&out_code, sb_str(&meths));
    sb_s(&out_code, fmt("static CobMeth CMETH%d[] = {\n", cp->id_));
    for (i = 0; i < cp->ncm; i++)
        sb_s(&out_code, fmt("    { \"%s\", PGM%d, %d },\n",
                            cp->cm[i].name, cp->cm[i].id,
                            cp->cm[i].fac));
    sb_s(&out_code, "    { 0, 0, 0 }\n};\n");
    sb_s(&out_code, fmt("static CobClass CLS%d = { \"%s\", %s%s%s, "
                        "CMETH%d, %d, 0, 0, 0, 0 };\n",
                        cp->id_, cp->clsname, cp->base ? "\"" : "",
                        cp->base ? cp->base : "0",
                        cp->base ? "\"" : "", cp->id_, cp->ncm));
    sb_s(&out_code, fmt("static void CLSINIT%d(void)\n{\n", cp->id_));
    sb_s(&out_code, fmt("    CLS%d.fac = (unsigned char *)cob_alloc("
                        "FSZ%d, 1);\n", cp->id_, cp->id_));
    {
        Item *r;
        for (r = cp->recs; r; r = r->rnext)
            if (r->master == r && r->storage == 5)
                sb_s(&out_code, fmt("    BIND%d(CLS%d.fac + %ld);\n",
                                    r->id, cp->id_, r->objoff));
    }
    sb_s(&out_code, fmt("    cob_run_init_z(INF%d);\n}\n\n",
        cp->ndec));
}

/* ---- INVOKE ---- */

static void p_invoke(void)
{
    char *obj, *sup = "0";
    char *argv_[64];
    int n = 0, i;
    char *mname = 0, *mfld = 0, *ret = "0";
    Hnd h;
    tp++;
    if (isw("SELF")) {
        tp++;
        if (!P->ismethod) syn("SELF outside a method");
        obj = "self";
    } else if (isw("SUPER")) {
        tp++;
        if (!P->ismethod) syn("SUPER outside a method");
        obj = "self";
        sup = fmt("&CLS%d", P->outer->id_);
    } else {
        Item *it = cur()->k == T_WORD ? lookup(cur()->s, 0, 0) : 0;
        if (it) {
            Opd o = p_ident();
            obj = fmt("cob_getobj(%s)", opd_fld(&o));
        } else if (cur()->k == T_WORD) {
            obj = fmt("cob_class_obj(\"%s\")", cur()->s);
            tp++;
        } else if (cur()->k == T_STR) {
            obj = fmt("cob_class_obj(\"%s\")", upcase(cur()->s));
            tp++;
        } else {
            syn("INVOKE needs an object or a class");
            return;
        }
    }
    if (cur()->k == T_STR) {
        mname = fmt("\"%s\"", upcase(cur()->s));
        tp++;
    } else {
        Opd m = p_operand();
        mfld = opd_fld(&m);
    }
    if (acc("USING")) n = p_call_args(argv_);
    if (acc("RETURNING")) {
        Opd r = p_ident();
        ret = r.c;
    }
    if (n > ncall) ncall = n;
    for (i = 0; i < n; i++) em(fmt("CA[%d] = %s;\n", i, argv_[i]));
    h = p_handlers(5);
    em("cob_clr_ex();\n");
    if (mname)
        em(fmt("E = cob_invoke(%s, %s, %d, CA, %s, %s);\n", obj, mname,
               n, ret, sup));
    else
        em(fmt("E = cob_invoke_f(%s, %s, %d, CA, %s, %s);\n", obj,
               mfld, n, ret, sup));
    em_handlers(&h, "E");
    end_scope("END-INVOKE");
}
