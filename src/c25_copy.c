/* c25_copy.c -- COPY and REPLACE on the token stream */

typedef struct Rep Rep;
struct Rep {
    TV from, to;
    int lead, trail;
    Rep *next;
};

static int tok_eq(const Tok *a, const Tok *b)
{
    if (a->k != b->k) return 0;
    if (a->k == T_STR)
        return a->len == b->len && !memcmp(a->s, b->s, (size_t)a->len)
               && a->fl == b->fl;
    return !strcmp(a->s, b->s);
}

static int kwt(const Tok *t, const char *w)
{
    return t->k == T_WORD && !strcmp(t->s, w);
}

/* parse "== ... ==" or a single token into v; returns new index */
static long rep_operand(TV *in, long i, TV *v)
{
    memset(v, 0, sizeof *v);
    if (i < in->n && in->a[i].k == T_OP && !strcmp(in->a[i].s, "==")) {
        i++;
        while (i < in->n && !(in->a[i].k == T_OP &&
                              !strcmp(in->a[i].s, "==")))
            tv_add(v, in->a[i++]);
        if (i < in->n) i++;
    } else if (i < in->n) tv_add(v, in->a[i++]);
    return i;
}

/* parse "a BY b" pairs until stop (a period or end); returns index */
static long rep_list(TV *in, long i, Rep **list, int stop_period)
{
    Rep **tail = list;
    while (*tail) tail = &(*tail)->next;
    while (i < in->n) {
        Rep *r;
        int lead = 0, trail = 0;
        if (in->a[i].k == T_PERIOD && stop_period) break;
        if (kwt(&in->a[i], "LEADING")) { lead = 1; i++; }
        else if (kwt(&in->a[i], "TRAILING")) { trail = 1; i++; }
        r = (Rep *)xalloc(sizeof(Rep));
        r->lead = lead;
        r->trail = trail;
        i = rep_operand(in, i, &r->from);
        if (i < in->n && kwt(&in->a[i], "BY")) i++;
        i = rep_operand(in, i, &r->to);
        *tail = r;
        tail = &r->next;
    }
    return i;
}

/* try the replacements at in[i]; return tokens consumed or 0 */
static long rep_apply(Rep *list, TV *in, long i, TV *out)
{
    Rep *r;
    for (r = list; r; r = r->next) {
        long k;
        if (r->from.n == 0) continue;
        if (r->lead || r->trail) {
            Tok *t = &in->a[i];
            const char *f = r->from.a[0].s;
            long fl = (long)strlen(f), tl;
            if (t->k != T_WORD) continue;
            tl = (long)strlen(t->s);
            if (tl <= fl) continue;
            if (r->lead && strncmp(t->s, f, (size_t)fl)) continue;
            if (r->trail && strcmp(t->s + tl - fl, f)) continue;
            {
                Tok nt = *t;
                const char *to = r->to.n ? r->to.a[0].s : "";
                char *w = r->lead ? cat2(to, t->s + fl)
                                  : cat2(xdupn(t->s, tl - fl), to);
                nt.s = w;
                nt.raw = w;
                nt.rep = 1;
                tv_add(out, nt);
            }
            return 1;
        }
        if (i + r->from.n > in->n) continue;
        for (k = 0; k < r->from.n; k++)
            if (!tok_eq(&in->a[i + k], &r->from.a[k])) break;
        if (k < r->from.n) continue;
        for (k = 0; k < r->to.n; k++) {
            Tok nt = r->to.a[k];
            nt.line = in->a[i].line;
            nt.file = in->a[i].file;
            nt.rep = 1;
            if (k == 0) nt.sp = in->a[i].sp;
            tv_add(out, nt);
        }
        return r->from.n;
    }
    return 0;
}

static char *find_copy(const char *name, const char *from)
{
    static const char *ext[] = { "", ".cpy", ".CPY", ".cbl", ".CBL",
                                 ".cob", ".COB", ".copy", ".COPY", 0 };
    char *dirs[40];
    int nd = 0, i, j, v;
    char *fd = xdup(from ? from : "");
    char *sl = strrchr(fd, '/');
    char *bs = strrchr(fd, '\\');
    if (bs && (!sl || bs > sl)) sl = bs;
    if (sl) { sl[1] = 0; dirs[nd++] = fd; }
    dirs[nd++] = xdup("");
    for (i = 0; i < nincdirs && nd < 38; i++) {
        char *d = incdirs[i];
        long l = (long)strlen(d);
        if (l && d[l - 1] != '/' && d[l - 1] != '\\')
            d = cat2(d, "/");
        dirs[nd++] = d;
    }
    for (i = 0; i < nd; i++)
        for (v = 0; v < 3; v++) {
            char *nm = xdup(name);
            char *q;
            if (v == 1) for (q = nm; *q; q++)
                *q = (char)tolower((unsigned char)*q);
            if (v == 2) for (q = nm; *q; q++)
                *q = (char)toupper((unsigned char)*q);
            for (j = 0; ext[j]; j++) {
                char *path = cat3(dirs[i], nm, ext[j]);
                if (file_ok(path)) return path;
            }
        }
    return 0;
}

static void copy_expand(TV *in, TV *out, int depth);

static void copy_expand(TV *in, TV *out, int depth)
{
    long i = 0;
    if (depth > 20) die("COPY nesting too deep");
    while (i < in->n) {
        Tok *t = &in->a[i];
        if (kwt(t, "COPY") && !t->rep) {
            Rep *reps = 0;
            char *name;
            char *path, *text;
            TV sub, sub2;
            long j = i + 1;
            if (j >= in->n) perr(t->file, t->line, "bad COPY", 0);
            name = in->a[j].s;
            j++;
            if (j < in->n && (kwt(&in->a[j], "OF") ||
                              kwt(&in->a[j], "IN"))) j += 2;
            if (j < in->n && kwt(&in->a[j], "SUPPRESS")) j++;
            if (j < in->n && kwt(&in->a[j], "REPLACING")) {
                j = rep_list(in, j + 1, &reps, 1);
            }
            if (j < in->n && in->a[j].k == T_PERIOD) j++;
            path = find_copy(name, t->file);
            if (!path)
                perr(t->file, t->line, "copybook not found:", name);
            text = read_file(path);
            if (!text) perr(t->file, t->line, "cannot read", path);
            memset(&sub, 0, sizeof sub);
            memset(&sub2, 0, sizeof sub2);
            lex_text(text, path, opt_free, &sub);
            if (reps) {
                long k;
                for (k = 0; k < sub.n; ) {
                    long c = rep_apply(reps, &sub, k, &sub2);
                    if (c) k += c;
                    else tv_add(&sub2, sub.a[k++]);
                }
            } else sub2 = sub;
            copy_expand(&sub2, out, depth + 1);
            i = j;
            continue;
        }
        tv_add(out, *t);
        i++;
    }
}

static void replace_pass(TV *in, TV *out)
{
    Rep *active = 0;
    long i = 0;
    while (i < in->n) {
        Tok *t = &in->a[i];
        if (kwt(t, "REPLACE") && (i == 0 ||
                                  in->a[i - 1].k == T_PERIOD)) {
            long j = i + 1;
            active = 0;
            if (j < in->n && kwt(&in->a[j], "OFF")) j++;
            else {
                if (j < in->n && kwt(&in->a[j], "LAST")) j++;
                j = rep_list(in, j, &active, 1);
            }
            if (j < in->n && in->a[j].k == T_PERIOD) j++;
            i = j;
            continue;
        }
        if (active) {
            long c = rep_apply(active, in, i, out);
            if (c) { i += c; continue; }
        }
        tv_add(out, *t);
        i++;
    }
}

/* load a source file into a token vector with COPY/REPLACE done */
static void load_source(const char *name, TV *out)
{
    char *text = read_file(name);
    TV a, b;
    if (!text) die(cat2("cannot read ", name));
    scan_dpc(text);
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    lex_text(text, name, opt_free, &a);
    copy_expand(&a, &b, 0);
    replace_pass(&b, out);
    {
        Tok e;
        memset(&e, 0, sizeof e);
        e.k = T_EOF;
        e.s = "";
        e.line = out->n ? out->a[out->n - 1].line : 0;
        e.file = (char *)name;
        tv_add(out, e);
    }
}
